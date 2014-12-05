/*
    Copyright (c) 2013 digitalSTROM AG, Zurich, Switzerland

    Author: Sergey 'Jin' Bostandzhyan <jin@dev.digitalstrom.org>

    This file is part of libdSvDC.

    libdsvdc is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libdsvdc is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with digitalSTROM Server. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __LIBDSVDC_MSG_PROCESSOR_H__
#define __LIBDSVDC_MSG_PROCESSOR_H__

#include <stdint.h>
#include <stdlib.h>

#include "messages.pb-c.h"

#if __GNUC__ >= 4
    #pragma GCC visibility push(hidden)
#endif

/* sends a message to the vdSM via the connection socket in the handle */
int dsvdc_send_message(dsvdc_t *handle, Vdcapi__Message *msg);

/* sends "generic response" type message with the given error code */
void dsvdc_send_error_message(dsvdc_t *handle, Vdcapi__ResultCode code,
                              uint32_t message_id);

/* Receives data buffer containing the protobuf message, attempts to decode it.
 * identifies the message and triggers appropriate callbacks or responses.  */
void dsvdc_process_message(dsvdc_t *handle, unsigned char *data, uint16_t len);

#if __GNUC__ >= 4
    #pragma GCC visibility pop
#endif


#endif/*__LIBDSVDC_MSG_PROCESSOR_H__*/

