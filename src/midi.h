#ifndef OMNISCENT_MIDI_H
#define OMNISCENT_MIDI_H

#include <stdint.h>

#define MIDI_MAX_CH    4
#define MIDI_DATA_MAX  2048

void midi_init(void);
void midi_start(void);
void midi_tick(void);
void midi_stop(void);

#endif
