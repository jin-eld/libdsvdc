/*
    Copyright (c) 2013 aizo ag, Zurich, Switzerland
    Copyright (c) 2014 digitalSTROM AG, Zurich, Switzerland

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
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include <getopt.h>
#define OPTSTR "rhl:c:g:"

#include <digitalSTROM/dsuid.h>

#include "dsvdc.h"
#include "common.h"

/*
 * button click states, see:
 * https://www.digitalstrom.org/wp-content/uploads/2013/11/vDC-API-properties1.pdf
 * Chapter, 4.2.3 ButtonInputState
 */
#define TIP_1X  0u
#define TIP_2x  1u
#define TIP_3x  2u
#define TIP_4x  3u

/*
 * TODO: fill scene table and add persistent storage
 *
static uint8_t g_scene_values[128] =
{
    0,      0,      0,      0,      0,
    100,    0,      0,      0,      0,
    0,      0,      0,      0,      0,
    0,      100,    100,    100,    0
};

static uint8_t g_scene_config[128] =
{
    0
};

static uint8_t g_output_value = 0;
static uint8_t g_min_dim_value = 5;
static uint8_t g_dim_step_value = 5;
static bool g_local_prioritized = false;
*/

static char g_dev_dsuid[35] = { 0 };
static char g_vdc_dsuid[35] = { 0 };

/* "library" dsuid is currently unused */
static char g_lib_dsuid[35] = { 0 };

static int g_shutdown_flag = 0;

static const uint8_t deviceIcon16_png[] =
{
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
  0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10,
  0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0xf3, 0xff, 0x61, 0x00, 0x00, 0x00,
  0x06, 0x62, 0x4b, 0x47, 0x44, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0xa0,
  0xbd, 0xa7, 0x93, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00,
  0x00, 0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18,
  0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xde, 0x08, 0x15,
  0x09, 0x19, 0x31, 0x76, 0x58, 0xa9, 0x3e, 0x00, 0x00, 0x01, 0x2a, 0x49,
  0x44, 0x41, 0x54, 0x38, 0xcb, 0xcd, 0x93, 0xbd, 0x4a, 0x03, 0x51, 0x14,
  0x84, 0xbf, 0xd9, 0xc4, 0x22, 0x92, 0xc2, 0x58, 0x59, 0x28, 0x42, 0x1e,
  0x40, 0xac, 0xdc, 0x55, 0x10, 0x5b, 0x6d, 0x74, 0xd7, 0x37, 0xf0, 0x49,
  0xc4, 0x87, 0xb0, 0xb2, 0xb1, 0xb1, 0x75, 0x63, 0xa1, 0xe0, 0x13, 0x64,
  0xbb, 0x14, 0x82, 0xe2, 0x4f, 0xa7, 0x45, 0x0a, 0x45, 0x11, 0xd4, 0x80,
  0x66, 0xc7, 0x46, 0x12, 0xa3, 0xe4, 0xa7, 0xcc, 0x54, 0xe7, 0x70, 0xcf,
  0x1d, 0xe6, 0xcc, 0x9d, 0x0b, 0xe3, 0x83, 0x24, 0x1a, 0xdc, 0xf7, 0x41,
  0xd0, 0xa9, 0x6c, 0xf5, 0x9c, 0xfc, 0xed, 0xfb, 0xa0, 0x3b, 0x94, 0x84,
  0x53, 0xa0, 0x3d, 0xa0, 0x0a, 0x3e, 0xe7, 0xbd, 0xb4, 0xcf, 0x64, 0xcb,
  0xa4, 0xd9, 0x08, 0x04, 0x71, 0xb4, 0x0e, 0x3e, 0x45, 0x04, 0x20, 0x03,
  0xc2, 0x7e, 0x40, 0x5a, 0x04, 0x9e, 0x06, 0x91, 0x14, 0x48, 0xa2, 0x12,
  0xe2, 0x12, 0x94, 0x03, 0x01, 0x52, 0x8e, 0x09, 0x90, 0xca, 0xc0, 0x1c,
  0x69, 0x76, 0x3c, 0x48, 0x41, 0x11, 0xda, 0xc2, 0x85, 0x4f, 0xa0, 0x89,
  0x94, 0x80, 0xae, 0x91, 0x63, 0xec, 0x43, 0xa4, 0x0f, 0xe2, 0x68, 0x05,
  0xfc, 0x48, 0x4d, 0x37, 0x90, 0xfd, 0x36, 0x79, 0x03, 0x78, 0x0d, 0xb0,
  0x26, 0x10, 0x0d, 0x6a, 0xd9, 0x3c, 0x76, 0x83, 0xb4, 0xfe, 0x46, 0x9a,
  0x1d, 0x61, 0x57, 0x81, 0x2f, 0xa0, 0x8c, 0x74, 0x45, 0xcc, 0x6a, 0xe7,
  0x72, 0x1c, 0xed, 0x80, 0xcf, 0x80, 0x5c, 0x6c, 0x85, 0x45, 0x0a, 0x2f,
  0x6d, 0x5c, 0xe9, 0x35, 0x2c, 0x59, 0x86, 0xb4, 0x0e, 0x71, 0xb8, 0x89,
  0x74, 0x02, 0x06, 0xeb, 0x16, 0x5c, 0x42, 0x9a, 0xc5, 0x00, 0x5e, 0x1b,
  0xfe, 0x54, 0xdb, 0xd1, 0x34, 0xe6, 0x0e, 0xa8, 0x60, 0x77, 0xbd, 0x17,
  0x17, 0x28, 0x5f, 0x0a, 0x86, 0x12, 0xe4, 0x3c, 0x13, 0x68, 0x06, 0x7c,
  0x80, 0xd4, 0x04, 0xee, 0x91, 0x77, 0x49, 0xb3, 0x05, 0x1c, 0xb4, 0x46,
  0x4d, 0xe9, 0x7f, 0xa5, 0x71, 0xa8, 0xde, 0x24, 0x0e, 0x86, 0x3b, 0xf1,
  0x8e, 0xc3, 0x9f, 0x2d, 0xe4, 0xf1, 0xf8, 0x83, 0xdf, 0x9f, 0x72, 0x65,
  0x87, 0xbf, 0x51, 0x22, 0xbd, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
  0x44, 0xae, 0x42, 0x60, 0x82
};

