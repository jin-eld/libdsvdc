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
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>

#include "dsvdc.h"
#include "properties.h"
#include "messages.pb-c.h"

extern bool dsvdc_property_is_sane(dsvdc_property_t *property);

START_TEST(create_valid_property)
{
    dsvdc_property_t *property = NULL;

    int ret = dsvdc_property_new(&property);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_new() returned %d", ret);
    ck_assert_msg(property != NULL, "dsvdc_property_new() invalid property");

    // test flat model, i.e. bunch of non recursive properties attached to the
    // original handle
    ret = dsvdc_property_add_int(property, "intprop", INT_MAX);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_int() returned %d", ret);

    ret = dsvdc_property_add_uint(property, "uintprop", UINT_MAX / 2);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_uint() returned %d",
                  ret);

    ret = dsvdc_property_add_bool(property, "boolprop", true);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_bool() returned %d",
                  ret);

    ret = dsvdc_property_add_double(property, "doubleprop", 1);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_double() returned %d",
                  ret);

    ret = dsvdc_property_add_string(property, "stringprop", "string[1]");
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_string() returned %d",
                  ret);

    // prepare a sublevel array
    dsvdc_property_t *sub = NULL;

    ret = dsvdc_property_new(&sub);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_new() returned %d", ret);
    ck_assert_msg(sub != NULL, "dsvdc_property_new() invalid property");

    // fill sublevel array
    ret = dsvdc_property_add_int(sub, "intprop", INT_MIN);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_int() returned %d", ret);

    ret = dsvdc_property_add_uint(sub, "uintprop", UINT_MAX);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_uint() returned %d",
                  ret);

    ret = dsvdc_property_add_bool(sub, "boolprop", false);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_bool() returned %d",
                  ret);

    ret = dsvdc_property_add_double(sub, "doubleprop", 0);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_double() returned %d",
                  ret);

    ret = dsvdc_property_add_string(sub, "stringprop", "string[0]");
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_string() returned %d",
                  ret);


    // and attach the sub props to the main property
    dsvdc_property_add_property(property, "fokel", &sub);

    // manually parse out the values and make sure they are correct
    ck_assert_msg(dsvdc_property_get_num_properties(property) == 6,
                  "invalid number of properties");

    char *name;
    ret = dsvdc_property_get_property_by_name(property, "fokel", &sub);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_get_property_by_name() "
                  "returned %d", ret);

    ret = dsvdc_property_get_name(property, 5, &name);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_get_name() returned %d",
                  ret);

    ret = strcmp(name, "fokel");
    ck_assert_msg(ret == 0, "dsvdc_propety_get_name() - name mismatch");
    free(name);

    dsvdc_property_value_t type;
    ret = dsvdc_property_get_value_type(sub, 1, &type);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_get_value_type() "
                  "returned %d", ret);

    ck_assert_msg(type == DSVDC_PROPERTY_VALUE_UINT64, "unexpected value type, "
                  "expecting uint64_t property at index 1, got %d\n", type);

    uint64_t val;
    ret = dsvdc_property_get_uint(sub, 1, &val);
    ck_assert_msg(ret == DSVDC_OK, "dvdc_property_get_uint() returned %d", ret);
    ck_assert_msg(val == UINT_MAX, "dvdc_property_get_uint() returned "
                  "unexpected value %u\n", val);

    dsvdc_property_free(sub);
    dsvdc_property_free(property);
}
END_TEST

Suite *dsvdc_suite()
{
    Suite *s = suite_create("dSvDC Properties");
    TCase *tc_valid_property = tcase_create("valid property");
    tcase_add_test(tc_valid_property, create_valid_property);
    suite_add_tcase(s, tc_valid_property);

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

