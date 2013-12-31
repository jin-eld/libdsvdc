/*
    Copyright (c) 2013 aizo ag, Zurich, Switzerland

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>

#include "common.h"
#include "dsvdc.h"
#include "sockutil.h"
#include "msg_processor.h"
#include "messages.pb-c.h"
#include "log.h"
#include "utlist.h"

static int dsvdc_setup_socket(dsvdc_t *handle)
{
    struct sockaddr_in addr;
    int reuse_on = 1;
    int retcode;

    handle->listen_fd = -1;
    handle->connected_fd = -1;

    struct linger
    {
        int l_on;
        int l_linger;
    } linger;

    linger.l_on = 1;
    linger.l_linger = 30;

    handle->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (handle->listen_fd < 0)
    {
        log("could not create socket. %s\n", strerror(errno));
        return DSVDC_ERR_SOCKET;
    }

    retcode = setsockopt(handle->listen_fd, SOL_SOCKET, SO_REUSEADDR,
                         (const char*)&reuse_on, sizeof(int));
    if (retcode < 0)
    {
        close(handle->listen_fd);
        handle->listen_fd = -1;
        log("could not set socket option SO_REUSEDDR. %s\n", strerror(errno));
        return DSVDC_ERR_SOCKET;
    }

    retcode = setsockopt(handle->listen_fd, SOL_SOCKET, SO_LINGER,
                         (const char*)&linger, sizeof(linger));
    if (retcode < 0)
    {
        close(handle->listen_fd);
        handle->listen_fd = -1;
        log("could not set socket option SO_LINGER. %s\n", strerror(errno));
        return DSVDC_ERR_SOCKET;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;

    int portsearch = 0;
    if (handle->port == 0)
    {
        handle->port = DEFAULT_VDC_PORT;
        portsearch = 1;
    }
    log("setting start port to %d\n", handle->port);
    addr.sin_port = htons(handle->port);

    while (1)
    {
        retcode = bind(handle->listen_fd, (struct sockaddr *)&addr,
                       sizeof(addr));
        if (retcode < 0)
        {
            if ((portsearch == 1) && (errno == EADDRINUSE) &&
               (handle->port < (USHRT_MAX - 1)))
            {
                addr.sin_port = htons(++handle->port);
                continue;
            }
        }

        break;
    } /* "while" port search loop */

    if (retcode < 0)
    {
        close(handle->listen_fd);
        handle->listen_fd = -1;
        log("failed to bind to socket. %s\n", strerror(errno));
        return DSVDC_ERR_SOCKET;
    }

    log("bound to port %d\n", handle->port);

    retcode = listen(handle->listen_fd, 1);
    if (retcode < 0)
    {
        close(handle->listen_fd);
        handle->listen_fd = -1;
        log("failed to listen on socket. %s\n", strerror(errno));
        return DSVDC_ERR_SOCKET;
    }

    return DSVDC_OK;
}

