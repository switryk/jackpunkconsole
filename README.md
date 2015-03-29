# jackpunkconsole

## Description

jackpunkconsole is a sound synthesizer that emulates the sound of an
[Atari punk console](http://en.wikipedia.org/wiki/Atari_Punk_Console).
It is written in C and uses the
[Jack Audio Connection Kit](http://jackaudio.org/). It has a Gtk3
interface and a midi connection through Jack, so it is possible to play
sounds with a midi device.

As it uses Jack for audio, it is possible to redirect the sound in a
FX processors, recorders, or anything from fun to useful.

## Compilation

You need libjack and optionally libgtk-3 development packages installed
on your system in order to compile jackpunkconsole.

In order to compile and install it on your system, run the 
classical `./configure`, `make` and `make install`.

## Usage

Run jackpunkconsole without arguments. Initially, it will output sound
with default parameters but you need to make all the Jack connectivity
to actually hear something.

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
top by clicking and dragging (you will see the corresponding slider
change accordingly).

### Midi

TODO (needs to be improved...)

## Sample

+ [Raw sound](http://witryk.be/jpc-sample01.ogg)

## TODO

 + Improve midi playability (play correct notes)