void signal_handler(int signum)
{
    if ((signum == SIGINT) || (signum == SIGTERM))
    {
        g_shutdown_flag++;
    }
}

void get_network_interface(char *mac, int maxlen)
{
    memset(mac, 0, maxlen);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        return;
    }

    struct if_nameindex *if_name;
    if_name = if_nameindex();
    if (if_name == NULL)
    {
        close(sock);
        return;
    }

    int i;
    for (i = 0; if_name[i].if_index || if_name[i].if_name != NULL; i++)
    {
        struct ifreq ifr;
        strncpy(ifr.ifr_name, if_name[i].if_name, IFNAMSIZ-1);
        ifr.ifr_name[IFNAMSIZ-1] = '\0';

        if (strncmp(ifr.ifr_name, "lo", 2) == 0)
        {
            continue;
        }

        if (ioctl(sock, SIOCGIFHWADDR, &ifr) >= 0)
        {
            int j, k;
            for (j = 0, k = 0; j < 6; j++)
            {
                k += snprintf(mac + k, maxlen - k - 1, j ? ":%02X" : "%02X",
                        (int)(unsigned int)(unsigned char) ifr.ifr_hwaddr.sa_data[j]);
            }
            if (k < maxlen)
            {
                mac[k] = 0;
            }
            else
            {
                mac[maxlen] = 0;
            }
            break;
        }
    }
    if_freenameindex(if_name);
    close(sock);
}

void print_copyright()
{
    printf("\nlibdSvDC reference table light device, version %s\n", VERSION);
    printf("Copyright (c) 2015 digitalSTROM AG, Zurich, Switzerland\n");
    printf("libdSvD is free software, covered by the GNU General Public "
           "License as published\n"
           "by the Free Software Foundation, version 3 or later.\n\n");
}

void print_usage(const char *program)
{
    printf("Usage: %s [options]\n\
\n\
Supported options:\n\
    --random or -r      randomize dSUIDs\n\
    --library-dsuid     specify library dSUID\n\
    --container-dsuid   specify virtual container dSUID\n\
    --device-dsuid      specify device dSUID\n\
    --help or -h        this help text\n\
\n", program);
}

