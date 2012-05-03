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
#include <syslog.h>
#include <ctype.h>

#include "debug.h"
#include "hashmap.h"
#include "mapping.h"

bool parse_translation_table(const char *path, map_t mymap) {
	FILE *table;
	char *line = NULL;
	size_t line_size = 0;
	char key[KEY_MAX_LENGTH];
	char value[KEY_MAX_LENGTH];
	int error = 0;
	int len;

	key[0] = value[0] = 0;	
	if(!path)
		return false;

	table = fopen(path, "r");
	if(!table) {
		fprintf(stderr, "Could not open translation table %s: %s\n", path, strerror(errno));
		return false;
	}

	while(getline(&line, &line_size, table) >= 0) {
		len = sscanf(line, "%99s %99s", key, value);
		if(len != 2) {
			syslog(LOG_ERR, "line ignored: %s\n", line);
			continue;
		}
		key[KEY_MAX_LENGTH-1] = '\0';
		value[KEY_MAX_LENGTH-1] = '\0';
		
		if(strlen(key) < 1 || strlen(value) < 1)
			continue;

		DBG ("parse_translation_table: key = %s, value = %s\n", key, value);
		
		map_entry_t *map_entry = malloc(sizeof(map_entry_t));
  		snprintf(map_entry->key, KEY_MAX_LENGTH, "%s", key);
		snprintf(map_entry->value, KEY_MAX_LENGTH, "%s", value);

		error = hashmap_put(mymap, map_entry->key, map_entry);			

		if(error) {
			fprintf(stderr, "hashmap_put failure: %d\n", error);
			fclose(table);
			free(line);
			return false;
		}
	}

	fclose(table);
	free(line);
	
	return true;
}
