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

int main(int argc, char *argv[]) {
	dispatch_t* dpp;
	resmgr_io_funcs_t io_funcs;
	resmgr_connect_funcs_t connect_funcs;
	ioattr_t ioattrs[NumDevices];
	dispatch_context_t *ctp;
	int id;

	if (argc != 4) {
		perror("Incorrect number of command line args\n");
		exit(EXIT_FAILURE);
	}

	input_obj->bpm = atoi(argv[1]);
	input_obj->tstop = atoi(argv[2]);
	input_obj->tsbot = atoi(argv[3]);

	iofunc_funcs_t metocb_funcs = {
	_IOFUNC_NFUNCS, metocb_calloc, metocb_free };

	iofunc_mount_t metocb_mount = { 0, 0, 0, 0, &metro_ocb_funcs };

	if ((dpp = dispatch_create()) == NULL) {
		fprintf(stderr, "%s:  Unable to allocate dispatch context.\n", argv[0]);
		return (EXIT_FAILURE);
	}
	iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS,
			&io_funcs);
	connect_funcs.open = io_open;
	io_funcs.read = io_read;
	io_funcs.write = io_write;

	iofunc_attr_init(&ioattr, S_IFCHR | 0666, NULL, NULL);

	if ((id = resmgr_attach(dpp, NULL, "/dev/example", _FTYPE_ANY, 0,
			&connect_funcs, &io_funcs, &ioattr)) == -1) {
		fprintf(stderr, "%s:  Unable to attach name.\n", argv[0]);
		return (EXIT_FAILURE);
	}

	for (int i = 0; i < NumDevices; i++) {
		iofunc_attr_init(&ioattrs[i].attr, S_IFCHR | 0666, NULL, NULL);
		ioattrs[i].device = i;
		ioattrs[i].attr.mount = &time_mount;
		resmgr_attach(dpp, NULL, devnames[i], _FTYPE_ANY, NULL, &connect_funcs,
				&io_funcs, &ioattrs[i]);
	}

	ctp = dispatch_context_alloc(dpp);
	while (1) {
		ctp = dispatch_block(ctp);
		dispatch_handler(ctp);
	}
	return EXIT_SUCCESS;
}

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
				if (*tblp == "|") { // if start of pattern
					printf("%.2s", tblp); //print first 2
					tblp += 2; // increment pointer
				} else if (*tblp == '\0') {
					printf('\n'); // print newline
					tblp = table_lookup(table_idx); // reset pattern
				} else { // mid-measure print and increment
					printf('%c', *tblp);
					tblp++;
				}
				break;
			case PAUSE_PULSE_CODE:
				if (status == STARTED) {
					//set delay in timer
					itime.it_value.tv_sec = msg.pulse.value.sival_int;
					timer_settime(timer_id, 0, &itime, NULL);
				}
				break;
			case SET_PULSE_CODE: // set a new pattern
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

int io_read(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb) {
	static char data[] = "hello";
	int nb;

	nb = strlen(data);

	//test to see if we have already sent the whole message.
	if (ocb->offset == nb)
		return 0;

	//We will return which ever is smaller the size of our data or the size of the buffer
	nb = min(nb, msg->i.nbytes);

	//Set the number of bytes we will return
	_IO_SET_READ_NBYTES(ctp, nb);

	//Copy data into reply buffer.
	SETIOV(ctp->iov, data, nb);

	//update offset into our data used to determine start position for next read.
	ocb->offset += nb;

	//If we are going to send any bytes update the access time for this resource.
	if (nb > 0)
		ocb->attr->flags |= IOFUNC_ATTR_ATIME;

	return (_RESMGR_NPARTS(1));
}

int io_write(resmgr_context_t *ctp, io_write_t *msg, RESMGR_OCB_T *ocb) {
	int nb;

	if (msg->i.nbytes == ctp->info.msglen - (ctp->offset + sizeof(*msg))) {
		/* have all the data */
		char *buf;
		buf = (char *) (msg + 1);

		nb = write( STDOUT_FILENO, buf, msg->i.nbytes); // fd 1 is stdout
		if (-1 == nb)
			return errno;
	} else {
#if 0
		//Get all the data in one big buffer
		char *buf;
		buf = malloc( msg->i.nbytes );
		if (NULL == buf )
		return ENOMEM;
		nb = resmgr_msgread(ctp, buf, msg->i.nbytes, sizeof(*msg));
		nb = write(1, buf, nb );// fd 1 is stdout
		free(buf);
		if( -1 == nb )
		return errno;
#else
		//Reuse the same buffer over and over again.
		char buf[1000]; // my hardware buffer
		int count, bytes;
		count = 0;

		while (count < msg->i.nbytes) {
			bytes = resmgr_msgread(ctp, buf, 1000, count + sizeof(*msg));
			if (bytes == -1)
				return errno;
			bytes = write(1, buf, bytes); // fd 1 is standard out
			if (bytes == -1) {
				if (!count)
					return errno;
				else
					break;
			}
			count += bytes;
		}
		nb = count;
#endif
	}
	_IO_SET_WRITE_NBYTES(ctp, nb);

	if (msg->i.nbytes > 0)
		ocb->attr->flags |= IOFUNC_ATTR_MTIME | IOFUNC_ATTR_CTIME;

	return (_RESMGR_NPARTS(0));
}

int io_open(resmgr_context_t *ctp, io_open_t *msg, RESMGR_HANDLE_T *handle,
		void *extra) {
	return (iofunc_open_default(ctp, msg, handle, extra));
}

metocb_t metocb_calloc(resmgr_context_t *ctp, ioattr_t *mattr) {
	metocb_t *metocb;
	metocb = calloc(1, sizeof(metocb_t));
	metocb->ocb.offset = 0;
	return(metocb);
}

void metocb_free(metocb_t *metocb) {
	free(metocb);
}
