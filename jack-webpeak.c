/** jack-webpeak - JACK audio peak server
 *
 * This tool is based on capture_client.c from the jackaudio.org examples
 * and modified by Robin Gareus <robin@gareus.org>, further modified by
 * Sam Mulvey <code@ktqa.org> to fit in with modern web standards.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Copyright (C) 2001 Paul Davis
 * Copyright (C) 2003 Jack O'Quin
 * Copyright (C) 2008, 2012 Robin Gareus
 * Copyright (C) 2020 Sam Mulvey
 *
 */

#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <signal.h>
#include <math.h>
#include <errno.h>
#include <jack/jack.h>
#include <ws.h>


#ifndef VERSION
#define VERSION "0.9"
#endif

typedef struct _jack_peaksock_thread_info {
	pthread_t thread_id;
	pthread_t mesg_thread_id;
	jack_nframes_t samplerate;
	jack_nframes_t buffersize;
	jack_client_t *client;
	unsigned int channels;
	useconds_t delay;
	float *peak;
	float *pcur;
	float *pmax;
	int   *ptme;
	/* format - bitwise
 	 * 1  -- on: use libwebsock
 	 * 2  -- on: IEC-268 dB scale, off: linear float
 	 * 4  -- on: JSON, off: plain text
 	 * 8  -- include peak-hold
 	 * 16 -- include xruns
	 */
	int format;
	float iecmult;
	int xruns;
	volatile int can_capture;
	volatile int can_process;
} jack_thread_info_t;

#define SAMPLESIZE (sizeof(jack_default_audio_sample_t))
#define BUF_BYTES_PER_CHANNEL 16
#define BUF_EXTRA 64


/* JACK data */
jack_port_t **ports;
jack_default_audio_sample_t **in;
jack_nframes_t nframes;

/* Synchronization between process thread and disk thread. */
pthread_mutex_t io_thread_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  data_ready = PTHREAD_COND_INITIALIZER;

/* global options/status */
int wsport = 0, want_quiet = 0, xrun_count = 0;
volatile int run = 1;

void cleanup(jack_thread_info_t *info) {
	free(info->peak);
	free(info->pcur);
	free(info->pmax);
	free(info->ptme);
	free(in);
	free(ports);
}

/* output functions and helpers */

float iec_scale(float db) {
	float def = 0.0f;

	if (db < -70.0f) {
		def = 0.0f;
	} else if (db < -60.0f) {
		def = (db + 70.0f) * 0.25f;
	} else if (db < -50.0f) {
		def = (db + 60.0f) * 0.5f + 2.5f;
	} else if (db < -40.0f) {
		def = (db + 50.0f) * 0.75f + 7.5;
	} else if (db < -30.0f) {
		def = (db + 40.0f) * 1.5f + 15.0f;
	} else if (db < -20.0f) {
		def = (db + 30.0f) * 2.0f + 30.0f;
	} else if (db < 0.0f) {
		def = (db + 20.0f) * 2.5f + 50.0f;
	} else {
		def = 100.0f;
	}
	return def;

}

int peak_db(float peak, float bias, float mult) {
	return (int) (iec_scale(20.0f * log10f(peak * bias)) * mult);
}

