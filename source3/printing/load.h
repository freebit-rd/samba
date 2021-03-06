/*
   Unix SMB/CIFS implementation.
   load printer lists
   Copyright (C) Andrew Tridgell 1992-2000

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _PRINTING_LOAD_H_
#define _PRINTING_LOAD_H_

/* The following definitions come from printing/load.c  */

bool pcap_cache_loaded(time_t *_last_change);
void load_printers(void);

#endif /* _PRINTING_LOAD_H_ */
