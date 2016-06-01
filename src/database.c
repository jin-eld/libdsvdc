/*
    Copyright (c) 2016 digitalSTROM AG, Zurich, Switzerland

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
    along with libdsvdc. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "dsvdc.h"
#include "log.h"
#include "properties.h"
#include "database.h"
#include "util.h"

int dsvdc_database_open(const char *filename, bool rw,
                        dsvdc_database_t **db)
{
	*db = NULL;

	dsvdc_database_t *d = malloc(sizeof(struct dsvdc_database));
	if (!d)
    {
        log("could not allocate new database instance\n");
        return DSVDC_ERR_OUT_OF_MEMORY; 
    }

    int flags = 0;
    int mode = 0;

    if (!rw) /* read only mode */
    {
        if (!file_exists(filename))
        {
            log("database file \"%s\" was not found\n", filename);
            free(d);
            return DSVDC_ERR_FILE_NOT_FOUND;
        }

        flags = GDBM_READER;
        d->rw = false;
    }
    else
    {
        flags = GDBM_WRCREAT | GDBM_SYNC;
        mode = 0644;
        d->rw = true;
    }

    /* use system block size (selected by bs value < 512), open in read only
     * mode */
    d->dbf = gdbm_open(filename, 0, flags, mode, NULL);
    if (d->dbf == NULL)
    {
        log("failed to open database %s: %s\n", filename, 
                                                gdbm_strerror(gdbm_errno));
        free(d);
    }

    *db = d;
    return DSVDC_OK;
}


void dsvdc_database_close(dsvdc_database_t *db)
{
    if (!db)
    {
        return;
    }

    if (db->dbf)
    {
        gdbm_close(db->dbf);
    }

    free(db);
}

/* In the database saved properties are wrapped inside a 
 * Vdcapi__VdcSendPushProperty message. */
int dsvdc_database_load_property(const dsvdc_database_t *db, const char *key, dsvdc_property_t **property)
{
    int ret;
    *property = NULL;

    if (db == NULL)
    {
        log("no database given\n");
        return DSVDC_ERR_PARAM;
    }

    if (key == NULL)
    {
        log("no properties to get, key is empty");
        return DSVDC_ERR_PARAM;
    }

    datum dkey;
    dkey.dptr = (char*)key;
    dkey.dsize = strlen(key);

    datum data;
    data = gdbm_fetch(db->dbf, dkey);

    if (data.dptr == NULL)
    {
        log("key \"%s\" was not found in the database\n", key);
        return DSVDC_ERR_DATA_NOT_FOUND;
    }

    ret = dsvdc_property_new(property);
    if (ret != DSVDC_OK)
    {
		free(data.dptr);
        return ret;
    }

    Vdcapi__VdcSendPushProperty *msg = vdcapi__vdc__send_push_property__unpack(
                                NULL, data.dsize, (const uint8_t *)data.dptr);
    free(data.dptr);
    if (!msg)
    {
        log("could not unpack property data for key \"%s\"\n", key);
        dsvdc_property_free(*property);
        return DSVDC_ERR_INVALID_PROPERTY;
    }

    ret = DSVDC_OK;

    (*property)->n_properties = msg->n_properties;
    if (msg->n_properties > 0)
    {
        (*property)->properties = dsvdc_property_deep_copy(msg->properties,
                                                           msg->n_properties);
        if ((*property)->properties == NULL)
        {
            dsvdc_property_free(*property);
            ret = DSVDC_ERR_OUT_OF_MEMORY;
        }
    }

    vdcapi__vdc__send_push_property__free_unpacked(msg, NULL);
           
    return ret;
}

int dsvdc_database_save_property(const dsvdc_database_t *db,
                                 const char *key, dsvdc_property_t *property)
{
    if (db == NULL)
    {
        log("no database given\n");
        return DSVDC_ERR_PARAM;
    }

    if (key == NULL)
    {
        log("no key specified");
        return DSVDC_ERR_PARAM;
    }

    if (property == NULL)
    {
        log("no data specified");
        return DSVDC_ERR_PARAM;
    }

    /* wrap property in a Vdcapi__VdcSendPushProperty message */
    Vdcapi__VdcSendPushProperty msg = VDCAPI__VDC__SEND_PUSH_PROPERTY__INIT;
    msg.n_properties = property->n_properties;
    msg.properties = property->properties;

    datum dkey;
    dkey.dptr = (char*)key;
    dkey.dsize = strlen(key);

    datum data;

    data.dsize = (int)vdcapi__vdc__send_push_property__get_packed_size(&msg);
    data.dptr = (char *)malloc(data.dsize * sizeof(uint8_t));
    if (!data.dptr)
    {
        log("could not allocate memory to save data for key %s\n", key);
        return DSVDC_ERR_OUT_OF_MEMORY;
    }

    vdcapi__vdc__send_push_property__pack(&msg, (uint8_t *)(data.dptr));
    int ret = gdbm_store(db->dbf, dkey, data, GDBM_REPLACE);

    free(data.dptr);

    if (ret != 0)
    {
        log("failed to store record for key %s: %s\n", key,
             gdbm_strerror(gdbm_errno));
        return DSVDC_ERR_DATABASE;
    }

    return DSVDC_OK;
}
