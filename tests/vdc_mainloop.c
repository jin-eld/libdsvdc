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

#include <check.h>
#include <unistd.h>

#include "dsvdc.h"

START_TEST(test_init_cleanup)
{
    dsvdc_t *handle;
    int i;
    ck_assert_msg(dsvdc_new(0, "1", "test", NULL, &handle) == DSVDC_OK,
                  "dsvdc_new() initializatino failed");

    for (i = 0; i < 3; i++)
    {
        dsvdc_work(handle, 1);
    }
    dsvdc_cleanup(handle);
    ck_assert_msg(handle != NULL, "dsvdc_cleanup() failed");
}
END_TEST

Suite *dsvdc_suite()
{
    Suite *s = suite_create("dSvDC");
    TCase *tc_init_cleanup = tcase_create("dSvDC");
    tcase_set_timeout(tc_init_cleanup, 4); 
    tcase_add_test(tc_init_cleanup, test_init_cleanup);
    suite_add_tcase(s, tc_init_cleanup);
    return s;
}

int main()
{
    Suite *dsvdc = dsvdc_suite();
    SRunner *test_runner = srunner_create(dsvdc);
    srunner_run_all(test_runner, CK_NORMAL);
    int failed = 0;
    failed = srunner_ntests_failed(test_runner);
    srunner_free(test_runner);
    return failed;
}

