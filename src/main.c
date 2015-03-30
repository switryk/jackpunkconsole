/*
    jackpunkconsole

    Copyright (C) 2015 Stéphane Witryk <s.witryk@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#ifndef HAVE_GTK
#   define _POSIX_SOURCE
#   include <pthread.h>
#   include <signal.h>
#else
#   include <gtk/gtk.h>
#endif

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <jack/jack.h>
#include <jack/midiport.h>

#include "midi_notes.h"

static jack_port_t *input_port;
static jack_port_t *output_port;

static int pot1 = 100000;
static int pot2 = 80000;

static jack_nframes_t current_srate = 0;

static int high_time_astable;
static int  low_time_astable;
static int high_time_monostable;

static int run_time_astable = 0;
static int run_time_monostable = 0;

static int output = 1;

static float gain = 1.0f;

static unsigned long midi_notes_played[] = { 0, 0, 0, 0 };

#ifndef HAVE_GTK
static int running = 1;
#else
static int mouse_pressed = 0;
#endif

#define NOTE_ON(_note) do {                          \
    if (_note < 0);                                  \
    else if (_note < 32)                             \
        midi_notes_played[0] |= 1 << (_note);        \
    else if (_note < 64)                             \
        midi_notes_played[1] |= 1 << ((_note) - 32); \
    else if (_note < 96)                             \
        midi_notes_played[2] |= 1 << ((_note) - 64); \
    else if (_note < 128)                            \
        midi_notes_played[3] |= 1 << ((_note) - 96); \
} while (0)

#define NOTE_OFF(_note) do {                            \
    if (_note < 0);                                     \
    else if (_note < 32)                                \
        midi_notes_played[0] &= ~(1 << (_note));        \
    else if (_note < 64)                                \
        midi_notes_played[1] &= ~(1 << ((_note) - 32)); \
    else if (_note < 96)                                \
        midi_notes_played[2] &= ~(1 << ((_note) - 64)); \
    else if (_note < 128)                               \
        midi_notes_played[3] &= ~(1 << ((_note) - 96)); \
} while (0)

#define GET_NOTE_AUX(_note) do {   \
    while (! (ulnote & 1)) {       \
        ulnote >>= 1;              \
        _note++;                   \
    }                              \
} while(0)

#define GET_NOTE(_note) do {                        \
    unsigned long ulnote;                           \
    if        (midi_notes_played[0]) {              \
        ulnote = midi_notes_played[0];              \
        _note = 0;                                  \
        GET_NOTE_AUX(_note);                        \
    } else if (midi_notes_played[1]) {              \
        ulnote = midi_notes_played[1];              \
        _note = 32;                                 \
        GET_NOTE_AUX(_note);                        \
    } else if (midi_notes_played[2]) {              \
        ulnote = midi_notes_played[2];              \
        _note = 64;                                 \
        GET_NOTE_AUX(_note);                        \
    } else if (midi_notes_played[3]) {              \
        ulnote = midi_notes_played[3];              \
        _note = 96;                                 \
        GET_NOTE_AUX(_note);                        \
    } else {                                        \
        _note = -1;                                 \
    }                                               \
} while (0)

#define IS_NOTE_ON() (midi_notes_played[0] \
    || midi_notes_played[1]                \
    || midi_notes_played[2]                \
    || midi_notes_played[3])

#define IS_NOTE_OFF() (! IS_NOTE_ON())

static void update_pot_values (int p1, int p2) {
    high_time_astable = 0.693*((float)p1 + 1000.0)*.01E-6*current_srate;
    low_time_astable = 0.693*((float)p1)*.01E-6*current_srate;
    high_time_monostable = 0.693*((float)p2)*.1E-6*current_srate;

    pot1 = p1;
    pot2 = p2;
}

static void update_srate (jack_nframes_t srate) {
    float slope = current_srate == 0 ? 1.f : (float)srate/current_srate;

    current_srate = srate;

    update_pot_values (pot1, pot2);

    run_time_astable *= slope;
    run_time_monostable *= slope;
}

static int process (jack_nframes_t nframes, void *arg) {
    void* port_buf = jack_port_get_buffer (input_port, nframes);

    jack_default_audio_sample_t *out = (jack_default_audio_sample_t *)
        jack_port_get_buffer (output_port, nframes);

    jack_midi_event_t in_event;
    jack_nframes_t event_index = 0;
    jack_nframes_t event_count = jack_midi_get_event_count (port_buf);

    jack_midi_event_get (&in_event, port_buf, 0);

    for (jack_nframes_t i = 0; i < nframes; ++i) {
        if ((in_event.time == i) && (event_index < event_count)) {
            if ( ((*(in_event.buffer) & 0xf0)) == 0x90 ) {
                /* note on */
                int note = *(in_event.buffer + 1);

                NOTE_ON(note);

                update_pot_values (midi_notes[note].pot1,
                                   midi_notes[note].pot2);
            } else if ( ((*(in_event.buffer)) & 0xf0) == 0x80 ) {
                /* note off */
                int note = *(in_event.buffer + 1);

                NOTE_OFF(note);

                /* any note still pressed? */
                GET_NOTE(note);
                if (-1 != note)
                    update_pot_values (midi_notes[note].pot1,
                                       midi_notes[note].pot2);
            } else if ( ((*(in_event.buffer)) & 0xf0) == 0xe0) {
                /* pitch bend */
                int pitch = (in_event.buffer[1] & 0x7f)
                         | ((in_event.buffer[2] & 0x7f) << 7);
                int note;

                /* normalize */
                if (pitch >= 0x2000)
                    pitch -= 0x2000;
                else
                    pitch = -(0x2000-pitch);

                GET_NOTE(note);
                double center = midi_notes[note].pot2;
                if (pitch >= 0)
                    update_pot_values(pot1, center - (double)pitch/0x2000 * center);
                else
                    update_pot_values(pot1, center - (double)pitch/0x2000 * (470000 - center));
            }

            ++event_index;
            if (event_index < event_count)
                jack_midi_event_get (&in_event, port_buf, event_index);
        }

        if (run_time_astable >= high_time_astable + low_time_astable)
            run_time_astable = 0;

        if (run_time_monostable >= high_time_monostable) {
            output = 0;
            if (run_time_astable == high_time_astable) {
                run_time_monostable = 0;
                output = 1;
            }
        }

        run_time_astable ++;
        run_time_monostable ++;

        out[i] = (
#ifdef HAVE_GTK
                  mouse_pressed
#else
                  0
#endif
                                 || IS_NOTE_ON())
            ? (output ? 1.f : -1.f) * gain
            : 0.0f;
    }
    return 0;
}

