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
static char g_vdc_dsuid[25] = { "3504175FE0000000BC514CBE" };
static char g_dev_dsuid[25] = { "3504175FE000000098BD8E80" };

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

static void announce_cb(dsvdc_t *handle, int code, void *arg, void *userdata)
{
    (void)handle;
    (void)userdata;
    printf("got code %d to announcement of device %s\n", code, (char *)arg);
}

static void bye_cb(dsvdc_t *handle, void *userdata)
{
    (void)handle;
    printf("received bye, vdSM terminated our session\n");
    bool *ready = (bool *)userdata;
    *ready = false;
}

static void getprop_cb(dsvdc_t *handle, const char *dsuid, const char *name,
                       uint32_t offset, uint32_t count,
                       dsvdc_property_t *property, void *userdata)
{
    int i;
    (void)offset;
    (void)count;
    (void)userdata;
    printf("received get property callback for device %s\n", dsuid);
    if (strcmp(name, "buttonInputSettings") == 0)
    {
        dsvdc_property_add_uint(property, 0, "group", 1);
        dsvdc_property_add_uint(property, 0, "function", 5);
        dsvdc_property_add_uint(property, 0, "mode", 0);
        dsvdc_send_property_response(handle, property);
    }
    else if (strcmp(name, "buttonInputDescriptions") == 0)
    {
        dsvdc_send_property_response(handle, property);
    }
    else if (strcmp(name, "primaryGroup") == 0)
    {
        dsvdc_property_add_uint(property, 0, "primaryGroup", 1);
        dsvdc_send_property_response(handle, property);
    }
    else if (strcmp(name, "outputSettings") == 0)
    {
        dsvdc_property_add_uint(property, 0, "group", 1);
        dsvdc_property_add_uint(property, 0, "mode", 1);
        dsvdc_send_property_response(handle, property);
    }
    else if (strcmp(name, "name") == 0)
    {
        dsvdc_property_add_string(property, 0, "name", "libdSvDC");
        dsvdc_send_property_response(handle, property);
    }
    else if (strcmp(name, "isMember") == 0)
    {
        for (i = 0; i < 64; i++)
        {
            dsvdc_property_add_bool(property, i, "isMember", (i == 1));
        }
        dsvdc_send_property_response(handle, property);
    }
    else
    {
        dsvdc_property_free(property);
    }
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
    if (dsvdc_new(0, g_vdc_dsuid, "Example vDC", &ready, &handle) != DSVDC_OK)
    {
        fprintf(stderr, "dsvdc_new() initialization failed\n");
        return EXIT_FAILURE;
    }

    /* setup callbacks */
    dsvdc_set_hello_callback(handle, hello_cb);
    dsvdc_set_ping_callback(handle, ping_cb);
    dsvdc_set_bye_callback(handle, bye_cb);
    dsvdc_set_get_property_callback(handle, getprop_cb);

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
                if (dsvdc_announce_device(handle, g_dev_dsuid,
                                         (void *)g_dev_dsuid, announce_cb) ==
                        DSVDC_OK)
                {
                    announced = true;
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