void send_button_click(dsvdc_t *handle, uint64_t clickType)
{
    dsvdc_property_t* pushEnvelope;
    dsvdc_property_t* propButtonState;
    dsvdc_property_t* prop;

    dsvdc_property_new(&pushEnvelope);
    dsvdc_property_new(&propButtonState);
    dsvdc_property_new(&prop);

    if (pushEnvelope == NULL || propButtonState == NULL || prop == NULL)
    {
        if (pushEnvelope)
        {
            dsvdc_property_free(pushEnvelope);
        }

        if (propButtonState)
        {
            dsvdc_property_free(propButtonState);
        }

        if (prop)
        {
            dsvdc_property_free(prop);
        }

        fprintf(stderr, "send_button_click: could not allocate property");
        return;
    }

    dsvdc_property_add_uint(prop, "clickType", clickType);
    dsvdc_property_add_property(propButtonState, "0", &prop);
    dsvdc_property_add_property(pushEnvelope, "buttonInputStates",
                                &propButtonState);
    dsvdc_push_property(handle, g_dev_dsuid, pushEnvelope);
    dsvdc_property_free(pushEnvelope);
}

static void hello_cb(dsvdc_t *handle, const char *dsuid, void *userdata)
{
    (void)handle;
    printf("Hello callback triggered, we are ready. "
           "Connected to vdsm %s\n", dsuid);
    bool *ready = (bool *)userdata;
    *ready = true;
}

static bool dsuid_compare(const char *dsuid, const char *ref_dsuid)
{
    size_t ctr;
    for (ctr = 0; ctr < DSUID_LENGTH; ++ctr)
    {
        if (dsuid[ctr] != toupper(ref_dsuid[ctr]))
        {
            return false;
        }
    }
    return true;
}

static void ping_cb(dsvdc_t *handle, const char *dsuid, void *userdata)
{
    int ret;
    (void)userdata;
    printf("ping_cb: received ping for device %s\n", dsuid);
    if (dsuid_compare(dsuid, g_lib_dsuid) ||
        dsuid_compare(dsuid, g_vdc_dsuid) ||
        dsuid_compare(dsuid, g_dev_dsuid))
    {
        ret = dsvdc_send_pong(handle, dsuid);
        printf("ping_cb: sent pong for device %s / return code %d\n", dsuid, ret);
    }
}

static void announce_device_cb(dsvdc_t *handle, int code, void *arg,
                               void *userdata)
{
    (void)handle;
    (void)userdata;
    printf("announce_device_cb: got code %d to announcement of device %s\n", code, (char *)arg);
}

static void announce_container_cb(dsvdc_t *handle, int code, void *arg,
                               void *userdata)
{
    (void)handle;
    (void)userdata;
    printf("announce_container_cb: got code %d to announcement of container %s\n", code, (char *)arg);
}

static void bye_cb(dsvdc_t *handle, const char *dsuid, void *userdata)
{
    (void)handle;
    printf("received bye, vdSM %s terminated our session\n", dsuid);
    bool *ready = (bool *)userdata;
    *ready = false;
}

static bool remove_cb(dsvdc_t *handle, const char *dsuid, void *userdata)
{
    (void)handle;
    (void)userdata;
    printf("received remove for dsuid %s\n", dsuid);

    return true;
}

static void setprop_cb(dsvdc_t *handle, const char *dsuid,
                       dsvdc_property_t *property,
                       const dsvdc_property_t *properties,
                       void *userdata)
{
    (void)userdata;
    int ret;
    uint8_t code = DSVDC_ERR_NOT_IMPLEMENTED;
    size_t i;
    printf("setprop_cb: request for dsuid \"%s\"\n",  dsuid);

    /*
     * Properties for the VDC
     */
    if (strcasecmp(g_dev_dsuid, dsuid) == 0)
    {
        for (i = 0; i < dsvdc_property_get_num_properties(properties); i++)
        {
            char *name;
            ret = dsvdc_property_get_name(properties, i, &name);
            if (ret != DSVDC_OK)
            {
                fprintf(stderr, "setprop_cb: error getting property name\n");
                code = DSVDC_ERR_MISSING_DATA;
                break;
            }

            if (!name)
            {
                fprintf(stderr, "setprop_cb: not yet handling wildcard "
                                "properties\n");
                code = DSVDC_ERR_NOT_IMPLEMENTED;
                break;
            }

            if (strcmp(name, "actionclick") == 0)
            {
                dsvdc_property_value_t vt;
                ret = dsvdc_property_get_value_type(properties, i, &vt);
                if (ret != DSVDC_OK)
                {
                    fprintf(stderr, "setprop_cb: error determining %s property "
                                    "value type\n", name);
                    code = DSVDC_ERR_INVALID_VALUE_TYPE;
                    free(name);
                    break;
                }

                if (vt != DSVDC_PROPERTY_VALUE_UINT64)
                {
                    fprintf(stderr, "setprop_cb: unexpected value type for "
                                    "property %s\n", name);
                    code = DSVDC_ERR_INVALID_VALUE_TYPE;
                    free(name);
                    break;
                }

                uint64_t clickType;
                ret = dsvdc_property_get_uint(properties, i, &clickType);
                if (ret != DSVDC_OK)
                {
                    fprintf(stderr, "setprop_cb: error getting property value, "
                                    "from property %s\n", name);
                    code = DSVDC_ERR_MISSING_DATA;
                    free(name);
                    break;
                }


                if ((clickType > 14) && (clickType != 255))
                {
                    fprintf(stderr, "setprop_cb: unsupoprted click type value");
                    code = DSVDC_ERR_INVALID_VALUE_TYPE;
                    free(name);
                    break;
                }

                code = DSVDC_OK;
                send_button_click(handle, clickType);
                free(name);
                break;
            }
            free(name);
        }
    }
    else
    {
        code = DSVDC_ERR_NOT_FOUND;
    }

    dsvdc_send_set_property_response(handle, property, code);
}

