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

#ifndef JPC_MIDI_NOTES_H_
#define JPC_MIDI_NOTES_H_

extern struct midi_notes_t {
    unsigned int note;
    double       freq;
    double       pot1;
    double       pot2;
} midi_notes[];

#endif
