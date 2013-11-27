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

#ifndef __LIBDSVDC_H__
#define __LIBDSVDC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct dsvdc dsvdc_t;

enum
{
    DSVDC_OK = 0,                   /*!< no error */

    /* library errors */
    DSVDC_ERR_OUT_OF_MEMORY = -1,   /*!< error allocating memory */
    DSVDC_ERR_SOCKET = -2,          /*!< socket/communication error */
    DSVDC_ERR_NOT_CONNECTED = -3,   /*!< no connection to vdSM */
    DSVDC_ERR_PARAM = -4,           /*!< missing or invalid function parameter*/
    DSVDC_ERR_TIMEOUT = -5,         /*!< vdSM did not response in time */

    /* vDC API errors as received from vdSM (synced with messages.proto) */
    DSVDC_ERR_MESSAGE_UNKNOWN = 1,
    DSVDC_ERR_INCOMPATIBLE_API = 2,
    DSVDC_ERR_SERVICE_NOT_AVAILABLE = 3,
    DSVDC_ERR_INSUFFICIENT_STORAGE = 4,
    DSVDC_ERR_FORBIDDEN = 5,
    DSVDC_ERR_NOT_IMPLEMENTED = 6,
    DSVDC_ERR_NO_CONTENT_FOR_ARRAY = 7,
    DSVDC_ERR_INVALID_VALUE_TYPE = 8,
    DSVDC_ERR_MISSING_SUBMESSAGE = 9,
    DSVDC_ERR_MISSING_DATA = 10
};

/*! \brief Initialize new library instance.
 *  \param[in] port port to listen for incoming vdSM connections. Use zero
 *              for automatic port selection.
 *  \param[in] dsuid dSUID of the vDC.
 *  \param userdata arbitrary user data that will be passed as an argument
 *   to all callback functions.
 *  \param[out] handle newly allocated library handle, must be freeid using
 *              dsvdc_cleanup() when no longer needed.
 *  \return Error/success code.
 */
int dsvdc_new(unsigned short port, const char *dsuid, void *userdata,
              dsvdc_t **handle);

/*! \brief Free library instance and all associated resources.
 *
 * This function must be called when the instance is no longer needed, it frees
 * and invalidates the given handle.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 */
void dsvdc_cleanup(dsvdc_t *handle);

/*! \brief Worker function that must be called periodically.
 *
 * This function handles the actual communication with the vdSM and triggers
 * appropriate callbacks in the library.
 *
 * \param[in] handle dsvdc handle that was returned by dsvdc_new().
 * \param[in] timeout timeout for select on socket in seconds, actually spent
 * time may be two times higher than specified. Use zero for no timeout.
 * If you have an own thread which will just loop on dsvdc_work() then
 * you should use some higher value to avoid unnecessary polling.
 */
void dsvdc_work(dsvdc_t *handle, unsigned short timeout);

/*! \brief Return connection status information if the vDC has an active
 * connection to a vdSM.
 *
 * \param[in] handle dsvdc handle that was returned by dsvdc_new().
 * \return true if there is a connection, false if there is no connection.
 */
bool dsvdc_is_connected(dsvdc_t *handle);

/*! \brief Register "hello" callback.
 *
 * The callback function will be called each time a VDSM_REQUEST_HELLO message
 * is received from the vdSM. Pass NULL for the callback function to unregister
 * the callback. After receiving "hello" you should announce your devices.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, void *userdata) callback function.
 *
 * The callback parameters are:
 * \param[in] handle dsvdc Handle that was returned by dsvdc_new().
 * \param[in] userdata userdata pointer that was passed to dsvdc_new().
 */
void dsvdc_set_hello_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, void *userdata));

/*! \brief Register "bye" callback.
 *
 * The callback function will be called each time a VDSM_SEND_BYE message
 * is received from the vdSM. Pass NULL for the callback function to unregister
 * the callback. After receiving "bye" you can consider the socket closed
 * and you should not attempt to send anything to the vdSM.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, void *userdata) callback function
 *
 * The callback parameters are:
 * \param[in] handle dsvdc handle that was returned by dsvdc_new().
 * \param[in] userdata userdata pointer that was passed to dsvdc_new().
 */
void dsvdc_set_bye_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, void *userdata));

/*! \brief Register "remove" callback.
 *
 * The callback function will be called each time a VDSM_SEND_REMOVE
 * message is received from the vdSM. Pass NULL for the callback function to
 * unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, const char *dsuid) callback
 * function.
 *
 * Callback function must return "true" if removal is allowed or "false"
 * if the device is present and can not be removed.
 */
void dsvdc_set_remove_callback(dsvdc_t *handle,
        bool (*function)(dsvdc_t *handle, const char *dsuid, void *userdata));

/*! \brief Register "ping" callback.
 *
 * The callback function will be called each time a VDSM_SEND_PING
 * message is received from the vdSM. Pass NULL for the callback function to
 * unregister the callback.
 *
 * Ping requests to the vDC itself are handled by the library and a response
 * is sent automatically. Ping requests to vDC devices are forwarded via this
 * callback and must be answered by calling the dsvdc_ping_response() function.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, const char *dsuid) callback
 * function.
 *
 * Callback function must return "true" if removal is allowed or "false"
 * if the device is present and can not be removed.
 */