void * io_thread (void *arg) {
	jack_thread_info_t *info = (jack_thread_info_t *) arg;

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_mutex_lock(&io_thread_lock);

	char buffer[ (BUF_BYTES_PER_CHANNEL * info->channels) + BUF_EXTRA ]; // just... just let me have this.

	while (run) {

		const int pkhld = ceilf(2.0 / ( (info->delay/1000000.0) + ((float)info->buffersize / (float)info->samplerate)));

		if (info->can_capture) {
			int chn;
			int buf_pos = 0;

			memcpy(info->peak, info->pcur, sizeof(float)*info->channels);


			// json header
			if (info->format & 4) buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos,"{\"cnt\":%d,\"peak\":[", info->channels);



			for (chn = 0; chn < info->channels; ++chn) {
				info->pcur[chn]=0.0;

				switch (info->format & 6) {
					case 0: // raw formatting
						buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos, "%3.3f  ", info->peak[chn]);
						break;

					case 2: // IEC-268, raw
						buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos,"%3d  ", peak_db(info->peak[chn], 1.0, info->iecmult));
						break;

					case 4: // raw, JSON
						buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos, "%.3f", info->peak[chn]);
						if (chn < info->channels - 1) buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos,",");
						break;

					case 6: // IEC-268, JSON
						buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos, "%d", peak_db(info->peak[chn], 1.0, info->iecmult));
						if (chn < info->channels - 1) buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos,",");
						break;
				}

				// manage peak hold
				if (info->peak[chn] > info->pmax[chn]) {
					info->pmax[chn] = info->peak[chn];
					info->ptme[chn] = 0;
				} else if (info->ptme[chn] <= pkhld) {
					(info->ptme[chn])++;
				} else {
					info->pmax[chn] = info->peak[chn];
				}


			}

			// display peak hold
			if (info->format & 8) {

				// my momma told me this was faster
				buf_pos += snprintf(
					buffer+buf_pos,
					sizeof(buffer)-buf_pos,
					( (info->format & 4) ? "],\"max\":["  : " | " ) // JSON interstitial or just some junk
				);

				// As above, so below.
				for (chn = 0; chn < info->channels; ++chn) {
					switch (info->format & 6) {
						case 0: // raw formatting
							buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos, "%3.3f  ", info->pmax[chn]);
							break;

						case 2: // IEC-268, raw
							buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos,"%3d  ", peak_db(info->pmax[chn], 1.0, info->iecmult));
							break;

						case 4: // raw, JSON
							buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos, "%.3f", info->pmax[chn]);
							if (chn < info->channels - 1) buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos,",");
							break;

						case 6: // IEC-268, JSON
							buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos, "%d", peak_db(info->pmax[chn], 1.0, info->iecmult));
							if (chn < info->channels - 1) buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos,",");
							break;

					}
				}
			} // end display peak hold

			// JSON footer, if ya need it.
			if (info->format & 4) buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos,"]");

			if (info->format & 16) {
				if (info->format & 4) {
					buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos, ",\"xruns\":%d", xrun_count);
				} else {
					buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos, " XRUNS: %d", xrun_count);
				}
			}


			if (info->format & 4) buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos,"}");


			if (info->format & 1) {
				//websocket, is always \r\n
				buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos, "\r\n");
				ws_sendframe_txt_bcast(wsport, buffer);
			} else {

				if (isatty(fileno(stdout))) { // are we a tty, then just CR to save space.
					buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos, "\r");
				} else { // otherwise, we're output to a file, so newline.
					buf_pos += snprintf(buffer+buf_pos, sizeof(buffer)-buf_pos, "\n");
				}

				fprintf(stdout, buffer);
				fflush(stdout);
			}

			if (info->delay>0) usleep(info->delay);
		}
		pthread_cond_wait(&data_ready, &io_thread_lock);
	}

	pthread_mutex_unlock(&io_thread_lock);
	return 0;
}

/* jack callbacks */

int jack_bufsiz_cb(jack_nframes_t nframes, void *arg) {
	jack_thread_info_t *info = (jack_thread_info_t *) arg;
	info->buffersize=nframes;
	return 0;
}

int jack_xrun_cb(void *arg) {
	//jack_thread_info_t *info = (jack_thread_info_t *) arg;
	pthread_mutex_lock(&io_thread_lock);
	xrun_count++;
	pthread_mutex_unlock(&io_thread_lock);
	return 0;
}

void jack_xrun_clear() {
	fprintf(stderr, "clearing xrun count\n");
	pthread_mutex_lock(&io_thread_lock);
	xrun_count = 0;
	pthread_mutex_unlock(&io_thread_lock);
}

int process (jack_nframes_t nframes, void *arg) {
	int chn;
	size_t i;
	jack_thread_info_t *info = (jack_thread_info_t *) arg;

	/* Do nothing until we're ready to begin. */
	if ((!info->can_process) || (!info->can_capture))
		return 0;

	for (chn = 0; chn < info->channels; ++chn)
		in[chn] = jack_port_get_buffer(ports[chn], nframes);

	for (i = 0; i < nframes; i++) {
		for (chn = 0; chn < info->channels; ++chn) {
			const float js = fabsf(in[chn][i]);
			if (js > info->pcur[chn]) info->pcur[chn] = js;
		}
	}

	/* Tell the io thread there is work to do. */
	if (pthread_mutex_trylock(&io_thread_lock) == 0) {
		pthread_cond_signal(&data_ready);
		pthread_mutex_unlock(&io_thread_lock);
	}
	return 0;
}

void jack_shutdown (void *arg) {
	fprintf(stderr, "JACK shutdown\n");
	abort();
}