static void getprop_cb(dsvdc_t *handle, const char *dsuid,
                       dsvdc_property_t *property,
                       const dsvdc_property_t *query,
                       void *userdata)
{
    (void)userdata;
    int ret;
    size_t i;
    printf("getprop_cb: request for dsuid \"%s\"\n",  dsuid);

    /*
     * Properties for the VDC
     */
    if (strcasecmp(g_vdc_dsuid, dsuid) == 0)
    {
        for (i = 0; i < dsvdc_property_get_num_properties(query); i++)
        {
            char *name;
            int ret = dsvdc_property_get_name(query, i, &name);
            if (ret != DSVDC_OK)
            {
                fprintf(stderr, "getprop_cb: error getting property name, abort\n");
                dsvdc_send_get_property_response(handle, property);
                return;
            }
            if (!name)
            {
                fprintf(stderr, "getprop_cb: not yet handling wildcard properties\n");
                dsvdc_send_get_property_response(handle, property);
                return;
            }
            printf("getprop_cb: property name \"%s\"\n", name);

            if (strcmp(name, "hardwareGuid") == 0)
            {
              char info[256];
              char buffer[32];
              get_network_interface(buffer, 32);
              strcpy(info, "macaddress:");
              strcat(info, buffer);
              dsvdc_property_add_string(property, name, info);

            }
            else if (strcmp(name, "modelGuid") == 0)
            {
              dsvdc_property_add_string(property, name, "[modelGuid]");
            }
            else if (strcmp(name, "vendorId") == 0)
            {
              dsvdc_property_add_string(property, name, "digitalSTROM");
            }
            else if (strcmp(name, "name") == 0)
            {
              dsvdc_property_add_string(property, name, "libdSvDC reference application vDC");
            }
            else if (strcmp(name, "model") == 0)
            {
              dsvdc_property_add_string(property, name, "libdSvDC vDC");
            }
            else if (strcmp(name, "type") == 0)
            {
                dsvdc_property_add_string(property, name, "vDC");
            }
            else if (strcmp(name, "capabilities") == 0)
            {
                dsvdc_property_t *reply;
                ret = dsvdc_property_new(&reply);
                if (ret != DSVDC_OK)
                {
                    printf("failed to allocate reply property for %s\n", name);
                    free(name);
                    continue;
                }
                dsvdc_property_add_bool(reply, "metering", false);
            }
            else if (strcmp(name, "configURL") == 0)
            {
                dsvdc_property_add_string(property, name,
                                          "https://localhost:11111");
            }
            free(name);
        }
        dsvdc_send_get_property_response(handle, property);
        return;
    }

    /*
     * Properties for the VDC
     */
    if (strcasecmp(g_dev_dsuid, dsuid) != 0)
    {
        printf("getprop_cb: unhandled dsuid \"%s\"\n", dsuid);
        return;
    }

    for (i = 0; i < dsvdc_property_get_num_properties(query); i++)
    {
        char *name;
        ret = dsvdc_property_get_name(query, i, &name);
        if (ret != DSVDC_OK)
        {
            printf("getprop_cb: error getting property name, abort\n");
            dsvdc_send_get_property_response(handle, property);
            return;
        }

        if (!name)
        {
            printf("getprop_cb: empty name, not yet handling wildcard properties\n");
            dsvdc_send_get_property_response(handle, property);
            return;
        }

        printf("getprop_cb: property name %s\n", name);

        if (strcmp(name, "outputDescription") == 0)
        {
            dsvdc_property_t *reply;
            ret  = dsvdc_property_new(&reply);
            if (ret != DSVDC_OK)
            {
                printf("failed to allocate reply property for %s\n", name);
                free(name);
                continue;
            }
            /* we have only one output */

            /* name of the output */
            dsvdc_property_add_string(reply, "name", "Reference table light");

            /* output supports dimming */
            dsvdc_property_add_uint(reply, "function", 1);

            /* output usage: "undefined" */
            dsvdc_property_add_uint(reply, "outputUsage", 0);

            /* no support for variable ramps */
            dsvdc_property_add_bool(reply, "variableRamp", false);

            /* max power 100W */
            dsvdc_property_add_uint(reply, "maxPower", 100);

            /* minimum dimming value 0, we don't support dimming at all */
            dsvdc_property_add_uint(reply, "minDim", 0);

            dsvdc_property_add_property(property, name, &reply);
        }
        else if (strcmp(name, "buttonInputDescriptions") == 0)
        {
            dsvdc_property_t *reply, *bProp;
            ret  = dsvdc_property_new(&reply);
            if (ret != DSVDC_OK)
            {
                printf("failed to allocate reply property for %s\n", name);
                free(name);
                continue;
            }
            ret  = dsvdc_property_new(&bProp);
            if (ret != DSVDC_OK)
            {
                printf("failed to allocate reply button property for %s\n", name);
                dsvdc_property_free(reply);
                free(name);
                continue;
            }

            dsvdc_property_add_string(bProp, "name", "keyboard button");
            dsvdc_property_add_bool(bProp, "supportsLocalKeyMode", true);
            dsvdc_property_add_uint(bProp, "buttonType", 1);
            dsvdc_property_add_uint(bProp, "buttonElementID", 0);

            dsvdc_property_add_property(reply, "0", &bProp);
            dsvdc_property_add_property(property, name, &reply);
        }
        else if (strcmp(name, "primaryGroup") == 0)
        {
            /* group 1 / Yellow, we imitate a simple lamp */
            dsvdc_property_add_uint(property, "primaryGroup", 1);
        }
        else if (strcmp(name, "binaryInputDescriptions") == 0)
        {
            /* no binary inputs for this device */
        }
        else if (strcmp(name, "sensorDescriptions") == 0)
        {
            dsvdc_property_t *reply, *sProp1, *sProp2;
            dsvdc_property_new(&reply);
            dsvdc_property_new(&sProp1);
            dsvdc_property_new(&sProp2);
            if (reply == NULL || sProp1 == NULL || sProp2 == NULL)
            {
                printf("failed to allocate reply sensor property for %s\n", name);
                if (reply)  dsvdc_property_free(reply);
                if (sProp1) dsvdc_property_free(sProp1);
                if (sProp2) dsvdc_property_free(sProp2);
                free(name);
                continue;
            }

            dsvdc_property_add_string(sProp1, "name", "Energy Meter");
            dsvdc_property_add_uint(sProp1, "sensorType", 16);
            dsvdc_property_add_uint(sProp1, "sensorUsage", 0);
            dsvdc_property_add_double(sProp1, "aliveSignInterval", 300);
            dsvdc_property_add_property(reply, "0", &sProp1);

            dsvdc_property_add_string(sProp2, "name", "Power");
            dsvdc_property_add_uint(sProp2, "sensorType", 14);
            dsvdc_property_add_uint(sProp2, "sensorUsage", 0);
            dsvdc_property_add_double(sProp2, "aliveSignInterval", 300);
            dsvdc_property_add_property(reply, "1", &sProp2);

            dsvdc_property_add_property(property, name, &reply);
        }
        else if (strcmp(name, "buttonInputSettings") == 0)
        {
            dsvdc_property_t *reply, *bProp;
            ret  = dsvdc_property_new(&reply);
            if (ret != DSVDC_OK)
            {
                printf("failed to allocate reply property for %s\n", name);
                free(name);
                continue;
            }
            ret  = dsvdc_property_new(&bProp);
            if (ret != DSVDC_OK)
            {
                printf("failed to allocate reply button property for %s\n", name);
                dsvdc_property_free(reply);
                free(name);
                continue;
            }

            dsvdc_property_add_uint(bProp, "group", 1);
            dsvdc_property_add_uint(bProp, "function", 5);
            dsvdc_property_add_uint(bProp, "mode", 0);
            dsvdc_property_add_bool(bProp, "setsLocalPriority", true);
            dsvdc_property_add_bool(bProp, "callsPresent", true);

            dsvdc_property_add_property(reply, "0", &bProp);
            dsvdc_property_add_property(property, name, &reply);
        }
        else if (strcmp(name, "outputSettings") == 0)
        {
            dsvdc_property_t *reply;
            dsvdc_property_t *groups;
            ret  = dsvdc_property_new(&reply);
            if (ret != DSVDC_OK)
            {
                printf("failed to allocate reply property for %s\n", name);
                free(name);
                continue;
            }
            ret  = dsvdc_property_new(&groups);
            if (ret != DSVDC_OK)
            {
                printf("failed to allocate groups property for %s\n", name);
                dsvdc_property_free(reply);
                free(name);
                continue;
            }

            dsvdc_property_add_bool(groups, "0", true);
            dsvdc_property_add_bool(groups, "1", true);
            dsvdc_property_add_property(reply, "groups", &groups);
            dsvdc_property_add_uint(reply, "mode", 2);
            dsvdc_property_add_bool(reply, "pushChanges", true);

            dsvdc_property_add_property(property, name, &reply);
        }
        else if (strcmp(name, "channelDescriptions") == 0)
        {
            /* no channel descriptions for this device */
        }
        else if (strcmp(name, "name") == 0)
        {
            if (strcmp(dsuid, g_vdc_dsuid) == 0)
            {
                dsvdc_property_add_string(property, name, "Reference libdSvDC");
            }
            else
            {
                dsvdc_property_add_string(property, name, "Reference Light");
            }
        }
        else if (strcmp(name, "hardwareGuid") == 0)
        {
            char mac[32];
            char hardwareGuid[64];
            get_network_interface(mac, 32);
            strcpy(hardwareGuid, "macaddress:");
            strcat(hardwareGuid, mac);
            dsvdc_property_add_string(property, name, hardwareGuid);
        }
        else if (strcmp(name, "hardwareModelGuid") == 0)
        {
            dsvdc_property_add_string(property, name, "gs1:0100000");
        }
        else if (strcmp(name, "modelUID") == 0)
        {
            dsvdc_property_add_string(property, name, "libdsvdc:reference");
        }
        else if ((strcmp(name, "modelVersion") == 0) ||
                 (strcmp(name, "hardwareVersion") == 0))
        {
            dsvdc_property_add_string(property, name, VERSION);
        }
        else if (strcmp(name, "modelFeatures") == 0)
        {
            dsvdc_property_t *reply;
            ret  = dsvdc_property_new(&reply);
            if (ret != DSVDC_OK)
            {
                printf("failed to allocate reply property for %s\n", name);
                free(name);
                continue;
            }

            dsvdc_property_add_bool(reply, "dontcare", true);
            dsvdc_property_add_bool(reply, "blink", true);
            dsvdc_property_add_bool(reply, "ledauto", false);
            dsvdc_property_add_bool(reply, "transt", false);
            dsvdc_property_add_bool(reply, "outmode", true);
            dsvdc_property_add_bool(reply, "outvalue8", true);
            dsvdc_property_add_bool(reply, "pushbutton", true);
            dsvdc_property_add_bool(reply, "pushbdevice", true);
            dsvdc_property_add_bool(reply, "pushbarea", true);
            dsvdc_property_add_bool(reply, "pushbadvanced", true);

            dsvdc_property_add_property(property, name, &reply);
        }
        else if (strcmp(name, "type") == 0)
        {
            dsvdc_property_add_string(property, name, "vDSD");
        }
        else if (strcmp(name, "model") == 0)
        {
            dsvdc_property_add_string(property, name,
                                      "libdSvDC reference application");
        }
        else if (strcmp(name, "deviceIcon16") == 0)
        {
            dsvdc_property_add_bytes(property, name, deviceIcon16_png,
                                     sizeof(deviceIcon16_png));
        }
        else if (strcmp(name, "configURL") == 0)
        {
            dsvdc_property_add_string(property, name,
                "https://gitorious.digitalstrom.org/virtual-devices/libdsvdc");
        }
        else if (strcmp(name, "scenes") == 0)
        {
            /* TODO: add scene value/configuration property handling */
        }
        free(name);
    }
    dsvdc_send_get_property_response(handle, property);
}

