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

#ifdef DEBUG

#include <stdarg.h>
#include <stdio.h>

#include "log.h"

void dsvdc_log(const char *format, const char *function, ...)
{
    va_list ap;
    va_start(ap, function);
    fprintf(stderr, "%s(): ", function);
    vfprintf(stderr, format, ap);
    fflush(stderr);
    va_end(ap);
}

#endif/*DEBUG*/

