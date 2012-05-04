/*
    irmplircd -- zeroconf LIRC daemon that reads IRMP events from the USB IR Remote Receiver
	             http://www.mikrocontroller.net/articles/USB_IR_Remote_Receiver
    Copyright (C) 2011-2012  Dirk E. Wagner

	based on:
    inputlircd -- zeroconf LIRC daemon that reads from /dev/input/event devices
    Copyright (C) 2006  Guus Sliepen <guus@sliepen.eu.org>

    This program is free software; you can redistribute it and/or modify it
    under the terms of version 2 of the GNU General Public License as published
    by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sysexits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <pwd.h>
#include <ctype.h>

/* Input subsystem interface */
#include <linux/input.h>

#include "debug.h"
#include "hashmap.h"
#include "mapping.h"

typedef struct {
  uint8_t	dummy;		// additional useless byte 
  uint8_t	protocol;	// protocol, i.e. NEC_PROTOCOL
  uint16_t	address;	// address
  uint16_t	command;	// command
  uint8_t	flags;		// flags, e.g. repetition
} IRMP_DATA;

typedef struct evdev {
	char *name;
	int fd;
	struct evdev *next;
} evdev_t;

static evdev_t *evdevs = NULL;

typedef struct client {
	int fd;
	struct client *next;
} client_t;

static client_t *clients = NULL;

static int sockfd = -1;

static bool grab = false;
static char *device = "/var/run/lirc/lircd";

static long repeat_time = 0L;
static struct timeval previous_input;
static int repeat = 0;

static map_t mymap;

static void *xalloc(size_t size) {
	void *buf = malloc(size);
	if(!buf) {
		fprintf(stderr, "Could not allocate %zd bytes with malloc(): %s\n", size, strerror(errno));
		exit(EX_OSERR);
	}
	memset(buf, 0, size);
	return buf;
}

static void add_evdevs(int argc, char *argv[]) {
	int i;
	evdev_t *newdev;

	for(i = 0; i < argc; i++) {
		newdev = xalloc(sizeof *newdev);
		newdev->fd = open(argv[i], O_RDONLY);
		if(newdev->fd < 0) {
			free(newdev);
			fprintf(stderr, "Could not open %s: %s\n", argv[i], strerror(errno));
			continue;
		}
		if(grab) {
			if(ioctl(newdev->fd, EVIOCGRAB, 1) < 0) {
				close(newdev->fd);
				free(newdev);
				fprintf(stderr, "Failed to grab %s: %s\n", argv[i], strerror(errno));
				continue;
			}
		}
		newdev->name = basename(strdup(argv[i]));
		newdev->next = evdevs;
		evdevs = newdev;
	}
}
	
static bool add_unixsocket(void) {
	struct sockaddr_un sa = {0};
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	if(sockfd < 0) {
		fprintf(stderr, "Unable to create an AF_UNIX socket: %s\n", strerror(errno));
		return false;
	}

	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, device, sizeof sa.sun_path - 1);

	unlink(device);

	if(bind(sockfd, (struct sockaddr *)&sa, sizeof sa) < 0) {
		fprintf(stderr, "Unable to bind AF_UNIX socket to %s: %s\n", device, strerror(errno));
		return false;
	}

	chmod(device, 0666);

	if(listen(sockfd, 3) < 0) {
		fprintf(stderr, "Unable to listen on AF_UNIX socket: %s\n", strerror(errno));
		return false;
	}

	return true;
}


static void processnewclient(void) {
	client_t *newclient = xalloc(sizeof *newclient);

	newclient->fd = accept(sockfd, NULL, NULL);

	if(newclient->fd < 0) {
		free(newclient);
		if(errno == ECONNABORTED || errno == EINTR)
			return;
		syslog(LOG_ERR, "Error during accept(): %s\n", strerror(errno));
		exit(EX_OSERR);
	}

    int flags = fcntl(newclient->fd, F_GETFL);
    fcntl(newclient->fd, F_SETFL, flags | O_NONBLOCK);

	newclient->next = clients;
	clients = newclient;
}

static long time_elapsed(struct timeval *last, struct timeval *current) {
	long seconds = current->tv_sec - last->tv_sec;
	return 1000000 * seconds + current->tv_usec - last->tv_usec;
}

