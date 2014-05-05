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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <utlist.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "common.h"
#include "msg_processor.h"
#include "sockutil.h"
#include "log.h"
#include "properties.h"

int dsvdc_send_message(dsvdc_t *handle, Vdcapi__Message *msg)
{
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if (handle->connected_fd < 0)
    {
        log("not sending message, no open vdSM connection.\n");
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
        return DSVDC_ERR_NOT_CONNECTED;
    }

    size_t msg_len = vdcapi__message__get_packed_size(msg);
    uint8_t *msg_buf = malloc(msg_len * sizeof(uint8_t));
    if (!msg_buf)
    {
        log("could not allocate %zu bytes for protobuf message "
            "serialization.\n", msg_len);
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    vdcapi__message__pack(msg, msg_buf);
    uint16_t netlen = htons(msg_len);
    if (sockwrite(handle->connected_fd, (unsigned char *)&netlen,
                  sizeof(uint16_t), SOCKET_WRITE_TIMEOUT) != sizeof(uint16_t))
    {
        log("could not send message to vdSM\n");
        free(msg_buf);
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
        return DSVDC_ERR_SOCKET;
    }

    int written = sockwrite(handle->connected_fd, (unsigned char *)msg_buf,
                            msg_len, SOCKET_WRITE_TIMEOUT);
    if ((written < 0) || ((size_t)written != msg_len))
    {
        log("could not send message to vdSM\n");
        free(msg_buf);
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
        return DSVDC_ERR_SOCKET;
    }

    free(msg_buf);
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
    return DSVDC_OK;
}

void dsvdc_send_error_message(dsvdc_t *handle, Vdcapi__ResultCode code,
                              uint32_t message_id)
{
    if (!handle)
    {
        return;
    }

    Vdcapi__Message msg = VDCAPI__MESSAGE__INIT;
    Vdcapi__GenericResponse submsg = VDCAPI__GENERIC_RESPONSE__INIT;

    submsg.code = code;

    switch (code)
    {
        case VDCAPI__RESULT_CODE__ERR_OK:
            submsg.description = "OK";
            break;
        case VDCAPI__RESULT_CODE__ERR_MESSAGE_UNKNOWN:
            submsg.description = "Unknown message type";
            break;
        case VDCAPI__RESULT_CODE__ERR_INCOMPATIBLE_API:
            submsg.description = "Incompatible API version";
            break;
        case VDCAPI__RESULT_CODE__ERR_SERVICE_NOT_AVAILABLE:
            submsg.description = "Service not available";
            break;
        case VDCAPI__RESULT_CODE__ERR_INSUFFICIENT_STORAGE:
            submsg.description = "Insufficient storage";
            break;
        case VDCAPI__RESULT_CODE__ERR_FORBIDDEN:
            submsg.description = "Forbidden";
            break;
        case VDCAPI__RESULT_CODE__ERR_NOT_IMPLEMENTED:
            submsg.description = "Not implemented";
            break;
        case VDCAPI__RESULT_CODE__ERR_NO_CONTENT_FOR_ARRAY:
            submsg.description = "No content for array"; /* do we need that? */
            break;
        case VDCAPI__RESULT_CODE__ERR_INVALID_VALUE_TYPE:
            submsg.description = "Invalid or unexpected value type";
            break;
        case VDCAPI__RESULT_CODE__ERR_MISSING_SUBMESSAGE:
            submsg.description = "Missing protocol submessage";
            break;
        case VDCAPI__RESULT_CODE__ERR_MISSING_DATA:
            submsg.description = "Missing data / empty message";
            break;
        case VDCAPI__RESULT_CODE__ERR_NOT_FOUND:
            submsg.description = "Requested entity was not found";
            break;
        case VDCAPI__RESULT_CODE__ERR_NOT_AUTHORIZED:
            submsg.description = "Not authorized to perform requested action";
            break;
        default:
            log("unhandled error code: %d\n", code);
    }

    msg.type = VDCAPI__TYPE__GENERIC_RESPONSE;
    msg.message_id = message_id;
    msg.generic_response = &submsg;
    dsvdc_send_message(handle, &msg);
}

int dsvdc_announce_container(dsvdc_t *handle, const char *dsuid, void *arg,
                             void (*function)(dsvdc_t *handle, int code,
                                              void *arg, void *userdata))
{
    int ret;
    Vdcapi__Message  msg = VDCAPI__MESSAGE__INIT;
    Vdcapi__VdcSendAnnounceVdc submsg = VDCAPI__VDC__SEND_ANNOUNCE_VDC__INIT;

    submsg.dsuid = (char *)dsuid;

    msg.type = VDCAPI__TYPE__VDC_SEND_ANNOUNCE_VDC;

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    msg.message_id = ++(handle->request_id);
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);

    msg.has_message_id = 1;
    msg.vdc_send_announce_vdc = &submsg;

    log("sending VDC_SEND_ANNOUNCE_VDC for container %s\n", dsuid);
    ret = dsvdc_send_message(handle, &msg);
    if (ret == DSVDC_OK)
    {
        log("VDC_SEND_ANNOUNCE_VDC sent, caching id %u for response "
            "tracking\n", msg.message_id);

        cached_request_t *request = malloc(sizeof(cached_request_t));
        if (!request)
        {
            log("VDC_SEND_ANNOUNCE_VDC: could not allocate memory for "
                "cache\n");
            return DSVDC_ERR_OUT_OF_MEMORY;
        }

        request->arg = arg;
        request->callback = (void *)function;
        time(&request->timestamp);
        request->message_id = msg.message_id;
        pthread_mutex_lock(&handle->dsvdc_handle_mutex);
        LL_PREPEND(handle->requests_list, request);
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
    }

    return ret;
}