int dsvdc_new(unsigned short port, const char *dsuid, void *userdata,
              dsvdc_t **handle)
{
    *handle = NULL;
    static pthread_mutexattr_t attr;


    if (dsuid == NULL)
    {
        log("could not initialise dsvdc instance, missing dsuid parameter\n");
        return DSVDC_ERR_PARAM;
    }

    dsvdc_t *inst = malloc(sizeof(struct dsvdc));
    if (!inst)
    {
        log("could not allocate new library instance\n");
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    /* init members */
    memset(inst->vdsm_dsuid, 0, sizeof(inst->vdsm_dsuid));
    memset(inst->vdc_dsuid, 0, sizeof(inst->vdc_dsuid));
    inst->port = port;
    inst->listen_fd = -1;
    inst->connected_fd = -1;
    inst->vdsm_push_uri = NULL;
    inst->requests_list = NULL;
    inst->last_list_cleanup = time(NULL);
    inst->request_id = 0;
    inst->callback_userdata = userdata;
    inst->vdsm_request_hello = NULL;
    inst->vdsm_send_ping = NULL;
    inst->vdsm_send_bye = NULL;
    inst->vdsm_send_remove = NULL;
    inst->vdsm_send_call_scene = NULL;
    inst->vdsm_send_save_scene = NULL;
    inst->vdsm_send_undo_scene = NULL;
    inst->vdsm_send_set_local_prio = NULL;
    inst->vdsm_send_call_min_scene = NULL;
    inst->vdsm_send_identify = NULL;
    inst->vdsm_send_set_control_value = NULL;
    inst->vdsm_request_get_property = NULL;

    if (pthread_mutexattr_init(&attr) != 0)
    {
        log("could not initialize dsvdc_handle_mutex attribute: %s\n",
            strerror(errno));
        free(inst);
        return DSVDC_ERR_OUT_OF_MEMORY ;
    }

    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
    {
        log("could not set dsvdc_handle_mutex attribute: %s\n",
            strerror(errno));
        pthread_mutexattr_destroy(&attr);
        free(inst);
        return DSVDC_ERR_OUT_OF_MEMORY ;
    }

    if (pthread_mutex_init(&inst->dsvdc_handle_mutex, &attr) != 0)
    {
        log("could not initialize dsvdc_handle_mutex: %s\n", strerror(errno));
        pthread_mutexattr_destroy(&attr);
        free(inst);
        return DSVDC_ERR_OUT_OF_MEMORY ;
    }
        
    pthread_mutexattr_destroy(&attr);

    int ret = dsvdc_setup_socket(inst);
    if (ret != DSVDC_OK)
    {
        pthread_mutex_destroy(&inst->dsvdc_handle_mutex);
        free(inst);
        return ret;
    }

    strncpy(inst->vdc_dsuid, dsuid, DSUID_LENGTH);
    inst->vdc_dsuid[DSUID_LENGTH] = '\0';
    inst->last_list_cleanup = time(NULL);

    *handle = inst;
    return DSVDC_OK;
};

bool dsvdc_is_connected(dsvdc_t *handle)
{
    bool connected;
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    connected = (handle->connected_fd > -1);
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
    return connected;
}

/* walk the requests list and remove obsolete requests */
static void dsvdc_cleanup_request_list(dsvdc_t *handle, bool connected)
{
    cached_request_t *request = NULL;
    int code;

    if (connected)
    {
        code = DSVDC_ERR_TIMEOUT;
    }
    else
    {
        code = DSVDC_ERR_NOT_CONNECTED;
    }

    time_t now = time(NULL);

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if (connected &&
        (difftime(now, handle->last_list_cleanup) < REQLIST_CHECK_INTERVAL))
    {
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
        return;
    }

    LL_FOREACH(handle->requests_list, request)
    {
        if (!connected ||
            (difftime(now, request->timestamp) > DEFAULT_VDSM_MSG_TIMEOUT))
        {
            log("removing request with id %u from list...\n",
                 request->message_id);

            LL_DELETE(handle->requests_list, request);
            request->callback(handle, code, request->arg,
                              handle->callback_userdata);
            free(request);
        }
    }
    handle->last_list_cleanup = time(NULL);
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_work(dsvdc_t *handle, unsigned short timeout)
{
    fd_set rfds;
    int retcode;
    struct sockaddr fsaddr;
    socklen_t fromlen;
    int max_fd;
    bool locked = true;

    if (!handle)
    {
        log("invalid (NULL) handle parameter\n");
        return;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    int connected = (handle->connected_fd >= 0);

    /* if we are connected, respect the timeouts, otherwise wipe the list */
    dsvdc_cleanup_request_list(handle, connected);

    max_fd = handle->connected_fd;

    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(handle->listen_fd, &rfds);
    if (handle->connected_fd > -1)
    {
        FD_SET(handle->connected_fd, &rfds);
    }

    if (timeout != 0)
    {
        tv.tv_sec = timeout;
    }
    else
    {
        tv.tv_sec = 0;
    }

    tv.tv_usec = 0;

    max_fd = (handle->listen_fd > max_fd ? handle->listen_fd : max_fd);

    retcode = select(max_fd + 1, &rfds, NULL, NULL, &tv);

#ifdef DEBUG
        if ((retcode < 0) && (errno != EINTR))
        {
            log("select returned %d: %s\n", retcode, strerror(errno));
        }
#endif

    if (retcode <= 0)
    {
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
        return;
    }

    /* incoming connection */
    if (FD_ISSET(handle->listen_fd, &rfds))
    {
        memset(&fsaddr, 0, sizeof(struct sockaddr));
        memset(&fromlen, 0, sizeof(socklen_t));

        int new_fd = accept(handle->listen_fd, &fsaddr, &fromlen);
        if (new_fd < 0)
        {
            log("could not accept new connection. %s\n", strerror(errno));
        }
        else
        {
            /* we already have an active connection, reject the new one */
            if (handle->connected_fd >= 0)
            {
                log("vDC is already connected to the vdSM, "
                    "rejecting new incoming connection.\n");
                dsvdc_t fake_handle;
                fake_handle.connected_fd = new_fd;
                dsvdc_send_error_message(&fake_handle,
                        VDCAPI__RESULT_CODE__ERR_SERVICE_NOT_AVAILABLE,
                        RESERVED_REQUEST_ID);
                close(new_fd);
            }
            else
            {
                handle->connected_fd = new_fd;
            }
        }
    }

    if (connected && FD_ISSET(handle->connected_fd, &rfds))
    {
        uint16_t size;
        size_t len;
        retcode = sockread(handle->connected_fd,
                            (unsigned char *)(&size),
                             sizeof(uint16_t), timeout, &len);
        /* if data packet length could not be read, or the data is too big (i.e.
         * protocol got out of sync or someone is messing with us), then
         * reset the connection */
        if ((retcode != socket_ok) || (len != sizeof(uint16_t)) ||
           (ntohs(size) > MAX_DATA_SIZE))
        {
            close(handle->connected_fd);
            handle->connected_fd = -1;
            log("could not read incoming message length or "
                "message exceeds allowed size, resetting connection.\n");
            pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
            return;
        }

        size = ntohs(size);
        if (size > 0)
        {
            unsigned char *data = malloc(size);
            if (!data)
            {
                close(handle->connected_fd);
                handle->connected_fd = -1;

                log("could not allocate memory for incoming "
                    "message, resetting connection.\n");
                pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
                return;
            }

            retcode = sockread(handle->connected_fd, data, size, timeout, &len);
            if ((retcode != socket_ok) || ((uint16_t)len != size))
            {
                close(handle->connected_fd);
                handle->connected_fd = -1;
                free(data);

                log("could not read incoming message, resetting connection.\n");
                pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
                return;
            }
            pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
            locked = false;
            dsvdc_process_message(handle, data, size);
            free(data);
        }
    }

    if (locked)
    {
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
    }
}

void dsvdc_set_hello_callback(dsvdc_t *handle,
                        void (*function)(dsvdc_t *handle, void *userdata))
{
    if (!handle)
    {
        return;
    }
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    handle->vdsm_request_hello = function;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_set_ping_callback(dsvdc_t *handle,
                        void (*function)(dsvdc_t *handle, const char *dsuid,
                                         void *userdata))
{
    if (!handle)
    {
        return;
    }
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    handle->vdsm_send_ping = function;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_set_bye_callback(dsvdc_t *handle,
                        void (*function)(dsvdc_t *handle, void *userdata))
{
    if (!handle)
    {
        return;
    }
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    handle->vdsm_send_bye = function;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_set_remove_callback(dsvdc_t *handle,
                        bool (*function)(dsvdc_t *handle, const char *dsuid,
                                         void *userdata))
{
    if (!handle)
    {
        return;
    }
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    handle->vdsm_send_remove = function;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_set_call_scene_notification_callback(dsvdc_t *handle,
                        void (*function)(dsvdc_t *handle, const char *dsuid,
                                         int32_t scene, bool force,
                                         int32_t group, int32_t zone_id,
                                         void *userdata))
{
    if (!handle)
    {
        return;
    }
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    handle->vdsm_send_call_scene = function;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_set_save_scene_notification_callback(dsvdc_t *handle,
                        void (*function)(dsvdc_t *handle, const char *dsuid,
                                         int32_t scene, int32_t group,
                                         int32_t zone_id, void *userdata))
{
    if (!handle)
    {
        return;
    }
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    handle->vdsm_send_save_scene = function;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_set_local_priority_notification_callback(dsvdc_t *handle,
                        void (*function)(dsvdc_t *handle, const char *dsuid,
                                         int32_t scene, int32_t group,
                                         int32_t zone_id, void *userdata))
{
    if (!handle)
    {
        return;
    }
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    handle->vdsm_send_set_local_prio = function;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_set_call_min_scene_notification_callback(dsvdc_t *handle,
                        void (*function)(dsvdc_t *handle, const char *dsuid,
                                         int32_t group, int32_t zone_id,
                                         void *userdata))
{
    if (!handle)
    {
        return;
    }
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    handle->vdsm_send_call_min_scene= function;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_set_identify_notification_callback(dsvdc_t *handle,
                        void (*function)(dsvdc_t *handle, const char *dsuid,
                                         int32_t group, int32_t zone_id,
                                         void *userdata))
{
    if (!handle)
    {
        return;
    }
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    handle->vdsm_send_identify = function;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_set_control_value_callback(dsvdc_t *handle,
                        void (*function)(dsvdc_t *handle, const char *dsuid,
                                         int32_t value, int32_t group,
                                         int32_t zone_id, void *userdata))
{
    if (!handle)
    {
        return;
    }
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    handle->vdsm_send_set_control_value = function;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_set_get_property_callback(dsvdc_t *handle,
                        void (*function)(dsvdc_t *handle, const char *dsuid,
                                         const char *name, uint32_t offset,
                                         uint32_t count,
                                         dsvdc_property_t *property,
                                         void *userdata))
{
    if (!handle)
    {
        return;
    }
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    handle->vdsm_request_get_property = function;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_cleanup(dsvdc_t *handle)
{
    if (!handle)
    {
        return;
    }

    dsvdc_cleanup_request_list(handle, false);

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);

    if (handle->listen_fd > -1)
    {
        close(handle->listen_fd);
        handle->listen_fd = -1;
    }

    if (handle->connected_fd > -1)
    {
        close(handle->connected_fd);
        handle->connected_fd = -1;
    }

    handle->vdsm_request_hello = NULL;
    handle->vdsm_request_get_property = NULL;
    handle->vdsm_send_ping = NULL;
    handle->vdsm_send_bye = NULL;
    handle->vdsm_send_remove = NULL;
    handle->vdsm_send_call_scene = NULL;
    handle->vdsm_send_save_scene = NULL;
    handle->vdsm_send_undo_scene = NULL;
    handle->vdsm_send_set_local_prio = NULL;
    handle->vdsm_send_call_min_scene = NULL;
    handle->vdsm_send_identify = NULL;
    handle->vdsm_send_set_control_value = NULL;
    handle->callback_userdata = NULL;

    if (handle->vdsm_push_uri)
    {
        free(handle->vdsm_push_uri);
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
    pthread_mutex_destroy(&handle->dsvdc_handle_mutex);
    free(handle);
}