static void processevent(evdev_t *evdev) {
	IRMP_DATA event;
	char hash_key[100];
	char irmp_fulldata[100];
	char message[100];
	int len;
	client_t *client, *prev, *next;

	message[0]=0;
	
	if((len=read(evdev->fd, &event, sizeof event)) <= 0) {
		syslog(LOG_ERR, "Error processing event from %s: %s\n", evdev->name, strerror(errno));
		exit(EX_OSERR);
	}

	DBG ("dummy = 0x%02d, p = %02d, a = 0x%04x, c = 0x%04x, f = 0x%02x\n", event.dummy, event.protocol, event.address, event.command, event.flags);
		
	struct timeval current;
	gettimeofday(&current, NULL);
	if(time_elapsed(&previous_input, &current) < repeat_time)
		repeat++;
	else 
		repeat = 0;

	snprintf (irmp_fulldata, sizeof irmp_fulldata, "%02x%04x%04x%02x", event.protocol, event.address, event.command, event.flags);
	snprintf (hash_key, sizeof hash_key, "%02x%04x%04x%02x", event.protocol, event.address, event.command, 0);

	map_entry_t *map_entry;
	
	if(hashmap_get(mymap, hash_key, (void**)(&map_entry))==MAP_OK) {
		DBG ("MAP_OK irmpd_fulldata=%s lirc=%s\n", irmp_fulldata, map_entry->value);	
		len = snprintf(message, sizeof message, "%s %x %s %s\n",  irmp_fulldata, event.flags, map_entry->value, "IRMP");
	} else {
		DBG ("MAP_ERROR irmpd_fulldata=%s|\n", irmp_fulldata);	
		len = snprintf(message, sizeof message, "%s %x %s %s\n",  irmp_fulldata, repeat, irmp_fulldata, "IRMP");
	}
	
	DBG ("LIRC message=%s\n", message);

	previous_input = current;
	for(client = clients; client; client = client->next) {
		if(write(client->fd, message, len) != len) {
			close(client->fd);
			client->fd = -1;
		}
	}

	for(prev = NULL, client = clients; client; client = next) {
		next = client->next;
		if(client->fd < 0) {
			if(prev)
				prev->next = client->next;
			else
				clients = client->next;
			free(client);
		} else {
			prev = client;
		}
	}
}

static void print_help() {

	printf("irmplircd [-d socket] [-f] [-c] [-r repeat-rate] [-m keycode] -u username] device [device ...]\n\n");
	printf("Options: \n");
	printf("\t-d <socket> UNIX socket. The default is /var/run/lirc/lircd.\n");
	printf("\t-f Run in the foreground.\n");
	printf("\t-r <rate> Repeat rate in ms.\n");
	printf("\t-g Grab the input device(s).\n");
	printf("\t-u <user> User name.\n");
	printf("\t-t <path> Path to translation table.\n");
	printf("\tdevice The input device e.g. /dev/hidraw0\n");
	
}

static void main_loop(void) {
	fd_set permset;
	fd_set fdset;
	evdev_t *evdev;
	int maxfd = 0;

	FD_ZERO(&permset);
	
	for(evdev = evdevs; evdev; evdev = evdev->next) {
		FD_SET(evdev->fd, &permset);
		if(evdev->fd > maxfd)
			maxfd = evdev->fd;
	}
	
	FD_SET(sockfd, &permset);
	if(sockfd > maxfd)
		maxfd = sockfd;

	maxfd++;
	
	while(true) {
		fdset = permset;
		
		if(select(maxfd, &fdset, NULL, NULL, NULL) < 0) {
			if(errno == EINTR)
				continue;
			syslog(LOG_ERR, "Error during select(): %s\n", strerror(errno));
			exit(EX_OSERR);
		}

		for(evdev = evdevs; evdev; evdev = evdev->next)
			if(FD_ISSET(evdev->fd, &fdset))
				processevent(evdev);

		if(FD_ISSET(sockfd, &fdset))
			processnewclient();
	}
}

int main(int argc, char *argv[]) {
	char *user = "nobody";
	char *translation_path = NULL;
	int opt;
	bool foreground = false;

	gettimeofday(&previous_input, NULL);

	while((opt = getopt(argc, argv, "d:gm:fu:r:t:")) != -1) {
        switch(opt) {
			case 'd':
				device = strdup(optarg);
				break;
			case 'g':
				grab = true;
				break;
			case 'u':
				user = strdup(optarg);
				break;
			case 'f':
				foreground = true;
				break;
			case 'r':
				repeat_time = atoi(optarg) * 1000L;
				break;
			case 't':
				translation_path = strdup(optarg);
				break;
            default:
				print_help();
                return EX_USAGE;
        }
    }

	if(argc <= optind) {
		fprintf(stderr, "Not enough arguments.\n");
		return EX_USAGE;
	}

	add_evdevs(argc - optind, argv + optind);

	if(!evdevs) {
		fprintf(stderr, "Unable to open any event device!\n");
		return EX_OSERR;
	}

	mymap = hashmap_new();
	
	if(!parse_translation_table(translation_path, mymap)) {
		hashmap_free(mymap);
		return EX_OSERR;
	}

	if (!add_unixsocket()) {
		hashmap_free(mymap);
		if (sockfd >= 0) close (sockfd);
		return EX_OSERR;
	}

	struct passwd *pwd = getpwnam(user);
	if(!pwd) {
		fprintf(stderr, "Unable to resolve user %s!\n", user);
		hashmap_free(mymap);
		if (sockfd >= 0) close (sockfd);
		return EX_OSERR;
	}

	if(setgid(pwd->pw_gid) || setuid(pwd->pw_uid)) {
		fprintf(stderr, "Unable to setuid/setguid to %s!\n", user);
		hashmap_free(mymap);
		if (sockfd >= 0) close (sockfd);
		return EX_OSERR;
	}

	if(!foreground)
		daemon(0, 0);

	syslog(LOG_INFO, "Started");

	signal(SIGPIPE, SIG_IGN);

	main_loop();

	/* Now, destroy the map */
	hashmap_free(mymap);
	if (sockfd >= 0) close (sockfd);

	return 0;
}