int dsvdc_announce_device(dsvdc_t *handle, const char *container_dsuid,
                          const char *dsuid, void *arg,
                          void (*function)(dsvdc_t *handle, int code,
                                           void *arg, void *userdata))
{
    int ret;
    Vdcapi__Message  msg = VDCAPI__MESSAGE__INIT;
    Vdcapi__VdcSendAnnounceDevice submsg =
                                        VDCAPI__VDC__SEND_ANNOUNCE_DEVICE__INIT;

    submsg.dsuid = (char *)dsuid;
    submsg.vdcdsuid = (char *)container_dsuid;

    msg.type = VDCAPI__TYPE__VDC_SEND_ANNOUNCE_DEVICE;

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    msg.message_id = ++(handle->request_id);
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);

    msg.has_message_id = 1;
    msg.vdc_send_announce_device = &submsg;

    log("sending VDC_SEND_ANNOUNCE_DEVICE for device %s in container %s\n",
        dsuid, container_dsuid);
    ret = dsvdc_send_message(handle, &msg);
    if (ret == DSVDC_OK)
    {
        log("VDC_SEND_ANNOUNCE_DEVICE sent, caching id %u for response "
            "tracking\n", msg.message_id);

        cached_request_t *request = malloc(sizeof(cached_request_t));
        if (!request)
        {
            log("VDC_SEND_ANNOUNCE_DEVICE: could not allocate memory for "
                "cache\n");
            return DSVDC_ERR_OUT_OF_MEMORY;
        }

        request->arg = arg;
        request->callback = (void *)function;
        time(&request->timestamp);
        request->message_id = msg.message_id;
        pthread_mutex_lock(&handle->dsvdc_handle_mutex);
        LL_PREPEND(handle->requests_list, request);
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
    }

    return ret;
}

