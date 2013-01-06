/*
    irmpexec -- LIRC client that reads IRMP events from the USB IR Remote Receiver 
                and executes a command
	        http://www.mikrocontroller.net/articles/USB_IR_Remote_Receiver
    Copyright (C) 2012  Dirk E. Wagner

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


 /* Standard headers */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/ioctl.h>
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

/* Unix socket support */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <sys/poll.h>
#include <sys/file.h>

#include "debug.h"
#include "hashmap.h"
#include "mapping.h"

static int lirc_fd = -1;
static map_t mymap;
static const char RemoteName[] = "IRMP-exec";

static struct sockaddr_un sa= {
	.sun_family = AF_UNIX,
	.sun_path = "/var/run/lirc/lircd"
};

static bool add_unixsocket(void) {
	lirc_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (lirc_fd < 0) {
		syslog (LOG_ERR, "Unable to create an AF_UNIX socket: %s", strerror(errno)); 
		return false;
	}
	
	fcntl (lirc_fd, F_SETFL, fcntl (lirc_fd, F_GETFL, 0));

	if (connect (lirc_fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
		syslog (LOG_ERR, "Unable to connect AF_UNIX socket %s: %s", sa.sun_path, strerror(errno));
		close(lirc_fd);
		lirc_fd = -1;
		return false;
	}
	
	return true;
}

static int file_ready(int FileDes, int TimeoutMs) {
	fd_set set;
	struct timeval timeout;
	FD_ZERO(&set);
	FD_SET(FileDes, &set);
	if (TimeoutMs >= 0) {
		if (TimeoutMs < 100)
       			TimeoutMs = 100;
		timeout.tv_sec  = TimeoutMs / 1000;
		timeout.tv_usec = (TimeoutMs % 1000) * 1000;
     	}
	return select (FD_SETSIZE, &set, NULL, NULL, (TimeoutMs >= 0) ? &timeout : NULL) > 0 && FD_ISSET(FileDes, &set);
}

static int safe_read(int filedes, void *buffer, int size) {
	for (;;) {
		int p = read(filedes, buffer, size);
		if (p < 0 && errno == EINTR) {
			syslog (LOG_ERR, "EINTR while reading from file handle %d - retrying", filedes);
			continue;
		}
		return p;
	}
}

static int wait_for_keycode (char *lircbuf) {

	int size = -1;
	int ready = 0;
	
	ready = file_ready(lirc_fd, -1);

	size = ready ? safe_read (lirc_fd, lircbuf, BUFSIZ) : -1;
	
//	DBG ("ready=%d size=%d\n", ready, size);
		
	if (ready && size <= 0) {
		syslog (LOG_ERR, "LIRC connection broken. Try to reconnect");
		close(lirc_fd);
		lirc_fd = -1;
		while (lirc_fd < 0) {
			sleep (3);
			if (add_unixsocket()) {
				syslog (LOG_INFO, "Reconnected to LIRC\n");
				break;
			}
		}
	} else 

	if (ready && size > 0) {
		lircbuf [size-1] = '\0';
	}

	return size;
}

static void print_help() {

	printf ("irmpexec [-w] [-d socket] [-f] [-u username] [-t path]\n\n");
	printf ("Options: \n");
	printf ("\t-d <socket> UNIX socket. The default is /var/run/lirc/lircd.\n");
	printf ("\t-f Run in the foreground.\n");
	printf ("\t-u <user> User name.\n");
	printf ("\t-t <path> Path to translation table.\n");
	printf ("\t-w lirc irw like mode, print data.\n");
	
}

static void main_loop(bool irw_mode) {

	int repeat = 0;
	int bufSize = 0;
	char lircbuf[BUFSIZ] = "";
	char irmp_data[BUFSIZ] = "";
	char irmp_code[BUFSIZ] = "";
	char id[BUFSIZ] = ""; 
	
	if (irw_mode) {
		printf ("irmpdata\t|repeat\t|irmp_codes\t|id\n"); 
	}

	do {
		memset(&lircbuf, 0, sizeof(lircbuf));
		if ((bufSize = wait_for_keycode (lircbuf)) > 0) {
			sscanf (lircbuf, "%s %d %s %s", irmp_data, &repeat, irmp_code, id);
			DBG ("irmpdata=%s repeat=%d irmp_code=%s id=%s\n", irmp_data, repeat, irmp_code, id);
			if (strncasecmp (id, "IRMP", 4) == 0) {
				if (irw_mode) {
					printf ("%s\t|%d\t|%s\t|%s\n", irmp_data, repeat, irmp_code, id);
				} else {
					if (!repeat) {

						map_entry_t *map_entry;
			
						if(hashmap_get(mymap, irmp_code, (void**)(&map_entry))==MAP_OK) {
							DBG ("MAP_OK map_entry->irmp_code=%s map_entry->value=%s\n", map_entry->key, map_entry->value);	
							syslog(LOG_INFO, "executing by IRMP (%s)", map_entry->value);
							system (map_entry->value);
						} else {
							DBG ("MAP_ERROR irmp_code=%s|\n", irmp_code);	
						}

					} else {
						DBG ("Repetition ignored\n");
					}
				}
			} else {
				DBG ("Wrong ID %s, code ignored\n", id);
			}
			
		}
	} while (true);

}

int main(int argc, char *argv[]) {
	
	char *user = "nobody";
	char *translation_path = "/etc/irmpexec.map";
	int opt;
	bool foreground = false;
	bool irw_mode = false;

	while ((opt = getopt(argc, argv, "whd:fu:t:")) != -1) {
        	switch (opt) {
			case 'd':
				strncpy (sa.sun_path, optarg, sizeof sa.sun_path - 1); 
				break;
			case 'u':
				user = strdup (optarg);
				break;
			case 'f':
				foreground = true;
				break;
			case 't':
				translation_path = strdup (optarg);
				break;
			case 'w':
				irw_mode = true;
				foreground = true;
				break;
			case 'h':
				print_help ();
				return 0;
            		default:
				print_help ();
                		return EX_USAGE;
        	}
    	}

	mymap = hashmap_new();
	
	if (!parse_translation_table(translation_path, mymap)) {
		hashmap_free(mymap);
		return EX_OSERR;
	}

	if (!add_unixsocket()) {
		hashmap_free(mymap);
		if (lirc_fd >= 0) close(lirc_fd);
		return EX_OSERR;
	}

	struct passwd *pwd = getpwnam(user);
	if(!pwd) {
		hashmap_free(mymap);
		if (lirc_fd >= 0) close(lirc_fd);
		fprintf(stderr, "Unable to resolve user %s!\n", user);
		return EX_OSERR;
	}

	if(setgid(pwd->pw_gid) || setuid(pwd->pw_uid)) {
		hashmap_free(mymap);
		if (lirc_fd >= 0) close(lirc_fd);
		fprintf(stderr, "Unable to setuid/setguid to %s!\n", user);
		return EX_OSERR;
	}

	if(!foreground)
		daemon(0, 0);

	syslog(LOG_INFO, "Started");

	signal(SIGPIPE, SIG_IGN);

	main_loop(irw_mode);

	/* Now, destroy the map */
	hashmap_free(mymap);
	if (lirc_fd >= 0) close(lirc_fd);
	
	return 0;
}
