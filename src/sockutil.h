/*
    Copyright (c) 2009 digitalSTROM.org, Zurich, Switzerland

    Author: Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>

    This file was imported from the "dsplwriter" project.

    libdsvdc is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libdsvdc is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libdsvdc. If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef __SOCKUTIL_H__
#define __SOCKUTIL_H__

enum socket_ops
{
    socket_ok = 0,
    socket_operation_timed_out = -1,
    socket_select_failed = -2,
    socket_recv_failed = -3,
    socket_send_failed = -4,
    socket_error = -5,
    socket_closed = -6
};

int sockread(int sockfd, unsigned char *data, size_t len, int timeout,
             size_t *out_bytes_read);
ssize_t sockwrite(int sockfd, unsigned char *data, size_t len, int timeout);

#endif//__SOCKUTIL_H__