void setup_ports (int nports, char *source_names[], jack_thread_info_t *info) {
	unsigned int i;
	const size_t in_size =  nports * sizeof(jack_default_audio_sample_t *);

	info->peak = malloc(sizeof(float) * nports);
	info->pcur = malloc(sizeof(float) * nports);
	info->pmax = malloc(sizeof(float) * nports);
	info->ptme = malloc(sizeof(int  ) * nports);

	/* Allocate data structures that depend on the number of ports. */
	ports = (jack_port_t **) malloc(sizeof(jack_port_t *) * nports);
	in = (jack_default_audio_sample_t **) malloc(in_size);
	memset(in, 0, in_size);

	for (i = 0; i < nports; i++) {
		char name[64];
		info->peak[i]=0.0;
		info->pcur[i]=0.0;
		info->ptme[i]=0;

		sprintf(name, "input_%d", i+1);

		if ((ports[i] = jack_port_register(info->client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0)) == 0) {
			fprintf(stderr, "cannot register input port \"%s\"!\n", name);
			jack_client_close(info->client);
			cleanup(info);
			exit(1);
		}
	}

	for (i = 0; i < nports; i++) {
		if (jack_connect(info->client, source_names[i], jack_port_name(ports[i]))) {
			fprintf(stderr, "cannot connect input port %s to %s\n", jack_port_name(ports[i]), source_names[i]);
		}
	}

	/* process() can start, now */
	info->can_process = 1;
}


void catchsig (int sig) {
	if (!want_quiet)
		fprintf(stderr,"\nCAUGHT SIGNAL - shutting down.\n");
	run=0;
	/* signal writer thread */
	pthread_mutex_lock(&io_thread_lock);
	pthread_cond_signal(&data_ready);
	pthread_mutex_unlock(&io_thread_lock);
}


static void usage (char *name, int status) {
	FILE *place = status ? stderr : stdout;

	fprintf(place, "%s - live peak/signal meter for JACK\n", basename(name));
	fprintf(place, "Utility to write audio peak to stdout or via websocket\n\n");
	fprintf(place, "Usage: %s [ OPTIONS ] port1 [ port2 ... ]\n\n", name);

	fprintf(place,
		"Options:\n"
		"  -n, --name <name>        name in JACK patchbay\n"
		"  -w, --websocket <port>   start localhost websocket server on port\n"
		"  -d, --delay <int>        output speed in miliseconds (default 100ms)\n"
		"                           a delay of zero writes at jack-period intervals\n"
		"  -h, --help               print this message\n"
		"  -i, --iec268 <mult>      use dB scale; output range 0-<mult> (integer)\n"
		"                           - if not specified, output range is linear [0..1]\n"
		"  -j, --json               write JSON format instead of plain text\n"
		"  -p, --peakhold           add peak-hold information\n"
		"  -x, --xruns              include xruns count\n"
		"  -q, --quiet              inhibit usual output\n"
		"  -v, --version            print version information\n"
		"\n"
		"Examples:\n"
		"jack-webpeak system:capture_1 system:capture_2\n"
		"\n"
		"jack-webpeak --websocket 8000 --iec268 200 --json  system:capture_1\n"
		"\n"
		"Report bugs to <code@ktqa.org>.\n"
		"Website and manual: <https://github.com/refutationalist/jack-webpeak>\n"
	);
	exit(status);
}

static void version(char *name) {
	printf(
		"%s %s\n\n"
		"Copyright (C) 2020 Sam Mulvey <code@ktqa.org>\n"
		"This is free software; see the source for copying conditions.  There is NO\n"
		"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n",
		name,
		VERSION
	);
	exit(0);

}

void websocket_connect(ws_cli_conn_t *client) {
	if (!want_quiet) fprintf(stderr, "websocket connected from port %s\n", ws_getport(client));
}

void websocket_disconnect(ws_cli_conn_t *client) {
	if (!want_quiet) fprintf(stderr, "websocket disconnected from port %s\n", ws_getport(client));
}

void websocket_in(ws_cli_conn_t *client, const unsigned char *msg, uint64_t size, int type) {
	if (!want_quiet) fprintf(stderr, "incoming garbage from websocket client\n");
}

