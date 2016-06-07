/*
    Copyright (c) 2016 digitalSTROM.org, Zurich, Switzerland

    Author: Sergey 'Jin' Bostandzhyan <jin@dev.digitalstrom.org>

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

#include <sys/stat.h>

#include "util.h"

bool file_exists(const char *name)
{
    struct stat statbuf;

    if (stat(name, &statbuf) != 0)
	{
		return false;
	}

	return true;
}
