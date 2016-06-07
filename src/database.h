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

#ifndef __DSVDC_DATABASE_H__
#define __DSVDC_DATABASE_H__

#include <gdbm.h>

#if __GNUC__ >= 4
    #pragma GCC visibility push(hidden)
#endif

struct dsvdc_database
{
	GDBM_FILE dbf;
    bool rw;
};

#if __GNUC__ >= 4
    #pragma GCC visibility pop
#endif

#endif// __DSVDC_DATABASE_H__
