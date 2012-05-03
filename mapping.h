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

#ifndef __MAPPING_H__
#define __MAPPING_H__
 
#define KEY_MAX_LENGTH (256)

typedef struct {
	char key[KEY_MAX_LENGTH];
	char value[KEY_MAX_LENGTH];
} map_entry_t;

bool parse_translation_table(const char *path, map_t mymap);

#endif