/*
   Unix SMB/Netbios implementation.
   Version 3.0
   printing backend routines
   Copyright (C) Andrew Tridgell 1992-2000
   Copyright (C) Jeremy Allison 2002
   Copyright (C) Simo Sorce 2011

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

#ifndef _SOURCE3_PRINTING_QUEUE_PROCESS_H_
#define _SOURCE3_PRINTING_QUEUE_PROCESS_H_

struct dcesrv_context;

bool printing_subsystem_init(struct tevent_context *ev_ctx,
			     struct messaging_context *msg_ctx,
			     struct dcesrv_context *dce_ctx);
pid_t start_background_queue(struct tevent_context *ev,
			     struct messaging_context *msg,
			     char *logfile);
void send_to_bgqd(struct messaging_context *msg_ctx,
		  uint32_t msg_type,
		  const uint8_t *buf,
		  size_t buflen);

struct bq_state;
struct bq_state *register_printing_bq_handlers(
	TALLOC_CTX *mem_ctx,
	struct messaging_context *msg_ctx);

#endif /* _SOURCE3_PRINTING_QUEUE_PROCESS_H_ */
