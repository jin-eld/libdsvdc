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

/*! \file dsvdc.h
 *  \brief libdSVDC library interface.
 * */

#ifndef __LIBDSVDC_H__
#define __LIBDSVDC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct dsvdc dsvdc_t;
typedef struct dsvdc_property dsvdc_property_t;
enum
{
    DSVDC_OK = 0,                   /*!< no error */

    /* library errors */
    DSVDC_ERR_OUT_OF_MEMORY = -1,   /*!< error allocating memory */
    DSVDC_ERR_SOCKET = -2,          /*!< socket/communication error */
    DSVDC_ERR_NOT_CONNECTED = -3,   /*!< no connection to vdSM */
    DSVDC_ERR_PARAM = -4,           /*!< missing or invalid function parameter*/
    DSVDC_ERR_TIMEOUT = -5,         /*!< vdSM did not response in time */
    DSVDC_ERR_PROPERTY_INDEX = -6,  /*!< no property under given index */
    DSVDC_ERR_INVALID_PROPERTY = -7,/*!< invalid property structure */
    DSVDC_ERR_DISCOVERY = -8,       /*!< could not set up Avahi */
    DSVDC_ERR_PROPERTY_VALUE_TYPE = -9, /*!< property value type mismatch */

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
 *  \param[in] name name of this instance for Avahi announcements, only relevant
 *   if the library was compiled with Avahi support. Pass NULL if you want the
 *   name to be chosen for you (default name is "digitalSTROM vDC".
 *  \param[in] userdata arbitrary user data that will be passed as an argument
 *   to all callback functions.
 *  \param[out] handle newly allocated library handle, must be freeid using
 *              dsvdc_cleanup() when no longer needed.
 *  \return Error/success code.
 */
int dsvdc_new(unsigned short port, const char *dsuid, const char *name,
              void *userdata, dsvdc_t **handle);

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
 * the callback. After receiving "hello" you should announce your vdc's and
 * devices.
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
 * \param void (*function)(dsvdc_t *handle, const char *dsuid, void *userdata)
 * callback function
 *
 * The callback parameters are:
 * \param[in] handle dsvdc handle that was returned by dsvdc_new().
 * \param[in] userdata userdata pointer that was passed to dsvdc_new().
 */
void dsvdc_set_bye_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, const char *dsuid, void *userdata));

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
 * \param void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
 *                         int32_t scene, int force, int32_t group,
 *                         int32_t zone_id, void *userdata) callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_call_scene_notification_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
                         int32_t scene, bool force, int32_t group,
                         int32_t zone_id, void *userdata));

/*! \brief Register "save scene notificatoin" callback.
 *
 * The callback function will be called each time a VDSM_NOTIFICATION_SAVE_SCENE
 * message is received from the vdSM. Pass NULL for the callback function to
 * unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
 *                         int32_t scene, int32_t group, int32_t zone_id,
 *                         void *userdata) callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_save_scene_notification_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
                         int32_t scene, int32_t group, int32_t zone_id,
                         void *userdata));

/*! \brief Register "undo scene notificatoin" callback.
 *
 * The callback function will be called each time a VDSM_NOTIFICATION_UNDO_SCENE
 * message is received from the vdSM. Pass NULL for the callback function to
 * unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
 *                         int32_t scene, int32_t group, int32_t zone_id,
 *                         void *userdata) callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_undo_scene_notification_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
                         int32_t scene, int32_t group, int32_t zone_id,
                         void *userdata));

/*! \brief Register "set local priority notificatoin" callback.
 *
 * The callback function will be called each time a
 * VDSM_NOTIFICATION_SET_LOCAL_PRIO message is received from the vdSM.
 * Pass NULL for the callback function to unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
 *                         int32_t scene, int32_t group, int32_t zone_id,
 *                         void *userdata) callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_local_priority_notification_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
                         int32_t scene, int32_t group, int32_t zone_id,
                         void *userdata));


/*! \brief Register "set call minimum scene notificatoin" callback.
 *
 * The callback function will be called each time a
 * VDSM_NOTIFICATION_CALL_MIN_SCENE message is received from the vdSM.
 * Pass NULL for the callback function to unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
 *                         int32_t group, int32_t zone_id, void *userdata)
 *                         callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_call_min_scene_notification_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
                         int32_t group, int32_t zone_id, void *userdata));

/*! \brief Register "identify notificatoin" callback.
 *
 * The callback function will be called each time a
 * VDSM_NOTIFICATION_IDENTIFY message is received from the vdSM.
 * Pass NULL for the callback function to unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
 *                         int32_t group, int32_t zone_id, void *userdata)
 *                         callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_identify_notification_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
                         int32_t group, int32_t zone_id, void *userdata));

/*! \brief Register "set control value" callback.
 *
 * The callback function will be called each time a
 * VDSM_NOTIFICATION_SET_CONTROL_VALUE message is received from the vdSM.
 * Pass NULL for the callback function to unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
 *                         int32_t value, int32_t group, int32_t zone_id,
 *                         void *userdata) callback function.
 *
 * Callback parameters group and zone_id are optional and will have the values
 * of -1 if not set.
 */
