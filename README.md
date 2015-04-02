# jackpunkconsole

## Description

jackpunkconsole is a sound synthesizer that emulates an
[Atari punk console](http://en.wikipedia.org/wiki/Atari_Punk_Console).
It is written in C and uses the
[Jack Audio Connection Kit](http://jackaudio.org/). It has a Gtk3
interface and a midi connection through Jack, so it is possible to play
sounds with a midi device.

As it uses Jack for audio, it is possible to redirect the sound in a
FX processors, recorders, or anything from fun to useful.

## Downloads

+ [Source code on GitHub](https://github.com/switryk/jackpunkconsole)

## Compilation

You need libjack and optionally libgtk-3 development packages installed
on your system in order to compile jackpunkconsole.

Run `autoreconf -i` in order to setup a configure script, then
run the classical `./configure`, `make` and `make install`.

## Usage

Run jackpunkconsole without arguments. You need to make all the
Jack connectivity to actually hear something.

Typically you will connect the jackpunkconsole `audio_out` to your system
`playback_1` and `playback_2`. If you use
[qjackctl](http://qjackctl.sourceforge.net/), here is how it looks like:
![qjackctl setup](http://witryk.be/jpc-screen02.png "qjackctl setup")

Of course, more complex connections are possible.

### GUI

![Main window](http://witryk.be/jpc-screen01.png "Main window")

There are basically three parameters:

1. the astable potentiometer (0-470k)
2. the monostable potentiometer (0-470k)
3. the gain (0-1)

The third one can be controlled by the slider at the bottom. The two
first can be controlled either by the sliders, or by the white area at
top by clicking and dragging (you will see the corresponding sliders
change accordingly).

In GUI mode, sound will output only when sliding one of the two
potentiometers or click and moving the cursor in the white area. In this
mode, it's not really possible to play precise notes.

### Midi

In midi mode, the user is able to play notes (A0 to G9). The pitch wheel
can be used to change the monostable potentiometer value when a note is
played.

To be exact, jackpunkconsole has
[precomputed potentiometers values](src/midi_notes.c)
for each midi note. When the user plays a note on its keyboard, those
values are used. The pitch wheel will only affect the monostable
potentiometer value: going up will increase it toward 470k, going down
will decrease it toward 0.

jackpunkconsole uses a Jack midi interface. In order to use a midi
keyboard, you have to use a midi bridge tool such as
[a2jmidid](http://home.gna.org/a2jmidid/) with this
command: `a2jmidid -e`.

## Sample

+ [Raw sound using the GUI](http://witryk.be/jpc-sample01.ogg)
+ Raw sound using a midi keyboard (TODO)
+ Effect processed sound (TODO)
