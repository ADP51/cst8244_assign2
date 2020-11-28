/*
 * metronome.h
 *
 *  Created on: Nov 27, 2020
 *      Author: Andrew Palmer
 */

#ifndef METRONOME_H_
#define METRONOME_H_

#define METRO_PULSE_CODE _PULSE_CODE_MINAVAIL
#define PAUSE_PULSE_CODE (_PULSE_CODE_MINAVAIL +1)
#define START_PULSE_CODE (_PULSE_CODE_MINAVAIL +2)
#define STOP_PULSE_CODE  (_PULSE_CODE_MINAVAIL +3)
#define QUIT_PULSE_CODE  (_PULSE_CODE_MINAVAIL +4)
#define SET_PULSE_CODE   (_PULSE_CODE_MINAVAIL +5)

#define STARTED 0
#define STOPPED 1
#define PAUSED 2

typedef union {
	struct _pulse pulse;
	char msg[255];
} my_message_t;

struct Timer {
	double bps;
	double bpmeasure;
	double interval;
	double nano;
} timer_t;

struct metronome_t {
	timer_t timer;
	int bpm;
	int tstop;
	int tsbot;
};

struct DataTableRow {
	int tstop;
	int tsbot;
	int perbeat;
	char pattern[16];
};

DataTableRow t[] = { { 2, 4, 4, "|1&2&" }, { 3, 4, 6, "|1&2&3&" }, { 4, 4, 8,
		"|1&2&3&4&" }, { 5, 4, 10, "|1&2&3&4-5-" }, { 3, 8, 6, "|1-2-3-" }, { 6,
		8, 6, "|1&a2&a" }, { 9, 8, 9, "|1&a2&a3&a" }, { 12, 8, 12,
		"|1&a2&a3&a4&a" } };



#endif /* METRONOME_H_ */
