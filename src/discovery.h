/*
    Copyright (c) 2014 aizo ag, Zurich, Switzerland

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

#ifndef __DSVDC_DISCOVERY_H__
#define __DSVDC_DISCOVERY_H__

#include "common.h"

enum
{
    DSVDC_DISCOVERY_OK = 0,
    DSVDC_DISCOVERY_ERR_PARAM = -1
};

int dsvdc_discovery_init(dsvdc_t *handle, const char* name);
void dsvdc_discovery_work(dsvdc_t *handle);
void dsvdc_discovery_cleanup(dsvdc_t *handle);

#endif//__DSVDC_DISCOVERY_H__
