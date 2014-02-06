/*
    Copyright (c) 2014 digitalSTROM AG, Zurich, Switzerland

    Author: Sergey 'Jin' Bostandzhyan <jin@dev.digitalstrom.org>

    The code is based on the Avahi example:
    http://avahi.org/download/doxygen/client-publish-service_8c-example.html

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
#include <string.h>

#include <avahi-client/publish.h>

#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/address.h>
#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>

#include "discovery.h"
#include "log.h"


#define DEFAULT_AVAHI_SERVICE_NAME "digitalSTROM vDC"

static void dsvdc_avahi_create_services(dsvdc_t *handle, AvahiClient *client);

static void dsvdc_avahi_entry_group_callback(AvahiEntryGroup *group,
                                             AvahiEntryGroupState state,
                                             AVAHI_GCC_UNUSED void *userdata)
{
    if (userdata == NULL)
    {
        log("invalid userdata / missing handle\n");
        return;
    }

    dsvdc_t *handle = (dsvdc_t *)userdata;

    if (!(handle->avahi_group == group || group == NULL))
    {
        log("invalid group\n");
    }

    switch (state)
    {
        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            log("service \"%s\" successfully established.\n",
                handle->avahi_name);
            break;

        case AVAHI_ENTRY_GROUP_COLLISION:
        {
            char *new_name;

            new_name = avahi_alternative_service_name(handle->avahi_name);
            pthread_mutex_lock(&handle->dsvdc_handle_mutex);
            if (handle->avahi_name)
            {
                avahi_free(handle->avahi_name);
            }
            handle->avahi_name = new_name;
            pthread_mutex_unlock(&handle->dsvdc_handle_mutex);

            log("service name collision, renaming service to '%s'\n",
                handle->avahi_name);
            /* recreate the service */
            dsvdc_avahi_create_services(handle,
                                        avahi_entry_group_get_client(group));
            break;
        }
        case AVAHI_ENTRY_GROUP_FAILURE :
            log("entry group failure: %s\n",
                avahi_strerror(
                    avahi_client_errno(avahi_entry_group_get_client(group))));

            /* failure while registering service */
            avahi_simple_poll_quit(handle->avahi_poll);
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
        default:
            break;
    }
}

static void dsvdc_avahi_create_services(dsvdc_t *handle, AvahiClient *client)
{
    int ret = 0;
    char *new_name = NULL;

    if (!client)
    {
        return;
    }

    if (!handle)
    {
        return;
    }

    /* If this is the first time we're called, let's create a new
     * entry group if necessary */

    if (!handle->avahi_group)
    {
        pthread_mutex_lock(&handle->dsvdc_handle_mutex);
        handle->avahi_group = avahi_entry_group_new(client,
                                    dsvdc_avahi_entry_group_callback, handle);
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
        if (!handle->avahi_group)
        {
            log("avahi_entry_group_new() failed: %s\n",
                                    avahi_strerror(avahi_client_errno(client)));
            goto fail;
        }
    }

    /* add our entries if the group is empty */
    if (avahi_entry_group_is_empty(handle->avahi_group))
    {
        log("Adding service '%s'\n", handle->avahi_name);
        const char *key = "dSUID=";
        size_t len = strlen(key) + strlen(handle->vdc_dsuid) + 1;
        char *txt = malloc(len);
        if (txt)
        {
            snprintf(txt, len, "%s%s", key, handle->vdc_dsuid);
        }

        ret = avahi_entry_group_add_service(handle->avahi_group,
                                            AVAHI_IF_UNSPEC,
                                            AVAHI_PROTO_INET, 0,
                                            handle->avahi_name,
                                            "_ds-vdc._tcp", NULL,
                                            NULL, handle->port, txt,
                                            NULL);
        if (txt)
        {
            free(txt);
        }

        if (ret < 0)
        {
            if (ret == AVAHI_ERR_COLLISION)
            {
                goto collision;
            }

            log("could not add _ds-vdc._tcp service: %s\n",
                avahi_strerror(ret));
            goto fail;
        }

        /* the service */
        ret = avahi_entry_group_commit(handle->avahi_group);
        if (ret < 0)
        {
            log("could not commit entry group: %s\n", avahi_strerror(ret));
            goto fail;
        }
    }

    return;

collision:

    /* service name collision with a local service happened, pick a new name */
    new_name = avahi_alternative_service_name(handle->avahi_name);
    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    if (handle->avahi_name)
    {
        avahi_free(handle->avahi_name);
    }
    handle->avahi_name = new_name;
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);

    log("service name collision, renaming service to '%s'\n",
        handle->avahi_name);

    avahi_entry_group_reset(handle->avahi_group);

    dsvdc_avahi_create_services(handle, client);
    return;

