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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "properties.h"
#include "dsvdc.h"
#include "log.h"
#include "msg_processor.h"

static int dsvdc_property_add(dsvdc_property_t *property,
                              Vdcapi__PropertyElement *element)
{
    if (!property)
    {
        log("invalid property handle\n");
        return DSVDC_ERR_PARAM;
    }

    Vdcapi__PropertyElement **properties;
    size_t new_count = (property->n_properties + 1);

    if (!(property->properties))
    {
        properties = malloc(sizeof(Vdcapi__PropertyElement *) * new_count);
    }
    else
    {
        properties = realloc(property->properties,
                         sizeof(Vdcapi__PropertyElement *) * new_count);
    }

    if (!properties)
    {
        log("could not allocate memory for additional properties\n");
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    properties[new_count - 1] = NULL;

    property->properties = properties;
    property->n_properties = new_count;
    property->properties[new_count - 1] = element;

    return DSVDC_OK;
}

static int dsvdc_property_prepare_element(dsvdc_property_t *property,
                                          const char *key,
                                          Vdcapi__PropertyElement **element)
{
    *element = NULL;

    if (!property)
    {
        log("invalid property handle\n");
        return DSVDC_ERR_PARAM;
    }

    Vdcapi__PropertyElement *el = malloc(sizeof(Vdcapi__PropertyElement));
    if (!el)
    {
        log("could not allocate property element\n");
        return DSVDC_ERR_OUT_OF_MEMORY;
    }
    vdcapi__property_element__init(el);

    // a property name is set, then there must be a value
    if (key)
    {
        el->name = strdup(key);
        if (!el->name)
        {
            log("could not allocate memory for property name\n");
            free(el);
            return DSVDC_ERR_OUT_OF_MEMORY;
        }

        Vdcapi__PropertyValue *val = malloc(sizeof(Vdcapi__PropertyValue));
        if (!val)
        {
            log("could not allocate memory for property value\n");
            free(el->name);
            free(el);
            return DSVDC_ERR_OUT_OF_MEMORY;
        }
        vdcapi__property_value__init(val);

        el->value = val;
    }

    *element = el;

    return DSVDC_OK;
}

static void dsvdc_property_free_value(Vdcapi__PropertyValue *value)
{
    if (value)
    {
        if (value->v_string)
        {
            free(value->v_string);
        }
        else if (value->has_v_bytes)
        {
            if (value->v_bytes.data)
            {
                free(value->v_bytes.data);
            }
        }
        free(value);
    }
}

static void dsvdc_property_free_elements(Vdcapi__PropertyElement **elements,
                                         size_t n_elements)
{
    size_t j;
    if (!elements)
    {
        return;
    }

    for (j = 0; j < n_elements; j++)
    {
        Vdcapi__PropertyElement *el = elements[j];
        if (!el)
        {
            continue;
        }

        if (el->name)
        {
            free(el->name);
        }

        dsvdc_property_free_value(el->value);

        if (el->n_elements > 0)
        {
            dsvdc_property_free_elements(el->elements, el->n_elements);
        }

        free(el);
    } // for elements loop

    free(elements);
}
static Vdcapi__PropertyValue *dsvdc_property_copy_value(
                                                Vdcapi__PropertyValue *input)
{
    Vdcapi__PropertyValue *val = malloc(sizeof(Vdcapi__PropertyValue));
    if (!val)
    {
        log("could not allocate memory for property value");
        return NULL;
    }
    vdcapi__property_value__init(val);
    val->has_v_bool = input->has_v_bool;
    val->v_bool = input->v_bool;
    val->has_v_uint64 = input->has_v_uint64;
    val->v_uint64 = input->v_uint64;
    val->has_v_int64 = input->has_v_int64;
    val->v_int64 = input->v_int64;
    val->has_v_double = input->has_v_double;
    val->v_double = input->v_double;
    if (input->v_string)
    {
        val->v_string = strdup(input->v_string);
        if (!val->v_string)
        {
            log("could not allocate memory for property value");
            free(val);
            return NULL;
        }
    }
    else
    {
        val->v_string = NULL;
    }
    val->has_v_bytes = input->has_v_bytes;
    if (input->has_v_bytes && (input->v_bytes.len > 0))
    {
        val->v_bytes.len = input->v_bytes.len;
        val->v_bytes.data = malloc(sizeof(uint8_t)*val->v_bytes.len);
        if (!val->v_bytes.data)
        {
            log("could not allocate memory for property value");
            if (val->v_string)
            {
                free(val->v_string);
            }
            free(val);
            return NULL;
        }
        memcpy(val->v_bytes.data, input->v_bytes.data, val->v_bytes.len);
    }

    return val;
}

static Vdcapi__PropertyElement **dsvdc_property_deep_copy(
                                    Vdcapi__PropertyElement **input,
                                    size_t n_input)
{
    size_t i;
    Vdcapi__PropertyElement **copy;
    copy = malloc(sizeof(Vdcapi__PropertyElement) * n_input);
    if (!copy)
    {
        log("could not allocate memory for properties");
        return NULL;
    }

    for (i = 0; i < n_input; i++)
    {
        Vdcapi__PropertyElement *current_input = input[i];
        copy[i] = malloc(sizeof(Vdcapi__PropertyElement));
        if (!copy[i])
        {
            log("could not allocate memory for properties");
            dsvdc_property_free_elements(copy, n_input);
            return NULL;
        }

        Vdcapi__PropertyElement *current_copy = copy[i];
        vdcapi__property_element__init(current_copy);

        if (current_input->name)
        {
            current_copy->name = strdup(current_input->name);
            if (!current_copy->name)
            {
                log("could not allocate memory for properties");
                dsvdc_property_free_elements(copy, n_input);
                return NULL;
            }
        }

        if (current_input->value)
        {
            current_copy->value = dsvdc_property_copy_value(
                                                        current_input->value);
            if (!current_copy->value)
            {
                log("could not allocate memory for properties");
                dsvdc_property_free_elements(copy, n_input);
                return NULL;
            }
        }

        current_copy->n_elements = current_input->n_elements;
        if (current_copy->n_elements > 0)
        {
            current_copy->elements = dsvdc_property_deep_copy(
                                                    current_input->elements,
                                                    current_input->n_elements);
            if (!current_copy->elements)
            {
                log("could not allocate memory for properties");
                dsvdc_property_free_elements(copy, n_input);
                return NULL;
            }
        }
    }

    return copy;
}

#if __GNUC__ >= 4
    #pragma GCC visibility push(hidden)
#endif

int dsvdc_property_convert_query(Vdcapi__PropertyElement **query,
                                 size_t n_query, dsvdc_property_t **property)
{
    int ret;
    *property = NULL;

    ret = dsvdc_property_new(property);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    (*property)->n_properties = n_query;
    (*property)->properties = dsvdc_property_deep_copy(query, n_query);
    return DSVDC_OK;
}

#if __GNUC__ >= 4
    #pragma GCC visibility pop
#endif

/* public interface */

int dsvdc_property_new(dsvdc_property_t **property)
{
    *property = NULL;

    dsvdc_property_t *p = malloc(sizeof(struct dsvdc_property));
    if (!p)
    {
        log("could not allocate new property instance\n");
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    p->n_properties = 0;
    p->properties = NULL;

    *property = p;
    return DSVDC_OK;
}

void dsvdc_property_free(dsvdc_property_t *property)
{
    if (!property)
    {
        return;
    }

    if (property->n_properties > 0)
    {
        // will walk the properties recursively
        dsvdc_property_free_elements(property->properties,
                                     property->n_properties);
    }

    free(property);
}

int dsvdc_property_add_int(dsvdc_property_t *property,
                           const char *key, int64_t value)
{
    int ret;
    Vdcapi__PropertyElement *element = NULL;

    if (!key)
    {
        log("missing property name");
        return DSVDC_ERR_PARAM;
    }

    ret = dsvdc_property_prepare_element(property, key, &element);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    element->value->v_int64 = value;
    element->value->has_v_int64 = 1;

    ret = dsvdc_property_add(property, element);
    if (ret != DSVDC_OK)
    {
        free(element->value);
        free(element->name);
        free(element);
        return ret;
    }

    return DSVDC_OK;
}

int dsvdc_property_add_uint(dsvdc_property_t *property,
                            const char *key, uint64_t value)
{
    int ret;
    Vdcapi__PropertyElement *element = NULL;

    if (!key)
    {
        log("missing property name");
        return DSVDC_ERR_PARAM;
    }

    ret = dsvdc_property_prepare_element(property, key, &element);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    element->value->v_uint64 = value;
    element->value->has_v_uint64 = 1;

    ret = dsvdc_property_add(property, element);
    if (ret != DSVDC_OK)
    {
        free(element->value);
        free(element->name);
        free(element);
        return ret;
    }

    return DSVDC_OK;
}

int dsvdc_property_add_bool(dsvdc_property_t *property, const char *key,
                            bool value)
{
    int ret;
    Vdcapi__PropertyElement *element = NULL;

    if (!key)
    {
        log("missing property name");
        return DSVDC_ERR_PARAM;
    }

    ret = dsvdc_property_prepare_element(property, key, &element);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    element->value->v_bool = value;
    element->value->has_v_bool = 1;

    ret = dsvdc_property_add(property, element);
    if (ret != DSVDC_OK)
    {
        free(element->value);
        free(element->name);
        free(element);
        return ret;
    }

    return DSVDC_OK;
}

int dsvdc_property_add_double(dsvdc_property_t *property,
                              const char *key, double value)
{
    int ret;
    Vdcapi__PropertyElement *element = NULL;

    if (!key)
    {
        log("missing property name");
        return DSVDC_ERR_PARAM;
    }

    ret = dsvdc_property_prepare_element(property, key, &element);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    element->value->v_double = value;
    element->value->has_v_double = 1;

    ret = dsvdc_property_add(property, element);
    if (ret != DSVDC_OK)
    {
        free(element->value);
        free(element->name);
        free(element);
        return ret;
    }

    return DSVDC_OK;
}

int dsvdc_property_add_string(dsvdc_property_t *property, const char *key,
                              const char *value)
{
    int ret;
    Vdcapi__PropertyElement *element = NULL;

    if (!key)
    {
        log("missing property name");
        return DSVDC_ERR_PARAM;
    }

    if (value == NULL)
    {
        log("invalid string value\n");
        return DSVDC_ERR_PARAM;
    }

    ret = dsvdc_property_prepare_element(property, key, &element);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    element->value->v_string = strdup(value);
    if (!element->value->v_string)
    {
        log("could not allocate memory for string value\n");
        free(element->value);
        free(element->name);
        free(element);
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    ret = dsvdc_property_add(property, element);
    if (ret != DSVDC_OK)
    {
        free(element->value->v_string);
        free(element->value);
        free(element->name);
        free(element);
        return ret;
    }

    return DSVDC_OK;
}

int dsvdc_property_add_bytes(dsvdc_property_t *property, const char *key,
                              const uint8_t *value, const size_t length)
{
    int ret;
    Vdcapi__PropertyElement *element = NULL;

    if (!key)
    {
        log("missing property name");
        return DSVDC_ERR_PARAM;
    }

    if (value == NULL)
    {
        log("empty data buffer\n");
        return DSVDC_ERR_PARAM;
    }

    if (length == 0)
    {
        log("zero length data buffer\n");
        return DSVDC_ERR_PARAM;
    }

    ret = dsvdc_property_prepare_element(property, key, &element);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    element->value->v_bytes.len = length;
    element->value->v_bytes.data = (uint8_t *)malloc(sizeof(uint8_t) * length);
    if (!element->value->v_bytes.data)
    {
        log("could not allocate memory for data buffer\n");
        free(element->value);
        free(element->name);
        free(element);
        return DSVDC_ERR_OUT_OF_MEMORY;
    }
    memcpy(element->value->v_bytes.data, value, length);
    element->value->has_v_bytes = 1;

    ret = dsvdc_property_add(property, element);
    if (ret != DSVDC_OK)
    {
        free(element->value->v_bytes.data);
        free(element->value);
        free(element->name);
        free(element);
        return ret;
    }

    return DSVDC_OK;
}
int dsvdc_property_add_property(dsvdc_property_t *property, const char *name,
                                dsvdc_property_t **value)
{
    int ret;
    Vdcapi__PropertyElement *element = NULL;

    if (*value == NULL)
    {
        log("missing value property\n");
        return DSVDC_ERR_PARAM;
    }

    ret = dsvdc_property_prepare_element(property, NULL, &element);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    if (name)
    {
        element->name = strdup(name);
        if (!element->name)
        {
            log("could not allocate memory for element name");
            free(element);
            return DSVDC_ERR_OUT_OF_MEMORY;
        }
    }

    element->elements = (*value)->properties;
    element->n_elements = (*value)->n_properties;

    // claim ownership of content and free value property structure
    (*value)->properties = NULL;
    (*value)->n_properties = 0;
    free(*value);
    *value = NULL;

    ret = dsvdc_property_add(property, element);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    return DSVDC_OK;
}

int dsvdc_send_get_property_response(dsvdc_t *handle, dsvdc_property_t *property)
{
    if (!handle)
    {
        log("can't send get property response: invalid handle\n");
        if (property)
        {
            dsvdc_property_free(property);
        }
        return DSVDC_ERR_PARAM;
    }

    if (!property)
    {
        log("can't send get property response: invalid property reference\n");
        return DSVDC_ERR_PARAM;
    }

    Vdcapi__Message reply = VDCAPI__MESSAGE__INIT;
    Vdcapi__VdcResponseGetProperty submsg =
                                    VDCAPI__VDC__RESPONSE_GET_PROPERTY__INIT;

    submsg.n_properties = property->n_properties;
    submsg.properties = property->properties;
    reply.type = VDCAPI__TYPE__VDC_RESPONSE_GET_PROPERTY;
    reply.message_id = property->message_id;
    reply.has_message_id = 1;
    reply.vdc_response_get_property = &submsg;

    int ret = dsvdc_send_message(handle, &reply);
    log("VDC_RESPONSE_GET_PROPERTY/%u sent with code %d\n",
        reply.message_id, ret);
    dsvdc_property_free(property);
    return ret;
}

int dsvdc_send_set_property_response(dsvdc_t *handle,
                                     dsvdc_property_t *property, uint8_t code)
{
    if (!handle)
    {
        log("can't send set property response: invalid handle\n");
        if (property)
        {
            dsvdc_property_free(property);
        }
        return DSVDC_ERR_PARAM;
    }

    if (!property)
    {
        log("can't send get property response: invalid property reference\n");
        return DSVDC_ERR_PARAM;
    }


    if (code > DSVDC_ERR_NOT_AUTHORIZED)
    {
        log("can't send set property response: invalid code given\n");
        return DSVDC_ERR_PARAM;
    }

    dsvdc_send_error_message(handle, (Vdcapi__ResultCode)code,
                             property->message_id);
    log("VDC_RESPONSE_SET_PROPERTY/%u sent\n", property->message_id);
    dsvdc_property_free(property);
    return DSVDC_OK;
}

int dsvdc_push_property(dsvdc_t *handle, const char *dsuid,
                        dsvdc_property_t *property)
{
    if (!handle)
    {
        log("can't push property: invalid handle\n");
        return DSVDC_ERR_PARAM;
    }

    Vdcapi__Message msg = VDCAPI__MESSAGE__INIT;
    Vdcapi__VdcSendPushProperty submsg = VDCAPI__VDC__SEND_PUSH_PROPERTY__INIT;
    submsg.dsuid = (char *)dsuid;

    submsg.n_properties = property->n_properties;
    submsg.properties = property->properties;

    msg.type = VDCAPI__TYPE__VDC_SEND_PUSH_PROPERTY;
    msg.vdc_send_push_property = &submsg;

    int ret = dsvdc_send_message(handle, &msg);
    log("VDCAPI__TYPE__VDC_SEND_PUSH_PROPERTY  sent with code %d\n", ret);
    return ret;
}

size_t dsvdc_property_get_num_properties(const dsvdc_property_t *property)
{
    if (!property)
    {
        return 0;
    }

    return property->n_properties;
}

int dsvdc_property_get_name(const dsvdc_property_t *property, size_t index,
                            char **name)
{
    *name = NULL;

    if (!property)
    {
        return DSVDC_ERR_INVALID_PROPERTY;
    }

    if (index >= property->n_properties)
    {
        return DSVDC_ERR_PROPERTY_INDEX;
    }

    Vdcapi__PropertyElement *element = property->properties[index];

    if (!element)
    {
        return DSVDC_ERR_PROPERTY_INDEX;
    }

    if (element->name)
    {
        *name = strdup(element->name);
        if (!(*name))
        {
            log("could not allocate memory for property name\n");
            return DSVDC_ERR_OUT_OF_MEMORY;
        }
    }

    return DSVDC_OK;
}

int dsvdc_property_get_value_type(const dsvdc_property_t *property,
                                  size_t index,
                                  dsvdc_property_value_t *type)
{
    *type = DSVDC_PROPERTY_VALUE_NONE;

    if (!property)
    {
        return DSVDC_ERR_INVALID_PROPERTY;
    }

    if (index >= property->n_properties)
    {
        return DSVDC_ERR_PROPERTY_INDEX;
    }

    Vdcapi__PropertyElement *element = property->properties[index];

    if (!element)
    {
        return DSVDC_ERR_PROPERTY_INDEX;
    }

    Vdcapi__PropertyValue *val = element->value;

    if (!val)
    {
        return DSVDC_OK;
    }

    if (val->has_v_bool)
    {
        *type = DSVDC_PROPERTY_VALUE_BOOL;
    }
    else if (val->has_v_uint64)
    {
        *type = DSVDC_PROPERTY_VALUE_UINT64;

    }
    else if (val->has_v_int64)
    {
        *type = DSVDC_PROPERTY_VALUE_INT64;
    }
    else if (val->has_v_double)
    {
        *type = DSVDC_PROPERTY_VALUE_DOUBLE;
    }
    else if (val->v_string)
    {
        *type = DSVDC_PROPERTY_VALUE_STRING;
    }
    else if (val->v_bytes.len > 0)
    {
        *type = DSVDC_PROPERTY_VALUE_BYTES;
    }

    return DSVDC_OK;
}

int dsvdc_property_get_bool(const dsvdc_property_t *property,
                            size_t index, bool *out)
{
    dsvdc_property_value_t type;
    int ret = dsvdc_property_get_value_type(property, index, &type);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    if (type != DSVDC_PROPERTY_VALUE_BOOL)
    {
        return DSVDC_ERR_PROPERTY_VALUE_TYPE;
    }

    Vdcapi__PropertyElement *element = property->properties[index];
    Vdcapi__PropertyValue *val = element->value;
    *out = val->v_bool;
    return DSVDC_OK;
}

int dsvdc_property_get_uint(const dsvdc_property_t *property, size_t index,
                            uint64_t *out)
{
    dsvdc_property_value_t type;
    int ret = dsvdc_property_get_value_type(property, index, &type);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    if (type != DSVDC_PROPERTY_VALUE_UINT64)
    {
        return DSVDC_ERR_PROPERTY_VALUE_TYPE;
    }

    Vdcapi__PropertyElement *element = property->properties[index];
    Vdcapi__PropertyValue *val = element->value;
    *out = val->v_uint64;
    return DSVDC_OK;
}

int dsvdc_property_get_int(const dsvdc_property_t *property, size_t index,
                           int64_t *out)
{
    dsvdc_property_value_t type;
    int ret = dsvdc_property_get_value_type(property, index, &type);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    if (type != DSVDC_PROPERTY_VALUE_INT64)
    {
        return DSVDC_ERR_PROPERTY_VALUE_TYPE;
    }

    Vdcapi__PropertyElement *element = property->properties[index];
    Vdcapi__PropertyValue *val = element->value;
    *out = val->v_int64;
    return DSVDC_OK;
}

int dsvdc_property_get_double(const dsvdc_property_t *property, size_t index,
                              double *out)
{
    dsvdc_property_value_t type;
    int ret = dsvdc_property_get_value_type(property, index, &type);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    if (type != DSVDC_PROPERTY_VALUE_DOUBLE)
    {
        return DSVDC_ERR_PROPERTY_VALUE_TYPE;
    }

    Vdcapi__PropertyElement *element = property->properties[index];
    Vdcapi__PropertyValue *val = element->value;
    *out = val->v_double;
    return DSVDC_OK;
}

int dsvdc_property_get_string(const dsvdc_property_t *property, size_t index,
                              char **out)
{
    dsvdc_property_value_t type;
    int ret = dsvdc_property_get_value_type(property, index, &type);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    if (type != DSVDC_PROPERTY_VALUE_STRING)
    {
        return DSVDC_ERR_PROPERTY_VALUE_TYPE;
    }

    Vdcapi__PropertyElement *element = property->properties[index];
    Vdcapi__PropertyValue *val = element->value;
    *out = strdup(val->v_string);
    if (!(*out))
    {
        log("could not allocate memory for string property");
        return DSVDC_ERR_OUT_OF_MEMORY;
    }
    return DSVDC_OK;
}

int dsvdc_property_get_bytes(const dsvdc_property_t *property, size_t index,
                             uint8_t **out, size_t *len)
{
    *out = NULL;
    *len = 0;

    dsvdc_property_value_t type;
    int ret = dsvdc_property_get_value_type(property, index, &type);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    if (type != DSVDC_PROPERTY_VALUE_BYTES)
    {
        return DSVDC_ERR_PROPERTY_VALUE_TYPE;
    }

    Vdcapi__PropertyElement *element = property->properties[index];
    Vdcapi__PropertyValue *val = element->value;
    *out = malloc(sizeof(uint8_t) * val->v_bytes.len);
    if (!(*out))
    {
        log("could not allocate memory for bytes property");
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    memcpy(*out, val->v_bytes.data, val->v_bytes.len);
    *len = val->v_bytes.len;
    return DSVDC_OK;
}


int dsvdc_property_get_property_by_name(const dsvdc_property_t *property,
                                        const char *name,
                                        dsvdc_property_t **out)
{
    size_t i;
    *out = NULL;

    dsvdc_property_t *prop = NULL;

    if (!property)
    {
        return DSVDC_ERR_INVALID_PROPERTY;
    }

    for (i = 0; i < property->n_properties; i++)
    {
        Vdcapi__PropertyElement *element = property->properties[i];
        if (!element)
        {
            continue;
        }

        if (strcmp(name, element->name) == 0)
        {
            prop = malloc(sizeof(dsvdc_property_t));
            if (!prop)
            {
                log("could not allocate memory for property");
                return DSVDC_ERR_OUT_OF_MEMORY;
            }
            prop->properties = NULL;
            prop->n_properties = element->n_elements;
            if (prop->n_properties > 0)
            {
                prop->properties = dsvdc_property_deep_copy(element->elements,
                                                           element->n_elements);
                if (!prop->properties)
                {
                    free(prop);
                    return DSVDC_ERR_OUT_OF_MEMORY;
                }
            }
            break;
        }
    }

    *out = prop;

    return DSVDC_OK;
}

int dsvdc_property_get_property_by_index(const dsvdc_property_t *property,
                                         size_t index, dsvdc_property_t **out)
{
    *out = NULL;

    dsvdc_property_t *prop = NULL;

    if (!property)
    {
        return DSVDC_ERR_INVALID_PROPERTY;
    }

    if (index >= property->n_properties)
    {
        return DSVDC_ERR_PROPERTY_INDEX;
    }

    Vdcapi__PropertyElement *element = property->properties[index];
    if (!element)
    {
        return DSVDC_ERR_PROPERTY_INDEX;
    }

    prop = malloc(sizeof(dsvdc_property_t));
    if (!prop)
    {
        log("could not allocate memory for property");
        return DSVDC_ERR_OUT_OF_MEMORY;
    }
    prop->properties = NULL;
    prop->n_properties = element->n_elements;
    if (prop->n_properties > 0)
    {
        prop->properties = dsvdc_property_deep_copy(element->elements,
                                                   element->n_elements);
        if (!prop->properties)
        {
            free(prop);
            return DSVDC_ERR_OUT_OF_MEMORY;
        }
    }

    *out = prop;

    return DSVDC_OK;
}
