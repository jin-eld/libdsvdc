/*
    Copyright (c) 2016 aizo ag, Zurich, Switzerland

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
#include <stdlib.h>

#include "dsvdc.h"
#include "database.h"

#define DATABASE    "gdbmtest"
#define INTCOMPARE  -5000
#define UINTCOMPARE 10000
#define STRCOMPARE  "stringvalue"

START_TEST(open_invalid_database)
{
    dsvdc_database_t *db = NULL;

    int ret = dsvdc_database_open("nonexistence", false, &db);
    ck_assert_msg(ret != DSVDC_OK, "dsvdc_load_database returned %d", ret);
}
END_TEST

START_TEST(save_database)
{
    dsvdc_database_t *db = NULL;
    dsvdc_property_t *property = NULL;

    int ret = dsvdc_database_open(DATABASE, true, &db);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_load_database() returned %d", ret);
    ck_assert_msg(db != NULL, "dsvdc_property_new() invalid property");


    ret = dsvdc_property_new(&property);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_new() returned %d", ret);
    ck_assert_msg(property != NULL, "dsvdc_property_new() invalid property");

    ret = dsvdc_property_add_int(property, "intprop", INTCOMPARE);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_int() returned %d", ret);

    ret = dsvdc_property_add_uint(property, "uintprop", UINTCOMPARE);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_uint() returned %d",
                  ret);

    ret = dsvdc_property_add_string(property, "stringprop", STRCOMPARE);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_string() returned %d",
                  ret);

    ret = dsvdc_database_save_property(db, "testkey", property);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_database_save() returned %d", ret);

    dsvdc_property_free(property);
    dsvdc_database_close(db);
}
END_TEST

START_TEST(read_database)
{
    dsvdc_database_t *db = NULL;
    dsvdc_property_t *property = NULL;

    /* open in read only mode */
    int ret = dsvdc_database_open(DATABASE, false, &db);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_load_database() returned %d", ret);

    ret = dsvdc_database_load_property(db, "invalid", &property);
    ck_assert_msg(ret != DSVDC_OK, "dsvdc_load_database() returned %d", ret);

    ret = dsvdc_database_load_property(db,"testkey", &property);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_load_property() returned %d", ret);

    int64_t integer;
    uint64_t unsigned_integer;
    char *string;

    ret = dsvdc_property_get_int(property, 0, &integer);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_get_int() returned %d",
                  ret);
    ck_assert_msg(integer == INTCOMPARE,
                  "integer property retrieval mismatch! got %d, expected %d",
                  integer, INTCOMPARE);

    ret = dsvdc_property_get_uint(property, 1, &unsigned_integer);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_get_uint() returned %d",
                  ret);
    ck_assert_msg(unsigned_integer == UINTCOMPARE, "unsigned integer property"
                  " retrieval mismatch! got %d, expected %d", unsigned_integer,
                  UINTCOMPARE);

    ret = dsvdc_property_get_string(property, 2, &string);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_get_string() returned %d",
                  ret);
    ck_assert_msg((strcmp(string, STRCOMPARE) == 0),
                  "string property retrieval mismatch! got %s, expected %s",
                  string, STRCOMPARE);

    free(string);
    dsvdc_property_free(property);
    dsvdc_database_close(db);
}
END_TEST

Suite *dsvdc_suite()
{
    Suite *s = suite_create("dSvDC Property Database Operations");
    TCase *tc_invalid_db = tcase_create("open invalid database");
    tcase_add_test(tc_invalid_db, open_invalid_database);
    suite_add_tcase(s, tc_invalid_db);

    TCase *tc_fill_db = tcase_create("create and fill database");
    tcase_add_test(tc_fill_db, save_database);
    suite_add_tcase(s, tc_fill_db);

    TCase *tc_read_db = tcase_create("load and read database");
    tcase_add_test(tc_read_db, read_database);
    suite_add_tcase(s, tc_read_db);

    return s;
}

int main()
{
    unlink(DATABASE);

    Suite *dsvdc = dsvdc_suite();
    SRunner *test_runner = srunner_create(dsvdc);
    srunner_run_all(test_runner, CK_NORMAL);
    int failed = 0;
    failed = srunner_ntests_failed(test_runner);
    srunner_free(test_runner);

    unlink(DATABASE);

    return failed;
}

