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

#ifndef HAVE_GTK
static int running = 1;
#endif

static void update_pot_values(int p1, int p2) {
    high_time_astable = 0.693*((float)p1 + 1000.0)*.01E-6*current_srate;
    low_time_astable = 0.693*((float)p1)*.01E-6*current_srate;
    high_time_monostable = 0.693*((float)p2)*.1E-6*current_srate;

    pot1 = p1;
    pot2 = p2;
}

static void update_srate(jack_nframes_t srate) {
    float slope = current_srate == 0 ? 1.f : (float)srate/current_srate;

    current_srate = srate;

    update_pot_values(pot1, pot2);

    run_time_astable *= slope;
    run_time_monostable *= slope;
}

static int process(jack_nframes_t nframes, void *arg) {
    void* port_buf = jack_port_get_buffer(input_port, nframes);

    jack_default_audio_sample_t *out = (jack_default_audio_sample_t *)
        jack_port_get_buffer (output_port, nframes);
    jack_midi_event_t in_event;
    jack_nframes_t event_index = 0;
    jack_nframes_t event_count = jack_midi_get_event_count(port_buf);
    jack_midi_event_get(&in_event, port_buf, 0);
    for (jack_nframes_t i=0; i<nframes; i++) {
        if ((in_event.time == i) && (event_index < event_count)) {
            if ( ((*(in_event.buffer) & 0xf0)) == 0x90 ) {
                /* note on */
                unsigned char note = *(in_event.buffer + 1);

                update_pot_values(3700 * (127-note), pot2);

                printf("update_pot_values %d %d\n", pot1, pot2);
            } else if ( ((*(in_event.buffer)) & 0xf0) == 0x80 ) {
                /* note off */
            } else if ( ((*(in_event.buffer)) & 0xf0) == 0xe0) {
                /* pitch bend */
                int pitch =  (in_event.buffer[1] & 0x7f)
                          | ((in_event.buffer[2] & 0x7f) << 7);

                /* take absolute pitch */
                if (pitch >= 0x2000) {
                    pitch -= 0x2000;
                }
                pitch = 0x2000 - pitch;

                update_pot_values(pot1, pitch * 58);
            }
            event_index++;
            if(event_index < event_count)
                jack_midi_event_get(&in_event, port_buf, event_index);
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

        out[i] = (output ? 1.f : -1.f) * gain;
    }
    return 0;
}

static int srate(jack_nframes_t nframes, void *arg) {
    printf("new sample rate: %u\n", nframes);
    update_srate(nframes);

    return 0;
}

static void jack_shutdown(void *arg) {
#ifndef HAVE_GTK
    running = 0;
#endif
}

#ifdef HAVE_GTK

#define CLAMPVAL(_val,_a,_b) do {  \
    if (_val < _a) _val = _a;      \
    else if (_val > _b) _val = _b; \
} while (0)

static gboolean pot1change (GtkRange     *range,
                            GtkScrollType scroll,
                            gdouble       value,
                            gpointer      user_data) {
    int val = value;
    CLAMPVAL(val, 0, 470000);

    update_pot_values(val, pot2);

    return FALSE;
}

static gboolean pot2change (GtkRange     *range,
                            GtkScrollType scroll,
                            gdouble       value,
                            gpointer      user_data) {
    int val = value;
    CLAMPVAL(val, 0, 470000);

    update_pot_values(pot1, val);

    return FALSE;
}

static gboolean gainchange (GtkRange     *range,
                            GtkScrollType scroll,
                            gdouble       value,
                            gpointer      user_data) {
    CLAMPVAL(value, 0.0, 1.0);

    gain = value;

    return FALSE;
}

#undef CLAMPVAL

static void activate (GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *hbox;
    GtkWidget *pot1_widget;
    GtkWidget *pot2_widget;
    GtkWidget *potgain;
    GtkWidget *labelpot1;
    GtkWidget *labelpot2;
    GtkWidget *labelpotgain;

    window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "Jack Punk Console");
    gtk_window_set_default_size (GTK_WINDOW (window), 300, 200);

    hbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add (GTK_CONTAINER (window), hbox);

    pot1_widget = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                            0.0,
                                            470000,
                                            10.0);
    gtk_range_set_value(GTK_RANGE (pot1_widget), pot1);

    pot2_widget = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                            0.0,
                                            470000,
                                            10.0);
    gtk_range_set_value(GTK_RANGE (pot2_widget), pot2);

    potgain = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                        0.0,
                                        1.0,
                                        0.05);
    gtk_range_set_value(GTK_RANGE (potgain), gain);

    labelpot1 = gtk_label_new ("Astable potentiometer");
    labelpot2 = gtk_label_new ("Monostable potentiometer");
    labelpotgain = gtk_label_new ("Gain");

    gtk_container_add (GTK_CONTAINER (hbox), labelpot1);
    gtk_container_add (GTK_CONTAINER (hbox), pot1_widget);
    gtk_container_add (GTK_CONTAINER (hbox), labelpot2);
    gtk_container_add (GTK_CONTAINER (hbox), pot2_widget);
    gtk_container_add (GTK_CONTAINER (hbox), labelpotgain);
    gtk_container_add (GTK_CONTAINER (hbox), potgain);

    g_signal_connect (pot1_widget, "change-value", G_CALLBACK (pot1change), NULL);
    g_signal_connect (pot2_widget, "change-value", G_CALLBACK (pot2change), NULL);
    g_signal_connect (potgain, "change-value", G_CALLBACK (gainchange), NULL);

    gtk_widget_show_all (window);
}
#endif

#ifndef HAVE_GTK
static void sig_handler(int signum) {
    running = 0;
}

static void signal_setup(void) {
    const int signals[] = {
        SIGHUP,
        SIGINT,
        SIGPWR,
        SIGQUIT,
        SIGTERM,
        0
    };
    sigset_t waitset;

    sigemptyset(&waitset);
    for (int i = 0; 0 != signals[i]; i++) {
        struct sigaction sa;

        sigaddset(&waitset, signals[i]);

        errno = 0;
        sa.sa_flags = 0;
        sa.sa_handler = sig_handler;
        sigaction(signals[i], &sa, NULL);
    }
    pthread_sigmask(SIG_UNBLOCK, &waitset, NULL);
}
#endif

int main(int argc, char **argv) {
    printf(PACKAGE_STRING"\n");
    
    jack_client_t *client;
    if ((client = jack_client_open (PACKAGE_NAME,
                                    JackNullOption,
                                    NULL)) == 0) {
        fprintf(stderr, "Jack error: server not running?\n");
        return 1;
    }

    update_srate(jack_get_sample_rate (client));

    jack_set_process_callback (client, process, 0);
    jack_set_sample_rate_callback (client, srate, 0);
    jack_on_shutdown (client, jack_shutdown, 0);

    input_port = jack_port_register (client, "midi_in",
            JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    output_port = jack_port_register (client, "audio_out",
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (jack_activate (client)) {
        fprintf(stderr, "JAck error: cannot activate client");
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
    signal_setup();

    while (running) pause();
#endif

    jack_client_close(client);

    fprintf(stdout, "Bye.\n");

    return 0;
}