static int srate (jack_nframes_t nframes, void *arg) {
    update_srate (nframes);

    return 0;
}

static void jack_shutdown (void *arg) {
#ifndef HAVE_GTK
    running = 0;
#endif
}

#ifdef HAVE_GTK

#define CLAMPVAL(_val,_a,_b) \
    ((_val) < (_a) ? (_a) : ((_val) > (_b) ? (_b) : (_val)))

static struct pot_widgets {
    GtkWidget *pot1;
    GtkWidget *pot2;
    GtkWidget *twodslider;
    GtkWidget *potgain;
} pw;

static gboolean potchange (GtkRange *range,
                           GtkScrollType scroll,
                           gdouble value,
                           gpointer user_data) {
    struct pot_widgets *pw = (struct pot_widgets *)user_data;

    if ((GtkWidget *)range == pw->pot1)
        update_pot_values (CLAMPVAL(value, 0, 470000), pot2);
    else
        update_pot_values (pot1, CLAMPVAL(value, 0, 470000));

    gtk_widget_queue_draw (pw->twodslider);

    return FALSE;
}

static gboolean gainchange (GtkRange     *range,
                            GtkScrollType scroll,
                            gdouble       value,
                            gpointer      user_data) {
    gain = CLAMPVAL(value, 0.0, 1.0);

    return FALSE;
}

static gboolean draw_callback (GtkWidget *widget,
                               cairo_t *cr,
                               gpointer data) {
    guint width, height;
    GdkRGBA color;

    width = gtk_widget_get_allocated_width (widget);
    height = gtk_widget_get_allocated_height (widget);

    cairo_arc (cr,
               ((double)pot1/470000.0)*width,
               height-((double)pot2/470000.0)*height,
               5,
               0, 2 * G_PI);

    gtk_style_context_get_color (gtk_widget_get_style_context (widget),
                                 0,
                                 &color);
    gdk_cairo_set_source_rgba (cr, &color);

    cairo_fill (cr);

    return FALSE;
}

