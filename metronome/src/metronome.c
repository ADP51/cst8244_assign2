#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/netmgr.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include "./metronome.h"

metronome_t input_obj;
int srvr_coid;

void metronome_thread(metronome_t * input_obj) {
	struct sigevent event;
	struct itimerspec itime;
	timer_t timer_id;
	name_attach_t *attach;
	my_message_t msg;
	double sec_per_beat;
	double sec_per_measure;
	double interval;
	int nano;
	int rcvid;
	int table_idx;
	int status;
	char *tblp;

	if ((attach = name_attach(NULL, "metronome", 0)) == NULL) {
		perror("Cannot register metronome namespace.\n");
	}

	event.sigev_notify = SIGEV_PULSE;
	event.sigev_coid = ConnectAttach(ND_LOCAL_NODE, 0, attach->chid,
			_NTO_SIDE_CHANNEL, 0);
	event.sigev_priority = SIGEV_PULSE_PRIO_INHERIT;
	event.sigev_code = METRO_PULSE_CODE;

	timer_create(CLOCK_REALTIME, &event, &timer_id);

	table_idx = table_lookup(&input_obj);

	set_timer(&input_obj);
	start_timer(itime, timer_id, &input_obj);

	tblp = t[table_idx].pattern; // pointer to table in header with pattern

	//PHASE 2
	for (;;) {
		if ((rcivd = MsgReceive(attach->chid, &msg, sizeof(my_message_t), NULL))
				< 0) {
			perror("Error with MsgReceive\n");
			exit(EXIT_FAILURE);
		}

		if (rcvid == 0) {
			switch (msg.pulse) {
			case METRO_PULSE_CODE:
				//TODO figure out wtf we're suppose to do... god damn metronomes
				break;
			case PAUSE_PULSE_CODE:
				if (status == STARTED) {
					itime.it_value.tv_sec = msg.pulse.value.sival_int;
					timer_settime(timer_id, 0, &itime, NULL);
				}
				break;
			case SET_PULSE_CODE:
				table_idx = table_lookup(&input_obj);
				tblp = t[table_idx].pattern;
				set_timer(&input_obj);
				start_timer(itime, timer_id, &input_obj);
				printf("\n"); //new line
				break;
			case QUIT_PULSE_CODE:
				timer_delete(timer_id);
				name_detach(attach, 0);
				name_close(srvr_coid);
				exit(EXIT_SUCCESS);
				break;
			case STARTED_PULSE_CODE:
				if (status == STOPPED) {
					start_timer(itime, timer_id, &input_obj);
					status = STARTED;
				}
				break;
			case STOP_PULSE_CODE:
				// WHERE IS THIS SET TO PAUSED?
				if (status == STARTED || status == PAUSED) {
					stop_timer(&itime, timer_id);
					status = STOPPED;
				}
				break;
			}
		}
		fflush(stdout); // flush output
	}
}

int table_lookup(metronome_t * input_obj) {
	for (int i = 0; i < 8; i++) {
		if (t[i].tstop == input_obj.tstop && t[i].tsbot == input_obj->tsbot) {
			return i;
		}
	}

	return ERROR; // not found in table
}

void set_timer(metronome_t * input) {
	input->timer.bps = (double) 60 / input_obj->bpm;
	input->timer.measure = input_obj->tstop * sec_per_beat;
	input->timer.interval = sec_per_measure / input_obj->tsbot;
	input->timer.nano = interval.floor() * 1e+9;
}

void start_timer(struct itimerspec *itime, timer_t timer_id,
		metronome_t* input_obj) {
	//set time
	itime->it_value.tv_sec = 1;
	itime->it_value.tv_nsec = 0;
	itime->it_interval.tv_sec = input_obj->timer.interval;
	itime->it_interval.tv_nsec = input_obj->timer.nano;
	timer_settime(timer_id, 0, itime, NULL);
}

void stop_timer(struct itimerspec *itime, timer_t timer_id) {
	itime->it_value.tv_sec = 0;
	itime->it_value.tv_nsec = 0;
	itime->it_interval.tv_sec = 0;
	itime->it_interval.tv_nsec = 0;
	timer_settime(timer_id, 0, itime, NULL);
}

int main(void) {
	puts("Hello World!!!"); /* prints Hello World!!! */
	return EXIT_SUCCESS;
}
