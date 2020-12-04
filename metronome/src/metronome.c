#include "metronome.h"

metronome_t input_obj;
name_attach_t *attach;
int srvr_coid;
char data[512];

int main(int argc, char *argv[]) {
	dispatch_t *dpp;
	resmgr_io_funcs_t io_funcs;
	resmgr_connect_funcs_t connect_funcs;
	ioattr_t ioattrs[NumDevices];
	dispatch_context_t *ctp;
	pthread_attr_t thread_attr;

	if (argc != 4) {
		perror("Incorrect number of command line args\n");
		return EXIT_FAILURE;
	}

	input_obj.bpm = atoi(argv[1]);
	input_obj.tstop = atoi(argv[2]);
	input_obj.tsbot = atoi(argv[3]);

	iofunc_funcs_t metocb_funcs = {_IOFUNC_NFUNCS, metocb_calloc, metocb_free };

	iofunc_mount_t metocb_mount = { 0, 0, 0, 0, &metocb_funcs };

	if ((dpp = dispatch_create()) == NULL) {
		perror("Unable to allocate dispatch handle");
		return EXIT_FAILURE;
	}

	iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS, &io_funcs);
	connect_funcs.open = io_open;
	io_funcs.read = io_read;
	io_funcs.write = io_write;

	for (int i = 0; i < NumDevices; i++) {
		iofunc_attr_init(&ioattrs[i].attr, S_IFCHR | 0666, NULL, NULL);
		ioattrs[i].device = i;
		ioattrs[i].attr.mount = &metocb_mount;
		resmgr_attach(dpp, NULL, devnames[i], _FTYPE_ANY, 0, &connect_funcs, &io_funcs, &ioattrs[i]);
	}

	ctp = dispatch_context_alloc(dpp);

	//spin up thread
	pthread_attr_init(&thread_attr);
	pthread_create(NULL, &thread_attr, &metronome_thread, &input_obj);

	while(1) {
		ctp = dispatch_block(ctp);
		dispatch_handler(ctp);
	}

	pthread_attr_destroy(&thread_attr);
	name_detach(attach, 0);
	name_close(srvr_coid);
	return EXIT_SUCCESS;
}

void *metronome_thread() {
	struct sigevent event;
	struct itimerspec itime;
	timer_t timer_id;
	my_message_t msg;
	int rcvid;
	int table_idx;
	int status;
	char *tblp;

	if ((attach = name_attach(NULL, "metronome", 0)) == NULL) {
		perror("Error attaching to namespace");
		exit(EXIT_FAILURE);
	}

	event.sigev_notify = SIGEV_PULSE;
	event.sigev_coid = ConnectAttach(ND_LOCAL_NODE, 0, attach->chid, _NTO_SIDE_CHANNEL, 0);
	event.sigev_priority = SIGEV_PULSE_PRIO_INHERIT;
	event.sigev_code = MET_PULSE;

	timer_create(CLOCK_REALTIME, &event, &timer_id);

	table_idx = table_lookup(&input_obj);

	if(table_idx == -1){ // check if measure exists in table
		perror("\nStopped\nPlease set a valid measure."); // if not, set status to stopped
		stop_timer(&itime, timer_id);
		status = STOPPED;
	} else { // if exists, set and start timer
		set_timer(&input_obj);
		start_timer(&itime, timer_id, &input_obj);
	}

	tblp = t[table_idx].pattern;

	for (;;) {
		if ((rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL)) == -1) {
			perror("Error receiving message.");
			return (int*)EXIT_FAILURE;
		}

		if (rcvid == 0) {
			switch (msg.pulse.code) {
			case MET_PULSE:
				if (*tblp == '|') { // if start of pattern
					printf("%.2s", tblp); // print first 2
					tblp += 2; // increment pointer
				} else if (*tblp == '\0') {
					printf("\n"); // print newline
					tblp = t[table_idx].pattern; // reset pattern
				} else { // mid-measure print and increment
					printf("%c", *tblp);
					tblp++;
				}
				break;
			case START_PULSE:
				if (status == STOPPED) {
					start_timer(&itime, timer_id, &input_obj);
					status = STARTED;
				}
				break;
			case STOP_PULSE:
				if (status == STARTED || status == PAUSED){
					stop_timer(&itime, timer_id);
					status = STOPPED;
				}
				break;
			case PAUSE_PULSE:
				if (status == STARTED) {
					//set delay in timer
					itime.it_value.tv_sec = msg.pulse.value.sival_int;
					timer_settime(timer_id, 0, &itime, NULL);
				}
				break;
			case SET_PULSE: // set a new pattern
				table_idx = table_lookup(&input_obj);
				if(table_idx == -1){ // check if measure exists in table
					perror("\nStopped\nPlease set a valid measure.");
					stop_timer(&itime, timer_id);
					status = STOPPED;
					break;
				}
				tblp = t[table_idx].pattern;
				set_timer(&input_obj);
				start_timer(&itime, timer_id, &input_obj);
				status = STARTED;
				printf("\n"); //new line
				break;
			case QUIT_PULSE:
				timer_delete(timer_id);
				name_detach(attach, 0);
				name_close(srvr_coid);
				exit(EXIT_SUCCESS);
			}
		}
		fflush(stdout); // flush output
	}
	return NULL;
}

int table_lookup(metronome_t *input_obj) {
	for (int i = 0; i < 8; i++) {
		if (t[i].tstop == input_obj->tstop && t[i].tsbot == input_obj->tsbot) {
			return i;
		}
	}

	return -1;
}