static gboolean mouse_button_event (GtkWidget *widget,
                                    GdkEvent  *event,
                                    gpointer   user_data) {
    struct pot_widgets *pw = (struct pot_widgets *)user_data;
    GdkEventButton *e = (GdkEventButton *)event;

    if (pw->twodslider == widget) {
        guint width, height;

        width = gtk_widget_get_allocated_width (widget);
        height = gtk_widget_get_allocated_height (widget);

        update_pot_values (CLAMPVAL(e->x/width,0.0,1.0) * 470000.0,
                           (1.0 - CLAMPVAL(e->y/height,0.0,1.0))*470000.0);


        gtk_range_set_value (GTK_RANGE (pw->pot1), pot1);
        gtk_range_set_value (GTK_RANGE (pw->pot2), pot2);

        gtk_widget_queue_draw (widget);
    }

    if (e->type == GDK_BUTTON_PRESS) {
        mouse_pressed = 1;
    } else if (e->type == GDK_BUTTON_RELEASE) {
        mouse_pressed = 0;
    }

    return FALSE;
}

static gboolean mouse_motion_event (GtkWidget *widget,
                                    GdkEvent  *event,
                                    gpointer   user_data) {
    struct pot_widgets *pw = (struct pot_widgets *)user_data;
    GdkEventMotion *e = (GdkEventMotion *)event;
    int x, y;
    guint width, height;
    GdkModifierType state;

    width = gtk_widget_get_allocated_width (widget);
    height = gtk_widget_get_allocated_height (widget);

    if (e->is_hint) {
        return FALSE;
    } else {
        x = e->x;
        y = e->y;
        state = e->state;
    }

    if (state & GDK_BUTTON1_MASK) {
        update_pot_values (CLAMPVAL((double)x/width,0.0,1.0) * 470000.0,
                           (1.0 - CLAMPVAL((double)y/height,0.0,1.0))
                                   *470000.0);
        gtk_range_set_value (GTK_RANGE (pw->pot1), pot1);
        gtk_range_set_value (GTK_RANGE (pw->pot2), pot2);
        gtk_widget_queue_draw (widget);
    }

    return FALSE;
}

#undef CLAMPVAL