fail:
    avahi_simple_poll_quit(handle->avahi_poll);
}

static void dsvdc_avahi_client_callback(AvahiClient *client,
                                        AvahiClientState state,
                                        AVAHI_GCC_UNUSED void *userdata)
{
    if (!client)
    {
        log("invalid client\n");
        return;
    }

    if (userdata == NULL)
    {
        log("invalid userdata / missing handle\n");
        return;
    }

    dsvdc_t *handle = (dsvdc_t *)userdata;

    switch (state)
    {
        case AVAHI_CLIENT_S_RUNNING:
            /* the server has startup successfully and registered its host
               name on the network, create our services */
            dsvdc_avahi_create_services(handle, client);
            break;

        case AVAHI_CLIENT_FAILURE:
            log("avahi client failure: %s\n",
                avahi_strerror(avahi_client_errno(handle->avahi_client)));

            avahi_simple_poll_quit(handle->avahi_poll);
            break;

        case AVAHI_CLIENT_S_COLLISION:
            /* Let's drop our registered services. When the server is back
             * in AVAHI_SERVER_RUNNING state we will register them
             * again with the new host name. */
        case AVAHI_CLIENT_S_REGISTERING:
            /* The server records are now being established. This
             * might be caused by a host name change. We need to wait
             * for our own records to register until the host name is
             * properly esatblished. */

            if (handle->avahi_group)
            {
                avahi_entry_group_reset(handle->avahi_group);
            }
            break;

        case AVAHI_CLIENT_CONNECTING:
        default:
            break;
    }
}

int dsvdc_discovery_init(dsvdc_t *handle, const char *name)
{
    int error = 0;

    if (!handle)
    {
        return DSVDC_DISCOVERY_ERR_PARAM;
    }


    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    handle->avahi_poll = avahi_simple_poll_new();
    if (!handle->avahi_poll)
    {
        log("could not allocate avahi poll handle\n");
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    if (!name)
    {
        handle->avahi_name = avahi_strdup(DEFAULT_AVAHI_SERVICE_NAME);
    }
    else
    {
        handle->avahi_name = avahi_strdup(name);
    }

    if (!handle->avahi_name)
    {
        log("could not allocate memory for avahi service name\n");
        avahi_simple_poll_free(handle->avahi_poll);
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    handle->avahi_client = avahi_client_new(
                                avahi_simple_poll_get(handle->avahi_poll), 0,
                                dsvdc_avahi_client_callback, (void *)handle,
                                &error);
    if (!handle->avahi_client)
    {
        log("could not allocate avahi client: %s\n", avahi_strerror(error));
        avahi_simple_poll_free(handle->avahi_poll);
        avahi_free(handle->avahi_name);
        pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);

    return DSVDC_DISCOVERY_OK;
}

void dsvdc_discovery_work(dsvdc_t *handle)
{
    if (!handle)
    {
        log("discovery worker called with invalid handle\n");
        return;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);
    /* check justone event and return immediately */
    if (avahi_simple_poll_iterate(handle->avahi_poll, 0) != 0)
    {
        /* if daemon connection is lost we need to reinit */
        log("avahi error, reinitializing...\n");
        char *temp_name = strdup(handle->avahi_name);
        dsvdc_discovery_cleanup(handle);
        dsvdc_discovery_init(handle, temp_name);
        if (temp_name)
        {
            free(temp_name);
        }
    }
    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

void dsvdc_discovery_cleanup(dsvdc_t *handle)
{
    if (!handle)
    {
        return;
    }

    pthread_mutex_lock(&handle->dsvdc_handle_mutex);

    if (handle->avahi_group)
    {
        avahi_entry_group_free(handle->avahi_group);
        handle->avahi_group = NULL;
    }

    if (handle->avahi_client)
    {
        avahi_client_free(handle->avahi_client);
        handle->avahi_client = NULL;
    }

    if (handle->avahi_poll)
    {
        avahi_simple_poll_free(handle->avahi_poll);
        handle->avahi_poll = NULL;
    }

    if (handle->avahi_name)
    {
        avahi_free(handle->avahi_name);
        handle->avahi_name = NULL;
    }

    pthread_mutex_unlock(&handle->dsvdc_handle_mutex);
}