static void callscene_cb(dsvdc_t *handle, char **dsuid, size_t n_dsuid, int32_t scene,
                         bool force, int32_t group, int32_t zone_id, void *userdata)
{
    (void) handle;
    (void) group;
    (void) zone_id;
    (void) userdata;
    size_t n;

    for (n = 0; n < n_dsuid; n++)
    {
        printf("received %scall scene for device %s\n", force?"forced ":"", *dsuid);
    }

    switch (scene)
    {
        case 0:
            printf("call activity off\n");
            break;
        case 5:
            printf("call activity 1\n");
            break;
        case 17:
            printf("call activity 2\n");
            break;
        case 18:
            printf("call activity 3\n");
            break;
        case 19:
            printf("call activity 4\n");
            break;
        case 11:
            printf("call activity decrement\n");
            break;
        case 12:
            printf("call activity increment\n");
            break;
        case 13:
            printf("call activity min\n");
            break;
        case 14:
            printf("call activity max\n");
            break;
        case 40:
            printf("call activity auto-off\n");
            break;
        case 50:
            printf("call activity local-off\n");
            break;
        case 51:
            printf("call activity local-on\n");
            break;
        case 65:
            printf("call activity panic\n");
            break;
        case 67:
            printf("call activity standby\n");
            break;
        case 68:
            printf("call activity deep-off\n");
            break;
        case 69:
            printf("call activity sleeping\n");
            break;
        case 70:
            printf("call activity wakeup\n");
            break;
        case 71:
            printf("call activity present\n");
            break;
        case 72:
            printf("call activity absent\n");
            break;
        case 73:
            printf("call activity door bell\n");
            break;
        default:
            printf("scene %d not handled\n", scene);
            break;
    }

    /*
     * TODO: implement scene - output state handling
     *
    if (g_scene_config[scene] & effect-flag)
    {
    }
    if (g_scene_config[scene] & !dont-care)
    {
    }
    */
}

