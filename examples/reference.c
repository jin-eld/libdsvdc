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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include "dsvdc.h"

static int g_shutdown_flag = 0;
/* "library" dsuid is currently unused in the vdsm */
static char g_lib_dsuid[35] = { "053f848b85bb382198025cea1fd087f100" };
static char g_vdc_dsuid[35] = { "c38ec6e15c2e35edbde53a37330042f500" };
static char g_dev_dsuid[35] = { "1bb9eb4200c330698d84a896f916623300" };

void signal_handler(int signum)
{
    if ((signum == SIGINT) || (signum == SIGTERM))
    {
        g_shutdown_flag++;
    }
}

static void hello_cb(dsvdc_t *handle, void *userdata)
{
    (void)handle;
    printf("Hello callback triggered, we are ready\n");
    bool *ready = (bool *)userdata;
    *ready = true;
}

static void ping_cb(dsvdc_t *handle, const char *dsuid, void *userdata)
{
    int ret;
    (void)userdata;
    printf("received ping for device %s\n", dsuid);
    if ((strcmp(dsuid, g_vdc_dsuid) == 0) ||
        (strcmp(dsuid, g_dev_dsuid) == 0))
    {
        ret = dsvdc_send_pong(handle, dsuid);
        printf("sent pong for device %s / return code %d\n", dsuid, ret);
    }
}

static void announce_device_cb(dsvdc_t *handle, int code, void *arg,
                               void *userdata)
{
    (void)handle;
    (void)userdata;
    printf("got code %d to announcement of device %s\n", code, (char *)arg);
}

static void announce_container_cb(dsvdc_t *handle, int code, void *arg,
                               void *userdata)
{
    (void)handle;
    (void)userdata;
    printf("got code %d to announcement of device %s\n", code, (char *)arg);
    int ret = dsvdc_announce_device(handle, g_vdc_dsuid, g_dev_dsuid,
                                    (void *)g_dev_dsuid, announce_device_cb);
    if (ret != DSVDC_OK)
    {
        printf("dsvdc_announce_device returned error %d\n", ret);
    }
}

static void bye_cb(dsvdc_t *handle, const char *dsuid, void *userdata)
{
    (void)handle;
    printf("received bye, vdSM %s terminated our session\n", dsuid);
    bool *ready = (bool *)userdata;
    *ready = false;
}