void set_timer(metronome_t *input_obj) {
	input_obj->timer.length = (double) 60 / input_obj->bpm;
	input_obj->timer.measure = input_obj->timer.length * 2;
	input_obj->timer.interval = input_obj->timer.measure / input_obj->tsbot;
	input_obj->timer.nano = floor(input_obj->timer.interval * 1e+9);
}

void start_timer(struct itimerspec *itime, timer_t timer_id, metronome_t *input_obj) {
	itime->it_value.tv_sec = 1;
	itime->it_value.tv_nsec = 0;
	itime->it_interval.tv_sec = input_obj->timer.interval;
	itime->it_interval.tv_nsec = input_obj->timer.nano;
	timer_settime(timer_id, 0, itime, NULL);
}

void stop_timer(struct itimerspec *itime, timer_t timer_id) {
	itime->it_value.tv_sec = 0;
	timer_settime(timer_id, 0, itime, NULL);
}

int io_read(resmgr_context_t *ctp, io_read_t *msg, metocb_t *metocb) {
	int nb;
	int i = table_lookup(&input_obj);

	if (metocb->ocb.attr->device == METRONOME) {
		sprintf(data,
				"[metronome: %d beats/min, time signature: %d/%d, sec-per-interval: %.2f, nanoSecs: %.0f]\n",
				input_obj.bpm, t[i].tstop, t[i].tsbot, input_obj.timer.interval, input_obj.timer.nano
		);
	} else if (metocb->ocb.attr->device == HELP){
			sprintf(data,
					"Metronome Resource Manager (ResMgr)\n"
					"\nUsage: metronome <bpm> <ts-top> <ts-bottom>\n\nAPI:\n"
					"pause[1-9]                     -pause the metronome for 1-9 seconds\n"
					"quit                           -quit the metronome\n"
					"set <bpm> <ts-top> <ts-bottom> -set the metronome to <bpm> ts-top/ts-bottom\n"
					"start                          -start the metronome from stopped state\n"
					"stop                           -stop the metronome; use 'start' to resume\n"
			);
	}
	nb = strlen(data);

	//test to see if we have already sent the whole message.
	if (metocb->ocb.offset == nb)
		return 0;

	//We will return which ever is smaller the size of our data or the size of the buffer
	nb = min(nb, msg->i.nbytes);

	//Set the number of bytes we will return
	_IO_SET_READ_NBYTES(ctp, nb);

	//Copy data into reply buffer.
	SETIOV(ctp->iov, data, nb);

	//update offset into our data used to determine start position for next read.
	metocb->ocb.offset += nb;

	//If we are going to send any bytes update the access time for this resource.
	if (nb > 0)
		metocb->ocb.flags |= IOFUNC_ATTR_ATIME;

	return _RESMGR_NPARTS(1);
}

int io_write(resmgr_context_t *ctp, io_write_t *msg, metocb_t *metocb) {
	int nb = 0;

	if (metocb->ocb.attr->device == HELP) {
		nb = msg->i.nbytes;
		_IO_SET_WRITE_NBYTES(ctp, nb);
		return _RESMGR_NPARTS(0);
	}

	if (msg->i.nbytes == ctp->info.msglen - (ctp->offset + sizeof(*msg))) {
		char *buffer;
		char *message;
		int number = 0;
		buffer = (char *)(msg + 1);

		if(strstr(buffer, "start") != NULL){
			MsgSendPulse(srvr_coid, SchedGet(0, 0, NULL), START_PULSE, number);
		} else if (strstr(buffer, "stop") != NULL) {
			MsgSendPulse(srvr_coid, SchedGet(0, 0, NULL), STOP_PULSE, number);
		}
		else if (strstr(buffer, "pause") != NULL) {
			for (int i = 0; i < 2; i++) {
				message = strsep(&buffer, " ");
			}
			number = atoi(message);
			if (number >= 1 && number <= 9) {
				MsgSendPulse(srvr_coid, SchedGet(0, 0, NULL),PAUSE_PULSE, number);
			} else {
				printf("Please enter a number between 1 and 9.\n");
			}

		} else if (strstr(buffer, "quit") != NULL) {
			MsgSendPulse(srvr_coid, SchedGet(0, 0, NULL), QUIT_PULSE, number);
		} else if (strstr(buffer, "set") != NULL) {
			for (int i = 0; i < 4; i++) {
				message = strsep(&buffer, " ");
				if (i == 1) {
					input_obj.bpm = atoi(message);
				} else if (i == 2) {
					input_obj.tstop = atoi(message);
				} else if (i == 3) {
					input_obj.tsbot = atoi(message);
				}
			}

			MsgSendPulse(srvr_coid, SchedGet(0, 0, NULL), SET_PULSE, number);
		} else {
			printf("\nPlease enter a valid command (cat /dev/local/metronome-help for legal commands\n");
			strcpy(data, buffer);
		}

		nb = msg->i.nbytes;
	}

	_IO_SET_WRITE_NBYTES(ctp, nb);

	if (msg->i.nbytes > 0)
		metocb->ocb.flags |= IOFUNC_ATTR_MTIME | IOFUNC_ATTR_CTIME;

	return _RESMGR_NPARTS(0);
}

int io_open(resmgr_context_t *ctp, io_open_t *msg, RESMGR_HANDLE_T *handle,void *extra) {
	if ((srvr_coid = name_open("metronome", 0)) == -1) {
		perror("Error opening namespace");
		return EXIT_FAILURE;
	}
	return (iofunc_open_default(ctp, msg, &handle->attr, extra));
}

metocb_t *metocb_calloc(resmgr_context_t *ctp, ioattr_t *ioattr) {
	metocb_t *metocb;
	metocb = calloc(1, sizeof(metocb_t));
	metocb->ocb.offset = 0;
	return metocb;
}

void metocb_free(metocb_t *metocb) {
	free(metocb);
}