void dsvdc_set_control_value_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
                         int32_t value, int32_t group, int32_t zone_id,
                         void *userdata));

/*! \brief Register "get property" callback.
 *
 * The callback function will be called each time a
 * VDSM_REQUEST_GET_PROPERTY message is received from the vdSM.
 * Pass NULL for the callback function to unregister the callback.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param void (*function)(dsvdc_t *handle, const char *dsuid, const char *name,
 *                         uint32_t index, uint32_t offset, void *userdata)
 *                         callback function.
 */
void dsvdc_set_get_property_callback(dsvdc_t *handle,
        void (*function)(dsvdc_t *handle, const char *dsuid,
                         dsvdc_property_t *property,
                         const dsvdc_property_t *query,
                         void *userdata));

/*! \brief Send pong reply to a ping request.
 *
 * Each time you receive a ping callback, respond to it using this function.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param dsuid the device identifier that was received in the ping callback.
 * \return error code.
 */
int dsvdc_send_pong(dsvdc_t *handle, const char *dsuid);

/*! \brief Announce a virtual device container to the vdSM.
 *
 * Use this function to make your device container known to the vdSM.
 *
 * The confirmation callback will only be fired if the function returned with
 * DSVDC_OK. In all other cases you can assume that the message was not
 * sent.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param dsuid the vdc identifier that.
 * \param arg arbitrary argument that will be returend to you in the callback.
 * \param void (*function)(dsvdc_t *handle, void *arg, void *userdata) callback
 * function.
 * \return error code.
 */
int dsvdc_announce_container(dsvdc_t *handle, const char *dsuid, void *arg,
                             void (*function)(dsvdc_t *handle, int code,
                             void *arg, void *userdata));

/*! \brief Announce a device to the vdSM.
 *
 * Use this function to make your device known to the vdSM. The device must
 * be fully operational and ready to receive commands from the vdSM. According
 * to the specification the device must reside within a container.
 *
 * The confirmation callback will only be fired if the function returned with
 * DSVDC_OK. In all other cases you can assume that the message was not
 * sent.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param container_dsuid the device container identifier.
 * \param dsuid the device identifier.
 * \param arg arbitrary argument that will be returend to you in the callback.
 * \param void (*function)(dsvdc_t *handle, void *arg, void *userdata) callback
 * function.
 * \return error code.
 */
int dsvdc_announce_device(dsvdc_t *handle, const char *container_dsuid,
                          const char *dsuid, void *arg,
                          void (*function)(dsvdc_t *handle, int code, void *arg,
                                           void *userdata));

/*! \brief Notify vdSM that the device has vanished.
 *
 * Use this function to tell the vdSM that your device has been physically
 * disconnected.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param dsuid the device identifier that was received in the ping callback.
 * \return error code, indicating if the message was sent.
 */
int dsvdc_device_vanished(dsvdc_t *handle, const char *dsuid);

/*! \brief Allow devices to identify themselves to the vdSM.
 *
 * This is similar to the identify callback that is sent in response to an
 * identificatoin request. The difference is that in this case the device can
 * identify itself by its own choice, without a previous request from the vdSM.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param dsuid the device identifier.
 * \return error code.
 */
int dsvdc_identify_device(dsvdc_t *handle, const char *dsuid);

/*! \brief Send a response to get property request to the vdSM.
 *
 * Use this function to reply to the vdSM's get property request. Make sure
 * to use the same property handle that was allocated for you by the
 * get property callback. This is important, because the response needs to
 * match the request and the library takes care of this by automatically
 * setting the appropriate responde message id, this is also the reason why
 * there is no dSUID parameter.
 *
 * \note IMPORTANT: this function will free your property handle, no matter if
 * sending succeeded or not. You don't have a second chance if you constructed
 * an invalid property (which would be a coding mistake on your side), if
 * a property is invalid an error response message will be automatically sent
 * to the vdSM.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param property property which will be sent and freed.
 * \return error code, indicating if the message was sent.
 */
int dsvdc_send_property_response(dsvdc_t *handle, dsvdc_property_t *property);

