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

static int dsvdc_property_add(dsvdc_property_t *property, size_t index,
                              Vdcapi__PropertyElement *element)
{
    if (!property)
    {
        log("invalid property handle\n");
        return DSVDC_ERR_PARAM;
    }

    // if index is beyond the allocated area, realloc the first level array
    if (property->n_properties < (index + 1))
    {
        Vdcapi__Property **properties;
        size_t new_count = (index + 1);

        properties = realloc(property->properties,
                             sizeof(Vdcapi__Property *) * new_count);
        if (!properties)
        {
            log("could not allocate memory for additional properties\n");
            return DSVDC_ERR_OUT_OF_MEMORY;
        }

        size_t i;
        for (i = property->n_properties; i < new_count; i++)
        {
            properties[i] = NULL;
        }

        property->properties = properties;
        property->n_properties = new_count;
    }

    if (!property->properties[index])
    {
        property->properties[index] = malloc(sizeof(Vdcapi__Property));
        if (!property->properties[index])
        {
            log("could not allocate memory for property\n");
            return DSVDC_ERR_OUT_OF_MEMORY;
        }
        vdcapi__property__init(property->properties[index]);
    }

    Vdcapi__Property *prop = property->properties[index];
    Vdcapi__PropertyElement **elements = realloc(prop->elements,
                    sizeof(Vdcapi__PropertyElement *) * (prop->n_elements + 1));
    if (!elements)
    {
        log("could not allocate property elements array\n");
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    prop->elements = elements;
    prop->elements[prop->n_elements] = element;
    prop->n_elements = prop->n_elements + 1;

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

    *element = el;

    return DSVDC_OK;
}

bool dsvdc_property_is_sane(dsvdc_property_t *property)
{
    if (!property)
    {
        return false;
    }

    // First level must have no holes, each first level properties must
    // have the same number of elements. More extensive checks, like
    // structure of the element arrays (each property must have same
    // keys in the element array) are not performed.
    size_t i;
    size_t elements = 0;
    for (i = 0; i < property->n_properties; i++)
    {
        // "index hole"
        if (property->properties[i] == NULL)
        {
            log("uninitialized properties / \"holes\" on first level\n");
            return false;
        }

        if (i == 0) // save element count for first property
        {
            elements = property->properties[i]->n_elements;
            if (elements == 0) // empty properties make no sense
            {
                log("property with zero elements detected\n");
                return false;
            }
        }

        // properties have different element lenghts, not allowed in spec
        if (elements != property->properties[i]->n_elements)
        {
            log("property element count mismatch\n");
            return false;
        }
    }

    return true;
}

/* public interface */

int dsvdc_property_new(const char *name, size_t hint,
                       dsvdc_property_t **property)
{
    *property = NULL;

    if ((!name) || (strlen(name) == 0))
    {
        log("can't create a nameless property\n");
        return DSVDC_ERR_PARAM;
    }

    dsvdc_property_t *p = malloc(sizeof(struct dsvdc_property));
    if (!p)
    {
        log("could not allocate new property\n");
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    p->name = strdup(name);
    if (!p->name)
    {
        log("could not allocate memory for property name\n");
        free(p);
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    if (hint)
    {
        p->properties = malloc(sizeof(Vdcapi__Property *) * hint);
        if (!p->properties)
        {
            log("could not allocate memory for property array\n");
            free(p->name);
            free(p);
            return DSVDC_ERR_OUT_OF_MEMORY;
        }

        size_t i;
        for (i = 0; i < hint; i++)
        {
            p->properties[i] = NULL;
        }
    }
    else
    {
        p->properties = NULL;
    }

    p->n_properties = hint;

    *property = p;
    return DSVDC_OK;
}

void dsvdc_property_free(dsvdc_property_t *property)
{
    Vdcapi__Property **properties;

    if (!property)
    {
        return;
    }

    if (property->name)
    {
        free(property->name);
    }

    if (property->properties)
    {
        properties = property->properties;
        size_t i, j;

        for (i = 0; i < property->n_properties; i++)
        {
            Vdcapi__Property *prop = properties[i];
            if (!prop)
            {
                continue;
            }

            if (prop->elements)
            {
                for (j = 0; j < prop->n_elements; j++)
                {
                    Vdcapi__PropertyElement *el = prop->elements[j];
                    if (!el)
                    {
                        continue;
                    }

                    if (el->name)
                    {
                        free(el->name);
                    }

                    if (el->value)
                    {
                        if (el->value->v_string)
                        {
                            free(el->value->v_string);
                        }
                        free(el->value);
                    }

                    free(el);
                } // for elements loop
                free(prop->elements);
            }

            free(prop);
        } // for properties loop

        free(property->properties);
    } // vdc_response_get_property exists

    free(property);
}

int dsvdc_property_add_int(dsvdc_property_t *property, size_t index,
                           const char *key, int64_t value)
{
    int ret;
    Vdcapi__PropertyElement *element = NULL;

    ret = dsvdc_property_prepare_element(property, key, &element);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    element->value->v_int64 = value;
    element->value->has_v_int64 = 1;

    ret = dsvdc_property_add(property, index, element);
    if (ret != DSVDC_OK)
    {
        free(element->value);
        free(element->name);
        free(element);
        return ret;
    }

    return DSVDC_OK;
}

int dsvdc_property_add_uint(dsvdc_property_t *property, size_t index,
                            const char *key, uint64_t value)
{
    int ret;
    Vdcapi__PropertyElement *element = NULL;

    ret = dsvdc_property_prepare_element(property, key, &element);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    element->value->v_uint64 = value;
    element->value->has_v_uint64 = 1;

    ret = dsvdc_property_add(property, index, element);
    if (ret != DSVDC_OK)
    {
        free(element->value);
        free(element->name);
        free(element);
        return ret;
    }

    return DSVDC_OK;
}

int dsvdc_property_add_bool(dsvdc_property_t *property, size_t index,
                           const char *key, bool value)
{
    int ret;
    Vdcapi__PropertyElement *element = NULL;

    ret = dsvdc_property_prepare_element(property, key, &element);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    element->value->v_bool = value;
    element->value->has_v_bool = 1;

    ret = dsvdc_property_add(property, index, element);
    if (ret != DSVDC_OK)
    {
        free(element->value);
        free(element->name);
        free(element);
        return ret;
    }

    return DSVDC_OK;
}

int dsvdc_property_add_double(dsvdc_property_t *property, size_t index,
                              const char *key, double value)
{
    int ret;
    Vdcapi__PropertyElement *element = NULL;

    ret = dsvdc_property_prepare_element(property, key, &element);
    if (ret != DSVDC_OK)
    {
        return ret;
    }

    element->value->v_double = value;
    element->value->has_v_double = 1;

    ret = dsvdc_property_add(property, index, element);
    if (ret != DSVDC_OK)
    {
        free(element->value);
        free(element->name);
        free(element);
        return ret;
    }

    return DSVDC_OK;
}

int dsvdc_property_add_string(dsvdc_property_t *property, size_t index,
                              const char *key, const char *value)
{
    int ret;
    Vdcapi__PropertyElement *element = NULL;

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

    ret = dsvdc_property_add(property, index, element);
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

int dsvdc_send_property_response(dsvdc_t *handle, dsvdc_property_t *property)
{
    if (!handle)
    {
        log("can't send property response: invalid handle\n");
        dsvdc_property_free(property);
        return DSVDC_ERR_PARAM;
    }

    if (!dsvdc_property_is_sane(property))
    {
        log("property structure sanity checks failed, sending error\n");
        dsvdc_send_error_message(handle,
                VDCAPI__RESULT_CODE__ERR_SERVICE_NOT_AVAILABLE,
                property->message_id);
        dsvdc_property_free(property);
        return DSVDC_ERR_INVALID_PROPERTY;
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

int dsvdc_push_property(dsvdc_t *handle, const char *dsuid, const char *name,
                        uint32_t offset, dsvdc_property_t *property)
{
    if (!handle)
    {
        log("can't push property: invalid handle\n");
        return DSVDC_ERR_PARAM;
    }

    if (!dsvdc_property_is_sane(property))
    {
        log("property structure sanity checks failed, sending error\n");
        return DSVDC_ERR_INVALID_PROPERTY;
    }

    Vdcapi__Message msg = VDCAPI__MESSAGE__INIT;
    Vdcapi__VdcSendPushProperty submsg = VDCAPI__VDC__SEND_PUSH_PROPERTY__INIT;
    submsg.dsuid = (char *)dsuid;
    submsg.name = (char *)name;
    submsg.offset = offset;
    submsg.has_offset = 1;

    submsg.n_properties = property->n_properties;
    submsg.properties = property->properties;

    msg.type = VDCAPI__TYPE__VDC_SEND_PUSH_PROPERTY;
    msg.vdc_send_push_property = &submsg;

    int ret = dsvdc_send_message(handle, &msg);
    log("VDCAPI__TYPE__VDC_SEND_PUSH_PROPERTY  sent with code %d\n", ret);
    return ret;
}
