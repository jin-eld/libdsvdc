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

#ifndef __LIBDSVDC_COMMON_H__
#define __LIBDSVDC_COMMON_H__

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef HAVE_AVAHI
#include <avahi-client/publish.h>
#include <avahi-common/simple-watch.h>
#endif


#include "dsvdc.h"

/* for some reason -export-symbols-regex had no effect, eventhough the
   contets of the .exp file were correct */
#if __GNUC__ >= 4
    #pragma GCC visibility push(hidden)
#endif

/* search for free ports will start with this value */
#define DEFAULT_VDC_PORT        49500

/* 16k - maximum meaningful size of one message, reject everything bigger */
#define MAX_DATA_SIZE           16384

/* Currently known and supported API version */
#define SUPPORTED_API_VERSION   2

/* length of a dSUID as string (not containing NULL terminator) */
#define DSUID_LENGTH            34

/* max time to wait for a socket to become ready when sending a messsage */
#define SOCKET_WRITE_TIMEOUT    2 /* seconds */

/* defaults for optional parameters */
#define DEFAULT_UNKNOWN_ZONE    -1
#define DEFAULT_UNKNOWN_GROUP   -1

/* minimum value during which we expect the vdSM to answer our messages (in
 * seconds). Actual duration may be longer (depending on the check intervals) */
#define DEFAULT_VDSM_MSG_TIMEOUT 10

/* request id of zero must be ignored, i.e. don't assume it's an answer to
 * a particular request */
#define RESERVED_REQUEST_ID     0

/* how often do we need to iterate the request list in order to dump timed out
 * messages? */
#define REQLIST_CHECK_INTERVAL  20

typedef struct cached_request
{
    struct cached_request *next;
    uint32_t message_id;
    time_t timestamp;
    void *arg;
    void (*callback)(dsvdc_t *handle, int code, void *arg, void *userdata);
} cached_request_t;

/* "instance" structure */
struct dsvdc {
    pthread_mutex_t dsvdc_handle_mutex;
    unsigned short port;
    int listen_fd;
    int connected_fd;
    /* a string for now, will be a byte array later */
    char vdsm_dsuid[DSUID_LENGTH + 1];
    char vdc_dsuid[DSUID_LENGTH + 1];
    char *vdsm_push_uri;

    /* cached requests linked list */
    cached_request_t *requests_list;
    time_t last_list_cleanup;
    uint32_t request_id;

    /* announcements */
#ifdef HAVE_AVAHI
    AvahiEntryGroup *avahi_group;
    AvahiSimplePoll *avahi_poll;
    AvahiClient *avahi_client;
    char *avahi_name;
#endif

    /* callbacks */
    void *callback_userdata;
    void (*vdsm_request_hello)(dsvdc_t *handle, const char *dsuid, void *userdata);
    void (*vdsm_send_ping)(dsvdc_t *handle, const char *dsuid, void *userdata);
    void (*vdsm_send_bye)(dsvdc_t *handle, const char *dsuid, void *userdata);
    bool (*vdsm_send_remove)(dsvdc_t *handle, const char *dsuid,
                             void *userdata);
    void (*vdsm_send_call_scene)(dsvdc_t *handle, char **dsuid,
                                 size_t n_dsuid, int32_t scene, bool force,
                                 int32_t group, int32_t zone_id,
                                 void *userdata);
    void (*vdsm_send_save_scene)(dsvdc_t *handle, char **dsuid,
                                 size_t n_dsuid, int32_t scene, int32_t group,
                                 int32_t zone_id, void *userdata);
    void (*vdsm_send_undo_scene)(dsvdc_t *handle, char **dsuid,
                                 size_t n_dsuid, int32_t scene, int32_t group,
                                 int32_t zone_id, void *userdata);
    void (*vdsm_send_set_local_prio)(dsvdc_t *handle, char **dsuid,
                                 size_t n_dsuid, int32_t scene, int32_t group,
                                 int32_t zone_id, void *userdata);
    void (*vdsm_send_call_min_scene)(dsvdc_t *handle, char **dsuid,
                                 size_t n_dsuid, int32_t group, int32_t zone_id,
                                 void *userdata);
    void (*vdsm_send_identify)(dsvdc_t *handle, char **dsuid,
                                 size_t n_dsuid, int32_t group, int32_t zone_id,
                                 void *userdata);
    void (*vdsm_send_set_control_value)(dsvdc_t *handle, char **dsuid,
                                 size_t n_dsuid, int32_t value, int32_t group,
                                 int32_t zone_id, void *userdata);
    void (*vdsm_request_get_property)(dsvdc_t *handle, const char *dsuid,
                                 dsvdc_property_t *property,
                                 const dsvdc_property_t *query, void *userdata);
    void (*vdsm_request_set_property)(dsvdc_t *handle, const char *dsuid,
                                 dsvdc_property_t *property,
                                 const dsvdc_property_t *properties,
                                 void *userdata);
    void (*vdsm_send_output_channel_value)(dsvdc_t *handle, char **dsuid,
                                 size_t n_dsuid, bool apply,
                                 int32_t channel, double value,
                                 void *userdata);
};

#if __GNUC__ >= 4
    #pragma GCC visibility pop
#endif

#endif/*__LIBDSVDC_COMMON_H__*/