static void activate (GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *hbox;
    GtkWidget *pot1_widget;
    GtkWidget *pot2_widget;
    GtkWidget *twodslider;
    GtkWidget *potgain;
    GtkWidget *labelpot1;
    GtkWidget *labelpot2;
    GtkWidget *labelpotgain;

    window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "Jack Punk Console");
    gtk_window_set_default_size (GTK_WINDOW (window), 300, 200);

    hbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add (GTK_CONTAINER (window), hbox);

    twodslider = gtk_drawing_area_new ();
    gtk_widget_set_size_request (twodslider, 300, 300);
    gtk_widget_add_events (twodslider,
                             GDK_BUTTON_PRESS_MASK
                           | GDK_BUTTON_RELEASE_MASK
                           | GDK_POINTER_MOTION_MASK);

    pot1_widget = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                            0.0,
                                            470000,
                                            10.0);
    gtk_range_set_value (GTK_RANGE (pot1_widget), pot1);
    gtk_widget_add_events (pot1_widget,
                             GDK_BUTTON_PRESS_MASK
                           | GDK_BUTTON_RELEASE_MASK);

    pot2_widget = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                            0.0,
                                            470000,
                                            10.0);
    gtk_range_set_value (GTK_RANGE (pot2_widget), pot2);
    gtk_widget_add_events (pot2_widget,
                             GDK_BUTTON_PRESS_MASK
                           | GDK_BUTTON_RELEASE_MASK);

    potgain = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                        0.0,
                                        1.0,
                                        0.05);
    gtk_range_set_value (GTK_RANGE (potgain), gain);

    labelpot1 = gtk_label_new ("Astable potentiometer");
    labelpot2 = gtk_label_new ("Monostable potentiometer");
    labelpotgain = gtk_label_new ("Gain");

    gtk_container_add (GTK_CONTAINER (hbox), twodslider);
    gtk_container_add (GTK_CONTAINER (hbox), labelpot1);
    gtk_container_add (GTK_CONTAINER (hbox), pot1_widget);
    gtk_container_add (GTK_CONTAINER (hbox), labelpot2);
    gtk_container_add (GTK_CONTAINER (hbox), pot2_widget);
    gtk_container_add (GTK_CONTAINER (hbox), labelpotgain);
    gtk_container_add (GTK_CONTAINER (hbox), potgain);

    pw.pot1 = pot1_widget;
    pw.pot2 = pot2_widget;
    pw.potgain = potgain;
    pw.twodslider = twodslider;

    g_signal_connect (G_OBJECT (twodslider), "draw",
                      G_CALLBACK (draw_callback), (gpointer)&pw);
    g_signal_connect (G_OBJECT (twodslider), "button-press-event",
                      G_CALLBACK (mouse_button_event), (gpointer)&pw);
    g_signal_connect (G_OBJECT (twodslider), "button-release-event",
                      G_CALLBACK (mouse_button_event), (gpointer)&pw);
    g_signal_connect (G_OBJECT (twodslider), "motion_notify_event",
                      G_CALLBACK (mouse_motion_event), (gpointer)&pw);

    g_signal_connect (pot1_widget, "change-value",
                      G_CALLBACK (potchange), (gpointer)&pw);
    g_signal_connect (G_OBJECT (pot1_widget), "button-press-event",
                      G_CALLBACK (mouse_button_event), (gpointer)&pw);
    g_signal_connect (G_OBJECT (pot1_widget), "button-release-event",
                      G_CALLBACK (mouse_button_event), (gpointer)&pw);

    g_signal_connect (pot2_widget, "change-value",
                      G_CALLBACK (potchange), (gpointer)&pw);
    g_signal_connect (G_OBJECT (pot2_widget), "button-press-event",
                      G_CALLBACK (mouse_button_event), (gpointer)&pw);
    g_signal_connect (G_OBJECT (pot2_widget), "button-release-event",
                      G_CALLBACK (mouse_button_event), (gpointer)&pw);

    g_signal_connect (potgain, "change-value",
                      G_CALLBACK (gainchange), NULL);

    gtk_widget_show_all (window);
}
#endif

#ifndef HAVE_GTK
static void sig_handler (int signum) {
    running = 0;
}

static void signal_setup (void) {
    const int signals[] = {
        SIGHUP,
        SIGINT,
        SIGPWR,
        SIGQUIT,
        SIGTERM,
        0
    };
    sigset_t waitset;

    sigemptyset (&waitset);
    for (int i = 0; 0 != signals[i]; i++) {
        struct sigaction sa;

        sigaddset (&waitset, signals[i]);

        errno = 0;
        sa.sa_flags = 0;
        sa.sa_handler = sig_handler;
        sigaction (signals[i], &sa, NULL);
    }
    pthread_sigmask (SIG_UNBLOCK, &waitset, NULL);
}
#endif

int main (int argc, char **argv) {
    printf (PACKAGE_STRING"\n");

    jack_client_t *client;
    if ((client = jack_client_open (PACKAGE_NAME,
                                    JackNullOption,
                                    NULL)) == 0) {
        fprintf (stderr, "Jack error: server not running?\n");
        return 1;
    }

    update_srate (jack_get_sample_rate (client));

    jack_set_process_callback (client, process, 0);
    jack_set_sample_rate_callback (client, srate, 0);
    jack_on_shutdown (client, jack_shutdown, 0);

    input_port = jack_port_register (client, "midi_in",
            JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    output_port = jack_port_register (client, "audio_out",
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (jack_activate (client)) {
        fprintf (stderr, "Jack error: cannot activate client");
        return 1;
    }

#ifdef HAVE_GTK
    GtkApplication *app;
    app = gtk_application_new ("be.witryk.jackpunkconsole",
                               G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);
#else
    signal_setup ();

    while (running) pause ();
#endif

    jack_client_close (client);

    fprintf (stdout, "Bye.\n");

    return 0;
}