int dsvdc_send_pong(dsvdc_t *handle, const char *dsuid)
{
    int ret;

    if (!handle)
    {
        log("can't send pong: invalid handle\n");
        return DSVDC_ERR_PARAM;
    }

    if (!dsuid)
    {
        log("can't send pong: invalid dSUID\n");
        return DSVDC_ERR_PARAM;
    }

    Vdcapi__Message reply = VDCAPI__MESSAGE__INIT;
    Vdcapi__VdcSendPong submsg = VDCAPI__VDC__SEND_PONG__INIT;

    submsg.dsuid = (char *)dsuid;
    reply.type = VDCAPI__TYPE__VDC_SEND_PONG;
    reply.vdc_send_pong = &submsg;

    ret = dsvdc_send_message(handle, &reply);
    log("VDC_SEND_PONG sent with code %d\n", ret);
    return ret;
}

int dsvdc_identify_device(dsvdc_t *handle, const char *dsuid)
{
    int ret;

    if (!handle)
    {
        log("can't send identify message: invalid handle\n");
        return DSVDC_ERR_PARAM;
    }

    if (!dsuid)
    {
        log("can't send identify message: invalid dSUID\n");
        return DSVDC_ERR_PARAM;
    }

    Vdcapi__Message reply = VDCAPI__MESSAGE__INIT;
    Vdcapi__VdcSendIdentify submsg = VDCAPI__VDC__SEND_IDENTIFY__INIT;

    submsg.dsuid = (char *)dsuid;
    reply.type = VDCAPI__TYPE__VDC_SEND_IDENTIFY;
    reply.vdc_send_identify = &submsg;

    ret = dsvdc_send_message(handle, &reply);
    log("VDC_SEND_IDENTIFY sent with code %d\n", ret);
    return ret;
}

int dsvdc_device_vanished(dsvdc_t *handle, const char *dsuid)
{
    int ret;

    if (!handle)
    {
        log("can't send vanish message: invalid handle\n");
        return DSVDC_ERR_PARAM;
    }

    if (!dsuid)
    {
        log("can't send vanish message: invalid dSUID\n");
        return DSVDC_ERR_PARAM;
    }

    Vdcapi__Message reply = VDCAPI__MESSAGE__INIT;
    Vdcapi__VdcSendVanish submsg = VDCAPI__VDC__SEND_VANISH__INIT;

    submsg.dsuid = (char *)dsuid;
    reply.type = VDCAPI__TYPE__VDC_SEND_VANISH;
    reply.vdc_send_vanish = &submsg;

    ret = dsvdc_send_message(handle, &reply);
    log("VDC_SEND_VANISH sent with code %d\n", ret);
    return ret;
}
/* private functions */