static void savescene_cb(dsvdc_t *handle, char **dsuid, size_t n_dsuid, int32_t scene,
                         int32_t group, int32_t zone_id, void *userdata)
{
    (void) handle;
    (void) userdata;
    size_t n;

    for (n = 0; n < n_dsuid; n++)
    {
        printf("received savescene for device %s: zone %d, group %d, scene %d\n",
                *dsuid, zone_id, group, scene);
    }
}

static void blink_cb(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
                         int32_t group, int32_t zone_id, void *userdata)
{
    (void) handle;
    (void) userdata;
    size_t n;

    for (n = 0; n < n_dsuid; n++)
    {
        printf("received blink for device %s: zone %d, group %d\n", *dsuid, zone_id, group);
    }
}

static void output_channel_value_cb(dsvdc_t *handle, char **dsuid, size_t n_dsuid,
        bool apply, int32_t channel, double value, void *userdata)
{
    (void) handle;
    (void) userdata;
    size_t n;

    for (n = 0; n < n_dsuid; n++)
    {
        printf("received output value for device %s: channel %d, value %.2f, apply-flag %d\n",
                *dsuid, channel, value, apply);
    }
}

unsigned int random_in_range(unsigned int min, unsigned int max)
{
    double scaled = (double)random()/RAND_MAX;
    return (max - min + 1) * scaled + min;
}