/*! \brief Push property to vdSM.
 *
 * Some properties (especially button/input/sensor states) might change within
 * the device and can be reported to the dS system via push property, avoiding
 * the need for the vdSM to poll values.
 *
 * \param handle dsvdc handle that was returned by dsvdc_new().
 * \param name property name.
 * \param offset property offset in the target first level array.
 * \param property property which will be sent, this property can be reused
 * and thus will not be freed by this function. If you do not need your property
 * anymore you must free it using dsvdc_property_free().
 */
int dsvdc_push_property(dsvdc_t *handle, const char *dsuid,
                        dsvdc_property_t *property);
/*
 * ****************************************************************************
 * Property functions.
 * ****************************************************************************
 */

/*! \brief Property value types as returned by the
 *  dsvdc_property_get_value_type() function. This allows you to determine
 *  which value is stored within the given property so that you can use
 *  the appropriate property_get_xxx() function.
 */
typedef enum
{
    DSVDC_PROPERTY_VALUE_NONE,      /*!< property does not have а value */
    DSVDC_PROPERTY_VALUE_BOOL,      /*!< property value is of type boolean */
    DSVDC_PROPERTY_VALUE_UINT64,    /*!< property value is a uint64_t */
    DSVDC_PROPERTY_VALUE_INT64,     /*!< property value is an int64_t */
    DSVDC_PROPERTY_VALUE_DOUBLE,    /*!< property value is a double */
    DSVDC_PROPERTY_VALUE_STRING,    /*!< property value is a char* */
    DSVDC_PROPERTY_VALUE_BYTES      /*!< property value is a uint8_t* */
} dsvdc_property_value_t;


/*! \brief Allocate new property for further manipulation.
 *
 * Use this function to allocate a new property which can be used for
 * push property notifications. Make sure to free your property using the
 * dsvdc_property_free() function when you no longer need it.
 *
 * \note All property helper functions that manipulate properties
 * are NOT THREADSAFE!
 *
 * \param[out] property the newly allocated property.
 * \return error code, indicating if the property was allocated.
 */

int dsvdc_property_new(dsvdc_property_t **property);

/*! \brief Free previously allocated property.
 *
 * Use this function to free the resources for a property that has been
 * allocated by the dsvdc_property_new() function.
 *
 * \note The dsvdc_get_property_callback() function will give you an
 * allocated property which you are supposed to give back as a parameter to
 * the dsvdc_device_property_response() function. When doing so, the property
 * gets freed and invalidated by dsvdc_device_property_response().
 *
 * In situations where you receive a property request, but do not want to
 * respond and thus do not call the dsvdc_device_property_response function,
 * you have to free the property that you received in the callback yourself,
 * by calling dsvdc_property_free().
 *
 * \param[in] property that should be freed.
 */
void dsvdc_property_free(dsvdc_property_t *property);

/*!\brief Add an integer element to a property.
 *
 * \param[in] property property handle
 * \param[in] key name of the property element
 * \param[in] value value of the property element
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_add_int(dsvdc_property_t *property, const char *key,
                           int64_t value);

/*!\brief Add an unsigned integer element to a property.
 *
 * \param[in] property property handle
 * \param[in] key name of the property element
 * \param[in] value value of the property element
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_add_uint(dsvdc_property_t *property, const char *key,
                            uint64_t value);

/*!\brief Add a boolean element to a property.
 *
 * \param[in] property property handle
 * \param[in] key name of the property element
 * \param[in] value value of the property element
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_add_bool(dsvdc_property_t *property, const char *key,
                            bool value);

/*!\brief Add a double element to a property.
 *
 * \param[in] property property handle
 * \param[in] key name of the property element
 * \param[in] value value of the property element
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_add_double(dsvdc_property_t *property, const char *key,
                              double value);

/*!\brief Add a string element to a property.
 *
 * \param[in] property property handle
 * \param[in] key name of the property element
 * \param[in] value value of the property element
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_add_string(dsvdc_property_t *property, const char *key,
                              const char *value);

/*!\brief Add bytes element to a property.
 *
 * \param[in] property property handle
 * \param[in] key name of the property element
 * \param[in] value value of the property element (byte array)
 * \param[in] length length of the byte array
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_add_bytes(dsvdc_property_t *property, const char *key,
                             const uint8_t *value, const size_t length);

/*!\brief Add a property to a property.
 *
 * This allows you to construct nested properties. This function takes
 * ownership of your value pointer, you must not access it anymore after
 * it has been passed to this function. The data below the value parameter
 * will be freed together with the property to which it is attached either
 * when the property is sent or freed explicitly via the dsvdc_property_free()
 * call.
 *
 * \param[in] property property handle
 * \param[in] name, optoinal name of the property, may be NULL
 * \param value property that will be added to the existing one
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_add_property(dsvdc_property_t *property, const char *name,
                                dsvdc_property_t **value);

/*!\brief Retrieve the number of properties within a property.
 *
 * A property can be seen as either a key:value pair or an array of
 * subproperties. This function will return the number of available
 * subproperties.
 *
 * \param[in] property property handle
 * \return number of available subproperties
 */