static void getprop_cb(dsvdc_t *handle, const char *dsuid,
                       dsvdc_property_t *property,
                       const dsvdc_property_t *query,
                       void *userdata)
{
    (void)userdata;
    int ret;
    size_t i;
    printf("\n**** get property %s\n",  dsuid);


    for (i = 0; i < dsvdc_property_get_num_properties(query); i++)
    {
        char *name;
        ret = dsvdc_property_get_name(query, i, &name);
        if (ret != DSVDC_OK)
        {
            dsvdc_send_property_response(handle, property);
            return;
        }

        if (!name) {
            printf("not yet handling wildcard properties\n");
            dsvdc_send_property_response(handle, property);
            return;
        }

        printf("processing property request for %s\n", name);

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
            dsvdc_property_add_string(reply, "name", "libdSvDC output");

            /* output supports on/off modes only */
            dsvdc_property_add_uint(reply, "function", 0);

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
            dsvdc_property_t *reply;
            ret  = dsvdc_property_new(&reply);
            if (ret != DSVDC_OK)
            {
                printf("failed to allocate reply property for %s\n", name);
                free(name);
                continue;
            }

            /* human readable name/number for the input */
            dsvdc_property_add_string(reply, "name", "virtual button");
            dsvdc_property_add_bool(reply, "supportsLocalKeyMode", false);

            /* type of physical button: single pushbutton */
            dsvdc_property_add_uint(reply, "buttonType", 1);

            /* element of multi-contact button: center */
            dsvdc_property_add_uint(reply, "buttonElementID", 0);

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
            /* TODO: reply with an element with empty value or skip it? */
        }
        else if (strcmp(name, "sensorDescriptions") == 0)
        {
            /* no sensor descriptions for this device */
            /* TODO: reply with an element with empty value or skip it? */
        }
        else if (strcmp(name, "buttonInputSettings") == 0)
        {
            dsvdc_property_t *reply;
            ret  = dsvdc_property_new(&reply);
            if (ret != DSVDC_OK)
            {
                printf("failed to allocate reply property for %s\n", name);
                free(name);
                continue;
            }

            dsvdc_property_add_uint(reply, "group", 1);

            /* supports on/off modes only */
            dsvdc_property_add_uint(reply, "function", 0);

            /* mode "standard" */
            dsvdc_property_add_uint(reply, "mode", 0);

            dsvdc_property_add_bool(reply, "setsLocalPriority", false);
            dsvdc_property_add_bool(reply, "callsPresent", false);

            dsvdc_property_add_property(property, name, &reply);
        }
        else if (strcmp(name, "outputSettings") == 0)
        {
            dsvdc_property_t *reply;
            ret  = dsvdc_property_new(&reply);
            if (ret != DSVDC_OK)
            {
                printf("failed to allocate reply property for %s\n", name);
                free(name);
                continue;
            }

            dsvdc_property_add_uint(reply, "group", 1);
            dsvdc_property_add_uint(reply, "mode", 1);

            dsvdc_property_add_property(property, name, &reply);
        }
        else if (strcmp(name, "channelDescriptions") == 0)
        {
            /* no channel descriptions for this device */
            /* TODO: reply with an element with empty value or skip it? */
        }
        else if (strcmp(name, "name") == 0)
        {
            dsvdc_property_add_string(property, "name", "libdSvDC");
        }
        free(name);
    }

    dsvdc_send_property_response(handle, property);
}

unsigned int random_in_range(unsigned int min, unsigned int max)
{
    double scaled = (double)random()/RAND_MAX;
    return (max - min + 1) * scaled + min;
}

int main()
{
    struct sigaction action;

    bool ready = false;
    bool announced = false;
    bool printed = false;

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

    /* randomize dsuid's so that we can run more than one example on the
       same machine */
    srandom(time(NULL));
    snprintf(g_vdc_dsuid + (strlen(g_vdc_dsuid) - 3), 4, "%u",
             random_in_range(100,999));
    snprintf(g_dev_dsuid + (strlen(g_vdc_dsuid) - 3), 4, "%u",
             random_in_range(100,999));

    printf("libdSvDC/%s example test program, press Ctrl-C to quit\n",
           g_vdc_dsuid);

    dsvdc_t *handle = NULL;

    /* initialize new library instance */
    if (dsvdc_new(0, g_lib_dsuid, "Example vDC", &ready, &handle) != DSVDC_OK)
    {
        fprintf(stderr, "dsvdc_new() initialization failed\n");
        return EXIT_FAILURE;
    }

    /* setup callbacks */
    dsvdc_set_hello_callback(handle, hello_cb);
    dsvdc_set_ping_callback(handle, ping_cb);
    dsvdc_set_bye_callback(handle, bye_cb);
    dsvdc_set_get_property_callback(handle, getprop_cb);

    int count = 0;
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
            count = 0;
        }
        else
        {
            printed = false;
            if (ready && !announced)
            {
                if (dsvdc_announce_container(handle, g_vdc_dsuid,
                                            (void *)g_lib_dsuid,
                                             announce_container_cb) == DSVDC_OK)
                {
                    announced = true;
                    count = 1;
                }
            }
            if (!ready)
            {
                announced = false;
            }
        }

        /* the below code is just a test for the vdSM, it will periodically
           send an "identify device" message */
        if (count > 0)
        {
            count++;
            if (count == 30)
            {
                dsvdc_identify_device(handle, g_dev_dsuid);
                count = 1;
            }
        }
    }
    dsvdc_device_vanished(handle, g_dev_dsuid);
    dsvdc_cleanup(handle);

    return EXIT_SUCCESS;
}