int main(int argc, char **argv)
{
    struct sigaction action;

    bool ready = false;
    bool announced = false;
    bool printed = false;

    int opt_index = 0;
    int o;
    bool random = false;

    struct option long_options[] =
    {
        { "random", 0, 0, 'r'           },
        { "library-dsuid", 1, 0, 'l'    },
        { "container-dsuid", 1, 0, 'c'  },
        { "device-dsuid", 1, 0, 'd'     },
        { "help", 0, 0, 'h'             },
        { 0, 0, 0, 0                    }
    };

    /* generate a host/mac based dsuid */
    dsuid_t dsuid;
    char mac[40];
    get_network_interface(mac, 32);

    dsuid_generate_v3_from_namespace(DSUID_NS_IEEE_MAC, mac, &dsuid);
    dsuid_to_string(&dsuid, g_vdc_dsuid);

    strcat(mac, "-0000");
    dsuid_generate_v3_from_namespace(DSUID_NS_IEEE_MAC, mac, &dsuid);
    dsuid_to_string(&dsuid, g_dev_dsuid);

    strncpy(mac + strlen(mac) - strlen("0000"), "1111", 4);
    dsuid_generate_v3_from_namespace(DSUID_NS_IEEE_MAC, mac, &dsuid);
    dsuid_to_string(&dsuid, g_lib_dsuid);

    print_copyright();

    while (1)
    {
        o = getopt_long(argc, argv, OPTSTR, long_options, &opt_index);
        if (o == -1) break;

        switch (o)
        {
            case 'r':
                random = true;
                break;
            case 'l':
                strncpy(g_lib_dsuid, optarg, sizeof(g_lib_dsuid));
                break;
            case 'c':
                strncpy(g_vdc_dsuid, optarg, sizeof(g_lib_dsuid));
                break;
            case 'd':
                strncpy(g_dev_dsuid, optarg, sizeof(g_lib_dsuid));
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
                break;
        }
    }

    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    action.sa_flags = 0;
    sigfillset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) < 0)
    {
        fprintf(stderr, "Could not register SIGINT handler!\n");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGTERM, &action, NULL) < 0)
    {
        fprintf(stderr, "Could not register SIGTERM handler!\n");
        return EXIT_FAILURE;
    }

    if (random)
    {
        /* randomize dsuid's so that we can run more than one example on the
           same machine */
        srandom(time(NULL));
        snprintf(g_lib_dsuid + (strlen(g_vdc_dsuid) - 3), 4, "%u",
                 random_in_range(100,999));
        snprintf(g_vdc_dsuid + (strlen(g_vdc_dsuid) - 3), 4, "%u",
                 random_in_range(100,999));
        snprintf(g_dev_dsuid + (strlen(g_vdc_dsuid) - 3), 4, "%u",
                 random_in_range(100,999));
    }
    printf("libdSvDC/%s example test program, press Ctrl-C to quit\n",
           g_vdc_dsuid);

    dsvdc_t *handle = NULL;

    /* initialize new library instance */
    if (dsvdc_new(0, g_lib_dsuid, "Example vDC", &ready, &handle) != DSVDC_OK)
    {
        fprintf(stderr, "dsvdc_new() initialization failed\n");
        return EXIT_FAILURE;
    }

    /* connection callbacks */
    dsvdc_set_hello_callback(handle, hello_cb);
    dsvdc_set_ping_callback(handle, ping_cb);
    dsvdc_set_bye_callback(handle, bye_cb);
    dsvdc_set_remove_callback(handle, remove_cb);

    /* device callbacks */
    dsvdc_set_get_property_callback(handle, getprop_cb);
    dsvdc_set_set_property_callback(handle, setprop_cb);
    dsvdc_set_call_scene_notification_callback(handle, callscene_cb);
    dsvdc_set_identify_notification_callback(handle, blink_cb);
    dsvdc_set_save_scene_notification_callback(handle, savescene_cb);
    dsvdc_set_output_channel_value_callback(handle, output_channel_value_cb);

    while(!g_shutdown_flag)
    {
        /* let the work function do our timing, 2secs timeout */
        dsvdc_work(handle, 2);
        if (!dsvdc_is_connected(handle))
        {
            if (!printed)
            {
                fprintf(stderr, "vdC example: we are not connected!\n");
                printed = true;
            }
            announced = false;
            ready = false;
        }
        else
        {
            printed = false;
            if (ready && !announced)
            {
                int ret;
                ret = dsvdc_announce_container(handle,
                        g_vdc_dsuid,
                        (void *) g_vdc_dsuid,
                        announce_container_cb);
                if (ret == DSVDC_OK)
                {
                    int ret = dsvdc_announce_device(handle,
                            g_vdc_dsuid,
                            g_dev_dsuid,
                            (void *) g_dev_dsuid,
                            announce_device_cb);
                    if (ret == DSVDC_OK)
                    {
                        announced = true;
                    }
                    else
                    {
                        printf("dsvdc_announce_device returned error %d\n", ret);
                    }
                }
                else
                {
                    printf("dsvdc_announce_container returned error %d\n", ret);
                }
            }
            if (!ready)
            {
                announced = false;
            }
        }
    }
    dsvdc_device_vanished(handle, g_dev_dsuid);
    dsvdc_cleanup(handle);

    return EXIT_SUCCESS;
}
