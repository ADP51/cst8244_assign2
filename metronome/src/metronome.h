/*
 * metronome.h
 *
 *  Created on: Nov 27, 2020
 *      Author: Andrew Palmer
 */

#ifndef METRONOME_H_
#define METRONOME_H_

//Pulse codes
#define METRO_PULSE_CODE _PULSE_CODE_MINAVAIL
#define PAUSE_PULSE_CODE (_PULSE_CODE_MINAVAIL +1)
#define START_PULSE_CODE (_PULSE_CODE_MINAVAIL +2)
#define STOP_PULSE_CODE  (_PULSE_CODE_MINAVAIL +3)
#define QUIT_PULSE_CODE  (_PULSE_CODE_MINAVAIL +4)
#define SET_PULSE_CODE   (_PULSE_CODE_MINAVAIL +5)

#define STARTED 0
#define STOPPED 1
#define PAUSED 2

//Setting up devices
#define ERROR -1
#define METRONOME 0
#define HELP 1
#define NumDevices 2

char *devnames[NumDevices] = {
	"/dev/local/metronome",
	"/dev/local/metronome-help"
};

typedef struct ioattr_t {
	iofunc_attr_t attr;
	int device;
} ioattr_t;

typedef struct metocb_s {
	iofunc_ocb_t ocb;
	char buffer[255];
} metocb_t;


typedef union {
	struct _pulse pulse;
	char msg[255];
} my_message_t;

struct Timer_attr {
	double length;
	double bpmeasure;
	double interval;
	int nano;
} typedef timer_attr;

struct metronome_t {
	timer_attr timer;
	int bpm;
	int tstop;
	int tsbot;
} typedef metronome_t;

struct DataTableRow {
	int tstop;
	int tsbot;
	int perbeat;
	char pattern[16];
} typedef DataTableRow;

struct DataTableRow t[] = { { 2, 4, 4, "|1&2&" }, { 3, 4, 6, "|1&2&3&" }, { 4, 4, 8,
		"|1&2&3&4&" }, { 5, 4, 10, "|1&2&3&4-5-" }, { 3, 8, 6, "|1-2-3-" }, { 6,
		8, 6, "|1&a2&a" }, { 9, 8, 9, "|1&a2&a3&a" }, { 12, 8, 12,
		"|1&a2&a3&a4&a" } };

//function declarations
void metronome_thread(void * input_obj);
int table_lookup(metronome_t *input_obj);
void set_timer(metronome_t * input);
void start_timer(struct itimerspec *itime, timer_t timer_id,
		metronome_t* input_obj);
void stop_timer(struct itimerspec *itime, timer_t timer_id);
int io_read(resmgr_context_t *ctp, io_read_t *msg, metocb_t *ocb);
int io_write(resmgr_context_t *ctp, io_write_t *msg, RESMGR_OCB_T *ocb);
int io_open(resmgr_context_t *ctp, io_open_t *msg, RESMGR_HANDLE_T *handle,
		void *extra);
metocb_t * metocb_calloc(resmgr_context_t *ctp, ioattr_t *mattr);
void metocb_free(metocb_t *metocb);

#endif /* METRONOME_H_ */
