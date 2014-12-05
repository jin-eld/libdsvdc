/*
    Copyright (c) 2009 digitalSTROM.org, Zurich, Switzerland
    Copyright (c) 2013 digitalSTROM AG, Zurich, Switzerland

    Author: Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>

    This file is derived from libcommchannel, partially imported from the 
    dsplwriter project.

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

#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sockutil.h"
#include "log.h"

#if __GNUC__ >= 4
    #pragma GCC visibility push(hidden)
#endif

int sockread(int sockfd, unsigned char *data, size_t len, int timeout,
             size_t *out_bytes_read)
{
    int ret;
    struct timeval tv;
    fd_set rfds;
    unsigned char *p = data;
    ssize_t bytes_total = 0;
    ssize_t bytes_read = 0;
    int eod = 0;
    if (out_bytes_read)
    {
        *out_bytes_read = 0;
    }
 
    while (1)
    {
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);

        if (timeout != 0)
        {
            if (!eod)
            {
                tv.tv_sec = timeout;
                tv.tv_usec = 0;
            }
            else
            {
                tv.tv_sec = 0;
                tv.tv_usec = 0;
            }
            ret = select(sockfd + 1, &rfds, NULL, NULL, &tv);
        }
        else
        {
            ret = select(sockfd + 1, &rfds, NULL, NULL, NULL);
        }

        if (ret == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            log("failed to select on socket: %s\n", strerror(errno));
            if (out_bytes_read)
            {
                *out_bytes_read = bytes_total;
            }
            return socket_select_failed;
        }

        if (ret == 0)
        {
            if (out_bytes_read)
            {
                *out_bytes_read = bytes_total;
            }

            if (eod)
            {
                return socket_ok;
            }
            else
            {
                return socket_operation_timed_out;
            }
        }

        if (FD_ISSET(sockfd, &rfds))
        {
            if (timeout)
            {
                eod = 1;
            }

            bytes_read = recv(sockfd, p, len, MSG_NOSIGNAL);

            if (bytes_read < 0)
            {
                if (out_bytes_read)
                {
                    *out_bytes_read = bytes_total;
                }

                log("recv failed: %s\n", strerror(errno));
                return socket_recv_failed;
            }

            // remote socket was closed
            if (bytes_read == 0)
            {
                if (out_bytes_read)
                {
                    *out_bytes_read = bytes_total;
                }

                return socket_closed;
            }

            bytes_total += bytes_read;
            len -= bytes_read;

            if (len <= 0)
            {
                break;
            }

            p = data + bytes_total;
        }
    }

    if (bytes_total < 0)
    {
        return socket_error;
    }

    if (out_bytes_read)
    {
        *out_bytes_read = bytes_total;
    }

    return socket_ok;
}

ssize_t sockwrite(int sockfd, unsigned char *data, size_t len, int timeout)
{
    int ret;
    struct timeval tv;
    fd_set wfds;
    unsigned char *p = data;
    ssize_t bytes_total = 0;
    ssize_t bytes_sent = 0;

    while (1)
    {
        FD_ZERO(&wfds);
        FD_SET(sockfd, &wfds);

        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        ret = select(sockfd + 1, NULL, &wfds, NULL, &tv);

        if (ret == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            log("Failed to read from socket: %s\n", strerror(errno));
            return socket_select_failed;
        }

        if (ret == 0)
        {
            return socket_operation_timed_out;
        }

        if (FD_ISSET(sockfd, &wfds))
        {
            bytes_sent = send(sockfd, p, len, MSG_NOSIGNAL);

            if (bytes_sent < 0)
            {
                return socket_send_failed;
            }

            if (bytes_sent == 0)
            {
                break;
            }

            bytes_total += bytes_sent;
            len -= bytes_sent;

            if (len <= 0)
            {
                break;
            }

            p = data + bytes_total;
        }
    }

    if (bytes_total < 0)
    {
        return socket_error;
    }

    return bytes_total;
}

#if __GNUC__ >= 4
    #pragma GCC visibility pop
#endif