static void dsvdc_process_hello(dsvdc_t *handle, Vdcapi__Message *msg)
{
    int ret;
    log("received VDSM_REQUEST_HELLO\n");
    if (!msg->has_message_id)
    {
        log("received VDSM_REQUEST_HELLO: missing message ID!\n");
        return;
    }

    if (!msg->vdsm_request_hello)
    {
        log("received VDSM_REQUEST_HELLO message type, but data is missing!\n");
        return;
    }

    if (!msg->vdsm_request_hello->has_api_version)
    {
        log("received VDSM_REQUEST_HELLO: missing API version information!\n");
        return;
    }

    if (msg->vdsm_request_hello->api_version != SUPPORTED_API_VERSION)
    {
        log("received VDSM_REQUEST_HELLO: unsupported API version %u!\n",
            msg->vdsm_request_hello->api_version);
        return;
    }

    if (!msg->vdsm_request_hello->dsuid)
    {
        log("received VDSM_REQUEST_HELLO: missing dSUID!\n");
        return;
    }

    Vdcapi__Message reply = VDCAPI__MESSAGE__INIT;
    Vdcapi__VdcResponseHello submsg = VDCAPI__VDC__RESPONSE_HELLO__INIT;

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    /* handle's dsuid is already nulled out*/
    strncpy(handle->vdsm_dsuid, msg->vdsm_request_hello->dsuid,
            DSUID_LENGTH);

    submsg.dsuid = handle->vdc_dsuid;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);

    reply.type = VDCAPI__TYPE__VDC_RESPONSE_HELLO;
    reply.message_id = msg->message_id;
    reply.has_message_id = 1;
    reply.vdc_response_hello = &submsg;
    ret = dsvdc_send_message(handle, &reply);
    log("VDC__RESPONSE_HELLO sent with code %d\n", ret);

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if ((ret == DSVDC_OK) && (handle->vdsm_request_hello))
    {
        handle->vdsm_request_hello(handle, handle->callback_userdata);
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

static void dsvdc_process_ping(dsvdc_t *handle, Vdcapi__Message *msg)
{
    log("received VDSM_SEND_PING\n");

    if (!msg->vdsm_send_ping)
    {
        log("received VDSM_SEND_PING message type, but data is missing!\n");
        return;
    }

    if (!msg->vdsm_send_ping->dsuid)
    {
        log("received VDSM_SEND_PING: missing dSUID!\n");
        return;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    /* ping goes out for our vDC, reply automatically */
    if (strncmp(handle->vdc_dsuid, msg->vdsm_send_ping->dsuid,
                DSUID_LENGTH) == 0)
    {
        dsvdc_send_pong(handle, handle->vdc_dsuid);
    }
    else /* ping goes out to a virtual device */
    {
        if (handle->vdsm_send_ping)
        {
            handle->vdsm_send_ping(handle, msg->vdsm_send_ping->dsuid,
                                   handle->callback_userdata);
        }
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

static void dsvdc_process_remove(dsvdc_t *handle, Vdcapi__Message *msg)
{
    log("received VDSM_SEND_REMOVE\n");

    if (!msg->vdsm_send_remove)
    {
        log("received VDSM_SEND_REMOVE message type, but data is missing!\n");
        if (msg->has_message_id)
        {
            dsvdc_send_error_message(handle,
                                     VDCAPI__RESULT_CODE__ERR_MISSING_DATA,
                                     msg->message_id);
        }
        return;
    }

    if (!msg->vdsm_send_remove->dsuid)
    {
        log("received VDSM_SEND_REMOVE: missing dSUID!\n");
        if (msg->has_message_id)
        {
            dsvdc_send_error_message(handle,
                                     VDCAPI__RESULT_CODE__ERR_MISSING_DATA,
                                     msg->message_id);
        }
        return;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);

    if (handle->vdsm_send_remove)
    {
        bool ret = handle->vdsm_send_remove(handle, 
                                            msg->vdsm_send_remove->dsuid,
                                            handle->callback_userdata);
        if (msg->has_message_id)
        {
            dsvdc_send_error_message(handle,
                                ret == true ?
                                VDCAPI__RESULT_CODE__ERR_OK :
                                VDCAPI__RESULT_CODE__ERR_MISSING_DATA,
                                     msg->message_id);
        }
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}


static void dsvdc_process_bye(dsvdc_t *handle, Vdcapi__Message *msg)
{
    log("received VDSM_SEND_BYE\n");

    if (!msg->vdsm_send_bye)
    {
        log("received VDSM_SEND_BYE message type, but data is missing!\n");
        return;
    }

    if (!msg->vdsm_send_bye->dsuid)
    {
        log("received VDSM_SEND_BYE: missing dSUID!\n");
        return;
    }


    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if (handle->connected_fd >= 0)
    {
        close(handle->connected_fd);
        handle->connected_fd = -1;
    }

    if (handle->vdsm_send_bye)
    {
        handle->vdsm_send_bye(handle, msg->vdsm_send_bye->dsuid,
                              handle->callback_userdata);
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

static void dsvdc_process_call_scene(dsvdc_t *handle, Vdcapi__Message *msg)
{
    log("received VDSM_NOTIFICATION_CALL_SCENE\n");
    if (!msg->vdsm_send_call_scene)
    {
        log("received VDSM_NOTIFICATION_CALL_SCENE "
            "message type, but data is missing!\n");
        return;
    }

    if (!msg->vdsm_send_call_scene->dsuid)
    {
        log("received VDSM_NOTIFICATION_CALL_SCENE: missing dSUID!\n");
        return;
    }

    if (!msg->vdsm_send_call_scene->has_scene)
    {
        log("received VDSM_NOTIFICATION_CALL_SCENE: "
            "missing required parameter 'scene'!\n");
        return;
    }

    if (!msg->vdsm_send_call_scene->has_force)
    {
        log("received VDSM_NOTIFICATION_CALL_SCENE: "
            "missing required parameter 'force'!\n");
        return;
    }

    int32_t group = DEFAULT_UNKNOWN_GROUP;
    if (msg->vdsm_send_call_scene->has_group)
    {
        group = msg->vdsm_send_call_scene->group;
    }

    int32_t zone_id = DEFAULT_UNKNOWN_ZONE;
    if (msg->vdsm_send_call_scene->has_zoneid)
    {
        zone_id = msg->vdsm_send_call_scene->zoneid;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if (handle->vdsm_send_call_scene)
    {
        handle->vdsm_send_call_scene(handle,
                msg->vdsm_send_call_scene->dsuid,
                msg->vdsm_send_call_scene->scene,
                msg->vdsm_send_call_scene->force,
                group, zone_id, handle->callback_userdata);
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

static void dsvdc_process_save_scene(dsvdc_t *handle, Vdcapi__Message *msg)
{
    log("received VDSM_NOTIFICATION_SAVE_SCENE\n");

    if (!msg->vdsm_send_save_scene)
    {
        log("received VDSM_NOTIFICATION_SAVE_SCENE "
            "message type, but data is missing!\n");
        return;
    }

    if (!msg->vdsm_send_save_scene->dsuid)
    {
        log("received VDSM_NOTIFICATION_SAVE_SCENE: missing dSUID!\n");
        return;
    }

    if (!msg->vdsm_send_save_scene->has_scene)
    {
        log("received VDSM_NOTIFICATION_SAVE_SCENE: "
                "missing required parameter 'scene'!\n");
        return;
    }

    int32_t group = DEFAULT_UNKNOWN_GROUP;
    if (msg->vdsm_send_save_scene->has_group)
    {
        group = msg->vdsm_send_save_scene->group;
    }

    int32_t zone_id = DEFAULT_UNKNOWN_ZONE;
    if (msg->vdsm_send_save_scene->has_zoneid)
    {
        zone_id = msg->vdsm_send_save_scene->zoneid;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if (handle->vdsm_send_save_scene)
    {
        handle->vdsm_send_save_scene(handle,
                msg->vdsm_send_save_scene->dsuid,
                msg->vdsm_send_save_scene->scene,
                group, zone_id, handle->callback_userdata);
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

static void dsvdc_process_undo_scene(dsvdc_t *handle, Vdcapi__Message *msg)
{
    log("received VDSM_NOTIFICATION_UNDO_SCENE\n");
    if (!msg->vdsm_send_undo_scene)
    {
        log("received VDSM_NOTIFICATION_UNDO_SCENE "
            "message type, but data is missing!\n");
        return;
    }

    if (!msg->vdsm_send_undo_scene->dsuid)
    {
        log("received VDSM_NOTIFICATION_UNDO_SCENE: missing dSUID!\n");
        return;
    }

    if (!msg->vdsm_send_undo_scene->has_scene)
    {
        log("received VDSM_NOTIFICATION_UNDO_SCENE: "
                "missing required parameter 'scene'!\n");
        return;
    }

    int32_t group = DEFAULT_UNKNOWN_GROUP;
    if (msg->vdsm_send_undo_scene->has_group)
    {
        group = msg->vdsm_send_undo_scene->group;
    }

    int32_t zone_id = DEFAULT_UNKNOWN_ZONE;
    if (msg->vdsm_send_undo_scene->has_zoneid)
    {
        zone_id = msg->vdsm_send_undo_scene->zoneid;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if (handle->vdsm_send_undo_scene)
    {
        handle->vdsm_send_undo_scene(handle,
                msg->vdsm_send_undo_scene->dsuid,
                msg->vdsm_send_undo_scene->scene,
                group, zone_id, handle->callback_userdata);
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

static void dsvdc_process_set_local_prio(dsvdc_t *handle, Vdcapi__Message *msg)
{
    log("received VDSM_NOTIFICATION_SET_LOCAL_PRIO\n");

    if (!msg->vdsm_send_set_local_prio)
    {
        log("received VDSM_NOTIFICATION_SET_LOCAL_PRIO: "
            "message type, but data is missing!\n");
        return;
    }

    if (!msg->vdsm_send_set_local_prio->dsuid)
    {
        log("received VDSM_NOTIFICATION_SET_LOCAL_PRIO: missing dSUID!\n");
        return;
    }

    if (!msg->vdsm_send_set_local_prio->has_scene)
    {
        log("received VDSM_NOTIFICATION_SET_LOCAL_PRIO: "
            "missing required parameter 'scene'!\n");
        return;
    }

    int32_t group = DEFAULT_UNKNOWN_GROUP;
    if (msg->vdsm_send_set_local_prio->has_group)
    {
        group = msg->vdsm_send_set_local_prio->group;
    }

    int32_t zone_id = DEFAULT_UNKNOWN_ZONE;
    if (msg->vdsm_send_set_local_prio->has_zoneid)
    {
        zone_id = msg->vdsm_send_set_local_prio->zoneid;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if (handle->vdsm_send_set_local_prio)
    {
        handle->vdsm_send_set_local_prio(handle,
                msg->vdsm_send_set_local_prio->dsuid,
                msg->vdsm_send_set_local_prio->scene,
                group, zone_id, handle->callback_userdata);
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

static void dsvdc_process_call_min_scene(dsvdc_t *handle, Vdcapi__Message *msg)
{
    log("received VDSM_NOTIFICATION_CALL_MIN_SCENE\n");

    if (!msg->vdsm_send_call_min_scene)
    {
        log("received VDSM_NOTIFICATION_CALL_MIN_SCENE: message type, but "
            "data is missing!\n");
        return;
    }

    if (!msg->vdsm_send_call_min_scene->dsuid)
    {
        log("received VDSM_NOTIFICATION_CALL_MIN_SCENE: missing dSUID!\n");
        return;
    }

    int32_t group = DEFAULT_UNKNOWN_GROUP;
    if (msg->vdsm_send_call_min_scene->has_group)
    {
        group = msg->vdsm_send_call_min_scene->group;
    }

    int32_t zone_id = DEFAULT_UNKNOWN_ZONE;
    if (msg->vdsm_send_call_min_scene->has_zoneid)
    {
        zone_id = msg->vdsm_send_call_min_scene->zoneid;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if (handle->vdsm_send_call_min_scene)
    {
        handle->vdsm_send_call_min_scene(handle,
                msg->vdsm_send_call_min_scene->dsuid,
                group, zone_id, handle->callback_userdata);
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

static void dsvdc_process_identify(dsvdc_t *handle, Vdcapi__Message *msg)
{
    log("received VDSM_NOTIFICATION_IDENTIFY\n");
    if (!msg->vdsm_send_identify)
    {
        log("received VDSM_NOTIFICATION_IDENTIFY: message type, but data is "
            "missing!\n");
        return;
    }

    if (!msg->vdsm_send_identify->dsuid)
    {
        log("received VDSM_NOTIFICATION_IDENTIFY: missing dSUID!\n");
        return;
    }

    int32_t group = DEFAULT_UNKNOWN_GROUP;
    if (msg->vdsm_send_identify->has_group)
    {
        group = msg->vdsm_send_identify->group;
    }

    int32_t zone_id = DEFAULT_UNKNOWN_ZONE;
    if (msg->vdsm_send_identify->has_zoneid)
    {
        zone_id = msg->vdsm_send_identify->zoneid;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if (handle->vdsm_send_identify)
    {
        handle->vdsm_send_identify(handle,
                msg->vdsm_send_identify->dsuid,
                group, zone_id, handle->callback_userdata);
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

static void dsvdc_process_set_control_value(dsvdc_t *handle,
                                            Vdcapi__Message *msg)
{
    log("received VDSM_NOTIFICATION_SET_CONTROL_VALUE\n");

    if (!msg->vdsm_send_set_control_value)
    {
        log("received VDSM_NOTIFICATION_SET_CONTROL_VALUE message type, but "
            "data is missing!\n");
        return;
    }

    if (!msg->vdsm_send_set_control_value->dsuid)
    {
        log("received VDSM_NOTIFICATION_SET_CONTROL_VALUE: missing dSUID!\n");
        return;
    }

    if (!msg->vdsm_send_set_control_value->has_value)
    {
        log("received VDSM_NOTIFICATION_SET_CONTROL_VALUE: missing value!\n");
        return;
    }

    int32_t group = DEFAULT_UNKNOWN_GROUP;
    if (msg->vdsm_send_set_control_value->has_group)
    {
        group = msg->vdsm_send_set_control_value->group;
    }

    int32_t zone_id = DEFAULT_UNKNOWN_ZONE;
    if (msg->vdsm_send_set_control_value->has_zoneid)
    {
        zone_id = msg->vdsm_send_set_control_value->zoneid;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if (handle->vdsm_send_set_control_value)
    {
        handle->vdsm_send_set_control_value(handle,
                msg->vdsm_send_set_control_value->dsuid,
                msg->vdsm_send_set_control_value->value,
                group, zone_id, handle->callback_userdata);
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

static void dsvdc_process_get_property(dsvdc_t *handle, Vdcapi__Message *msg)
{
    log("received VDSM_REQUEST_GET_PROPERTY\n");

    uint32_t offset = 0;
    uint32_t count = 0;

    if (!msg->vdsm_request_get_property)
    {
        log("received VDSM_REQUEST_GET_PROPERTY message type, but data is "
            "missing!\n");
        return;
    }

    if (!msg->vdsm_request_get_property->dsuid)
    {
        log("received VDSM_REQUEST_GET_PROPERTY: missing dSUID!\n");
        return;
    }

    if (!msg->vdsm_request_get_property->name)
    {
        log("received VDSM_REQUEST_GET_PROPERTY: missing property name!\n");
        return;
    }

    if (msg->vdsm_request_get_property->has_offset)
    {
        offset = msg->vdsm_request_get_property->offset;
    }

    if (msg->vdsm_request_get_property->has_count)
    {
        count = msg->vdsm_request_get_property->count;
    }

    log("VDSM_REQUEST_GET_PROPERTY/%u: dSUID[ %s], name[ %s ], index[ %u ], "
        "count[ %u ]\n", msg->message_id, msg->vdsm_request_get_property->dsuid,
        msg->vdsm_request_get_property->name, offset, count);

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if (handle->vdsm_request_get_property)
    {
        dsvdc_property_t *property = NULL;
        int ret = dsvdc_property_new(msg->vdsm_request_get_property->name, 0,
                                     &property);
        if (ret == DSVDC_OK)
        {
            property->message_id = msg->message_id;
        }
        else
        {
            log("VDSM_REQUEST_GET_PROPERTY: could not allocave new property, "
                "error code: %d\n", ret);
        }
        handle->vdsm_request_get_property(handle,
                msg->vdsm_request_get_property->dsuid,
                msg->vdsm_request_get_property->name,
                offset, count, property, handle->callback_userdata);
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

static cached_request_t *dsvdc_get_cached_request(dsvdc_t *handle, uint32_t id)
{
    cached_request_t *request = NULL;
    cached_request_t *tmp = NULL;


    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    LL_FOREACH_SAFE(handle->requests_list, request, tmp)
    {
        if (request->message_id == id)
        {
            LL_DELETE(handle->requests_list, request);
            pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
            return request;
        }
    }

    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
    return NULL;
}

static void dsvdc_process_generic_response(dsvdc_t *handle,
                                           Vdcapi__Message *msg)
{
    log("received GENERIC_RESPONSE\n");
    cached_request_t *request;

    if (!msg->generic_response)
    {
        log("received GENERIC_RESPONSE message type, but data is missing!\n");
        return;
    }

    if (!msg->has_message_id ||
        (msg->has_message_id && (msg->message_id == RESERVED_REQUEST_ID)))
    {
        log("GENERIC_RESPONSE with code [%d/%s] is not cached\n",
            msg->generic_response->code, msg->generic_response->description);
        return;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    request = dsvdc_get_cached_request(handle, msg->message_id);
    if (request)
    {
        log("found matching request with id %u in cache\n",
            request->message_id);
        request->callback(handle, msg->generic_response->code,
                          request->arg, handle->callback_userdata);
        free(request);
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_process_message(dsvdc_t *handle, unsigned char *data, uint16_t len)
{
    Vdcapi__Message *msg = vdcapi__message__unpack(NULL, len, data);
    if (!msg)
    {
        log("failed to unpack incoming message\n");
        return;
    }

    switch (msg->type)
    {
        case VDCAPI__TYPE__VDSM_REQUEST_HELLO:
            dsvdc_process_hello(handle, msg);
            break;

        case VDCAPI__TYPE__VDSM_REQUEST_GET_PROPERTY:
            dsvdc_process_get_property(handle, msg);
            break;

        case VDCAPI__TYPE__VDSM_SEND_PING:
            dsvdc_process_ping(handle, msg);
            break;

        case VDCAPI__TYPE__VDSM_REQUEST_SET_PROPERTY:
            dsvdc_send_error_message(handle,
                VDCAPI__RESULT_CODE__ERR_NOT_IMPLEMENTED, msg->message_id);
            break;

        case VDCAPI__TYPE__VDSM_SEND_REMOVE:
        {
            log("received VDSM_SEND_REMOVE\n");
            dsvdc_process_remove(handle, msg);
            break;
        }
        case VDCAPI__TYPE__VDSM_SEND_BYE:
            dsvdc_process_bye(handle, msg);
            break;

        case VDCAPI__TYPE__VDSM_NOTIFICATION_CALL_SCENE:
            dsvdc_process_call_scene(handle, msg);
            break;

        case VDCAPI__TYPE__VDSM_NOTIFICATION_SAVE_SCENE:
            dsvdc_process_save_scene(handle, msg);
            break;

        case VDCAPI__TYPE__VDSM_NOTIFICATION_UNDO_SCENE:
            dsvdc_process_undo_scene(handle, msg);
            break;

        case VDCAPI__TYPE__VDSM_NOTIFICATION_SET_LOCAL_PRIO:
            dsvdc_process_set_local_prio(handle, msg);
            break;

        case VDCAPI__TYPE__VDSM_NOTIFICATION_CALL_MIN_SCENE:
            dsvdc_process_call_min_scene(handle, msg);
            break;

        case VDCAPI__TYPE__VDSM_NOTIFICATION_IDENTIFY:
            dsvdc_process_identify(handle, msg);
            break;

        case VDCAPI__TYPE__VDSM_NOTIFICATION_SET_CONTROL_VALUE:
            dsvdc_process_set_control_value(handle, msg);
            break;

        case VDCAPI__TYPE__GENERIC_RESPONSE:
            dsvdc_process_generic_response(handle, msg);
            break;

        default:
            log("unhandled message type %d\n", msg->type);
            break;
    }
    vdcapi__message__free_unpacked(msg, NULL);
}