void dsvdc_set_ping_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, const char *dsuid, void *userdata));

/*! \brief Register "call scene notificatoin" callback.
 *
 * The callback function will be called each time a VDSM_NOTIFICATION_CALL_SCENE
 * message is received from the vdSM. Pass NULL for the callback function to
 * unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, const char *dsuid, int32_t scene,
 *                         int force, int32_t group, int32_t zone_id,
 *                         void *userdata) callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_call_scene_notification_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, const char *dsuid, int32_t scene,
                         bool force, int32_t group, int32_t zone_id,
                         void *userdata));

/*! \brief Register "save scene notificatoin" callback.
 *
 * The callback function will be called each time a VDSM_NOTIFICATION_SAVE_SCENE
 * message is received from the vdSM. Pass NULL for the callback function to
 * unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, const char *dsuid, int32_t scene,
 *                         int32_t group, int32_t zone_id, void *userdata)
 *                         callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_save_scene_notification_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, const char *dsuid, int32_t scene,
                         int32_t group, int32_t zone_id, void *userdata));

/*! \brief Register "undo scene notificatoin" callback.
 *
 * The callback function will be called each time a VDSM_NOTIFICATION_UNDO_SCENE
 * message is received from the vdSM. Pass NULL for the callback function to
 * unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, const char *dsuid, int32_t scene,
 *                         int32_t group, int32_t zone_id, void *userdata)
 *                         callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_undo_scene_notification_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, const char *dsuid, int32_t scene,
                         int32_t group, int32_t zone_id, void *userdata));

/*! \brief Register "set local priority notificatoin" callback.
 *
 * The callback function will be called each time a
 * VDSM_NOTIFICATION_SET_LOCAL_PRIO message is received from the vdSM.
 * Pass NULL for the callback function to unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, const char *dsuid, int32_t scene,
 *                         int32_t group, int32_t zone_id, void *userdata)
 *                         callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_local_priority_notification_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, const char *dsuid, int32_t scene,
                         int32_t group, int32_t zone_id, void *userdata));


/*! \brief Register "set call minimum scene notificatoin" callback.
 *
 * The callback function will be called each time a
 * VDSM_NOTIFICATION_CALL_MIN_SCENE message is received from the vdSM.
 * Pass NULL for the callback function to unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, const char *dsuid,
 *                         int32_t group, int32_t zone_id, void *userdata)
 *                         callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_call_min_scene_notification_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, const char *dsuid, int32_t group,
                         int32_t zone_id, void *userdata));

/*! \brief Register "identify notificatoin" callback.
 *
 * The callback function will be called each time a
 * VDSM_NOTIFICATION_IDENTIFY message is received from the vdSM.
 * Pass NULL for the callback function to unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, const char *dsuid,
 *                         int32_t group, int32_t zone_id, void *userdata)
 *                         callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_identify_notification_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, const char *dsuid, int32_t group,
                         int32_t zone_id, void *userdata));

/*! \brief Register "set control value" callback.
 *
 * The callback function will be called each time a
 * VDSM_NOTIFICATION_SET_CONTROL_VALUE message is received from the vdSM.
 * Pass NULL for the callback function to unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, const char *dsuid, int32_t value,
 *                         int32_t group, int32_t zone_id, void *userdata)
 *                         callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_control_value_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, const char *dsuid, int32_t value,
                         int32_t group, int32_t zone_id, void *userdata));

/*! \brief Send pong reply to a ping request.
 *
 * Each time you receive a ping callback, respond to it using this function.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param dsuid the device identifier that was received in the ping callback.
 * \return error code.
 */
int dsvdc_send_pong(dsvdc_t *handle, const char *dsuid);

/*! \brief Announce a device to the vdSM.
 *
 * Use this function to make your device known to the vdSM. The device must
 * be fully operational and ready to receive commands from the vdSM.
 *
 * The confirmation callback will only be fired if the function returned with
 * DSVDC_OK. In all other cases you can assume that the message was not
 * sent.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param dsuid the device identifier that was received in the ping callback.
 * \param arg arbitrary argument that will be returend to you in the callback.
 * \param void (*function)(dsvdc_t *handle, void *arg, void *userdata) callback
 * function.
 * \return error code.
 */
int dsvdc_announce_device(dsvdc_t *handle, const char *dsuid, void *arg,
                          void (*function)(dsvdc_t *handle, int code, void *arg,
                                           void *userdata));

/*! \brief Notify vdSM that the device has vanished.
 *
 * Use this function to tell the vdSM that your device has been physically
 * disconnected.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param dsuid the device identifier that was received in the ping callback.
 * \return error code, idicating if the message was sent
 */
int dsvdc_device_vanished(dsvdc_t *handle, const char *dsuid);

#ifdef __cplusplus
}
#endif

#endif/*__LIBDSVDC_H__*/