size_t dsvdc_property_get_num_properties(const dsvdc_property_t *property);

/*!\brief Retrieve the property name.
 *
 * \param[in] property property handle
 * \param[in] index property index
 * \param[out] name property name, if the function succeeds the name must be
 * freed by the caller
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_get_name(const dsvdc_property_t *property, size_t index,
                            char **name);

/*!\brief Retrieve the value type of the property.
 *
 * This function allows you to decide which dsvdc_property_get_xxx() you
 * have to use and will also let you know if the property does not provide a
 * value.
 *
 * \param[in] property property handle
 * \param[in] index property index
 * \param[out] type, value type of the property
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_get_value_type(const dsvdc_property_t *property,
                                  size_t index,
                                  dsvdc_property_value_t *type);

/*!\brief Retrieve a boolean property value.
 *
 * Use dsvdc_property_get_value_type() in order to determine which value type
 * is provided by the property.
 *
 * \param[in] property property handle
 * \param[in] index property index
 * \param[out] out, property value
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_get_bool(const dsvdc_property_t *property, size_t index,
                            bool *out);

/*!\brief Retrieve an unsigned integer property value.
 *
 * Use dsvdc_property_get_value_type() in order to determine which value type
 * is provided by the property.
 *
 * \param[in] property property handle
 * \param[in] index property index
 * \param[out] out, property value
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_get_uint(const dsvdc_property_t *property, size_t index,
                            uint64_t *out);

/*!\brief Retrieve an integer property value.
 *
 * Use dsvdc_property_get_value_type() in order to determine which value type
 * is provided by the property.
 *
 * \param[in] property property handle
 * \param[in] index property index
 * \param[out] out, property value
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_get_int(const dsvdc_property_t *property, size_t index,
                           int64_t *out);

/*!\brief Retrieve a double property value.
 *
 * Use dsvdc_property_get_value_type() in order to determine which value type
 * is provided by the property.
 *
 * \param[in] property property handle
 * \param[in] index property index
 * \param[out] out, property value
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_get_double(const dsvdc_property_t *property, size_t index,
                              double *out);

/*!\brief Retrieve a string property value.
 *
 * Use dsvdc_property_get_value_type() in order to determine which value type
 * is provided by the property.
 *
 * \param[in] property property handle
 * \param[in] index property index
 * \param[out] out, property value, must be freed by the caller
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_get_string(const dsvdc_property_t *property, size_t index,
                              char **out);

/*!\brief Retrieve a data array property value.
 *
 * Use dsvdc_property_get_value_type() in order to determine which value type
 * is provided by the property.
 *
 * \param[in] property property handle
 * \param[in] index property index
 * \param[out] out property value, must be freed by the caller
 * \param[out] len size of the data buffer, the buffer must be freed by the
 * caller
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_get_bytes(const dsvdc_property_t *property, size_t index,
                             uint8_t **out, size_t *len);

/*!\brief Retrieve a copy of the property by the given name.
 *
 * Use this function to access nested properties.
 *
 * \note Be aware that you are receving a copy of the original property,
 * you must free it when it is no longer needed.
 *
 * \param[in] property property handle
 * \param[in] name property name, first match will be returned
 * \param[out] out property copy, must be freed by the caller
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_get_property_by_name(const dsvdc_property_t *property,
                                        const char *name,
                                        dsvdc_property_t **out);

/*!\brief Retrieve a copy of the property by the given index.
 *
 * Use this function to access nested properties. To determine the number of
 * available properties use dsvdc_property_get_num_properties()
 *
 * \note Be aware that you are receving a copy of the original property,
 * you must free it when it is no longer needed.
 *
 * \param[in] property property handle
 * \param[in] index of the property
 * \param[out] out property copy, must be freed by the caller
 * \return error code, indicating if the operation was successful.
 */
int dsvdc_property_get_property_by_index(const dsvdc_property_t *property,
                                         size_t index, dsvdc_property_t **out);

#ifdef __cplusplus
}
#endif

#endif/*__LIBDSVDC_H__*/