int main (int argc, char **argv) {
	jack_client_t *client;
	jack_thread_info_t thread_info;
	jack_status_t jstat;
	int c;
	char jackname[33] = "jackpeak";

	memset(&thread_info, 0, sizeof(thread_info));
	thread_info.channels = 2;
	thread_info.delay = 100000;
	thread_info.format = 0;
	thread_info.iecmult = 2.0;
	thread_info.xruns = 0;

	const char *optstring = "hqn:pji:d:w:vx";
	struct option long_options[] = {
		{ "help",      no_argument,       0, 'h' },
		{ "json",      no_argument,       0, 'j' },
		{ "iec268",    required_argument, 0, 'i' },
		{ "delay",     required_argument, 0, 'd' },
		{ "peakhold",  required_argument, 0, 'p' },
		{ "quiet",     no_argument,       0, 'q' },
		{ "version",   no_argument,       0, 'v' },
		{ "xruns",     no_argument,       0, 'x' },
		{ "name",      required_argument, 0, 'n' },
		{ "websocket", required_argument, 0, 'w' },
		{ 0, 0, 0, 0 }
	};

	while ((c = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
		switch (c) {
			case 'h':
				usage(argv[0], 0);
				break;
			case 'q':
				want_quiet = 1;
				break;
			case 'i':
				thread_info.format|=2;
				thread_info.iecmult = atof(optarg)/100.0;
				break;
			case 'j':
				thread_info.format|=4;
				break;
			case 'p':
				thread_info.format|=8;
				break;
			case 'x':
				thread_info.format|=16;
				break;
			case 'n':
				strncpy(jackname, optarg, sizeof(jackname) - 1);
				break;
			case 'd':
				if (atol(optarg) < 0 || atol(optarg) > 60000)
					fprintf(stderr, "delay: time out of bounds.\n");
				else
					thread_info.delay = 1000*atol(optarg);
				break;
			case 'v':
				version(argv[0]);
				break;
			case 'w':
				if (atoi(optarg) < 1024 || atoi(optarg) > 65535) {
					fprintf(stderr, "websocket: bad port.  pick between 1024 and 65535.\n");
					exit(1);
				} else {
					wsport = atoi(optarg);
					thread_info.format|=1;
				}
				break;
			default:
				fprintf(stderr, "invalid argument.\n");
				usage(argv[0], 0);
				break;
		}
	}

	if (argc <= optind) {
		fprintf(stderr, "At least one port/audio-channel must be given.\n");
		usage(argv[0], 1);
	}

	/* set up JACK client */
	if ((client = jack_client_open(jackname, JackNoStartServer, &jstat)) == 0) {
		fprintf(stderr, "Can not connect to JACK.\n");
		exit(1);
	}

	thread_info.client = client;
	thread_info.can_process = 0;
	thread_info.channels = argc - optind;

	jack_set_process_callback(client, process, &thread_info);
	jack_on_shutdown(client, jack_shutdown, &thread_info);
	jack_set_buffer_size_callback(client, jack_bufsiz_cb, &thread_info);
	jack_set_xrun_callback(client, jack_xrun_cb, &thread_info);

	if (jack_activate(client)) {
		fprintf(stderr, "cannot activate client");
	}

	setup_ports(thread_info.channels, &argv[optind], &thread_info);

	signal(SIGHUP, jack_xrun_clear);
	signal(SIGINT, catchsig);
	signal(SIGPIPE, catchsig);

	/* set up i/o thread */
	pthread_create(&thread_info.thread_id, NULL, io_thread, &thread_info);
	thread_info.samplerate = jack_get_sample_rate(thread_info.client);

	if (!want_quiet) {
		fprintf(stderr, "%i channel%s, @%iSPS",
			thread_info.channels,
			(thread_info.channels>1)?"s":"",
			thread_info.samplerate
		);
		if (wsport != 0) fprintf(stderr, ", server @ ws://localhost:%d", wsport);
		fprintf(stderr, "\n");

	}

	/* all systems go - run the i/o thread */
	/*
	if (lw_port != 0) {
		struct ws_events wse;
		wse.onopen    = &websocket_connect;
		wse.onclose   = &websocket_disconnect;
		wse.onmessage = &websocket_in;
		ws_socket(&wse, "localhost", lw_port, 1, 0);
	}
	*/

	if (wsport != 0) {
		ws_socket(&(struct ws_server){
			/*
		 	 * Bind host:
		 	 * localhost -> localhost/127.0.0.1
		 	 * 0.0.0.0   -> global IPv4
		 	 * ::        -> global IPv4+IPv6 (DualStack)
		 	 */
			.host = "localhost",
			.port = wsport,
			.thread_loop   = 1,
			.timeout_ms    = 1000,
			.evs.onopen    = &websocket_connect,
			.evs.onclose   = &websocket_disconnect,
			.evs.onmessage = &websocket_in
		});
	}
	thread_info.can_capture = 1;
	pthread_join(thread_info.thread_id, NULL);
	jack_client_close(client);

	cleanup(&thread_info);

	return(0);
}
