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

#ifndef __DSVDC_LOG_H__
#define __DSVDC_LOG_H__

#ifdef DEBUG
    void dsvdc_log(const char *format, const char *function, ...);
    #define log(format, ...)    dsvdc_log(format, __func__, ## __VA_ARGS__) 
#else
    #define log(format, ...)
#endif

#endif/*__DSVDC_LOG_H__*/
