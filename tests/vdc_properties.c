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

#include "dsvdc.h"
#include "properties.h"
#include "messages.pb-c.h"

extern bool dsvdc_property_is_sane(dsvdc_property_t *property);

START_TEST(create_valid_property)
{
    dsvdc_property_t *property = NULL;
    size_t i;

    int ret = dsvdc_property_new("testprop", 0, &property);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_new() returned %d", ret);
    ck_assert_msg(property != NULL, "dsvdc_property_new() invalid property");

    // index 1
    ret = dsvdc_property_add_int(property, 1, "intprop", INT_MAX);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_int() returned %d", ret);

    ret = dsvdc_property_add_uint(property, 1, "uintprop", UINT_MAX / 2);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_uint() returned %d",
                  ret);

    ret = dsvdc_property_add_bool(property, 1, "boolprop", true);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_bool() returned %d",
                  ret);

    ret = dsvdc_property_add_double(property, 1, "doubleprop", 1);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_double() returned %d",
                  ret);

    ret = dsvdc_property_add_string(property, 1, "stringprop", "string[1]");
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_string() returned %d",
                  ret);

    // index 0
    ret = dsvdc_property_add_int(property, 0, "intprop", INT_MIN);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_int() returned %d", ret);

    ret = dsvdc_property_add_uint(property, 0, "uintprop", UINT_MAX);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_uint() returned %d",
                  ret);

    ret = dsvdc_property_add_bool(property, 0, "boolprop", false);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_bool() returned %d",
                  ret);

    ret = dsvdc_property_add_double(property, 0, "doubleprop", 0);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_double() returned %d",
                  ret);

    ret = dsvdc_property_add_string(property, 0, "stringprop", "string[0]");
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_string() returned %d",
                  ret);

    ck_assert_msg(dsvdc_property_is_sane(property) == true,
                  "created property is invalid");

    // manually parse out the values and make sure they are correct
    ck_assert_msg(property->n_properties == 2, "invalid number of properties");
    ck_assert_msg(strcmp(property->name, "testprop") == 0,
                  "invalid property name");

    Vdcapi__Property *i0 = property->properties[0];
    ck_assert_msg(i0 != NULL, "invalid property list at index 0");
    ck_assert_msg(i0->n_elements == 5, "invalid number of elements at index 0");

    bool str_found = false;
    bool double_found = false;
    bool bool_found = false;
    bool uint_found = false;
    bool int_found = false;

    for (i = 0; i < i0->n_elements; i++)
    {
        Vdcapi__PropertyElement *el = i0->elements[i];
        ck_assert_msg(el != NULL, "invalid element at index 0 pos %zu\n", i);
        ck_assert_msg(el->value != NULL,
                      "missing element value at index 0 pos %zu\n", i);

        if (strcmp(el->name, "intprop") == 0)
        {
            int_found = true;
            ck_assert_msg(el->value->has_v_int64, "invalid value type at "
                          "index 0 %zu\n", i);
            ck_assert_msg(el->value->v_int64 == INT_MIN,
                          "invalid element value at index 0 pos %zu\n", i);
        }
        else if (strcmp(el->name, "uintprop") == 0)
        {
            uint_found = true;
            ck_assert_msg(el->value->has_v_uint64, "invalid value type at "
                          "index 0 %zu\n", i);
            ck_assert_msg(el->value->v_uint64 == UINT_MAX,
                          "invalid element value at index 0 pos %zu\n", i);
        } 
        else if (strcmp(el->name, "boolprop") == 0)
        {
            bool_found = true;
            ck_assert_msg(el->value->has_v_bool, "invalid value type at "
                          "index 0 %zu\n", i);
            ck_assert_msg(el->value->v_bool == false,
                          "invalid element value at index 0 pos %zu\n", i);
        }
        else if (strcmp(el->name, "doubleprop") == 0)
        {
            double_found = true;
            ck_assert_msg(el->value->has_v_double, "invalid value type at "
                          "index 0 %zu\n", i);
            ck_assert_msg(el->value->v_double == 0,
                          "invalid element value at index 0 pos %zu\n", i);
        }
        else if (strcmp(el->name, "stringprop") == 0)
        {
            str_found = true;
            ck_assert_msg(el->value->v_string != NULL, "invalid value type at "
                          "index 0 %zu\n", i);
            ck_assert_msg(strcmp(el->value->v_string, "string[0]") == 0,
                          "invalid element value at index 0 pos %zu\n", i);
        }
    }

    if (!str_found || !double_found || !bool_found || !uint_found || !int_found)
    {
        ck_abort_msg("one or more property elements not found");
    }
    
    Vdcapi__Property *i1 = property->properties[1];
    ck_assert_msg(i1 != NULL, "invalid property list at index 1");
    ck_assert_msg(i1->n_elements == 5, "invalid number of elements at index 1");

    str_found = false;
    double_found = false;
    bool_found = false;
    uint_found = false;
    int_found = false;

    for (i = 0; i < i1->n_elements; i++)
    {
        Vdcapi__PropertyElement *el = i1->elements[i];
        ck_assert_msg(el != NULL, "invalid element at index 1 pos %zu\n", i);
        ck_assert_msg(el->value != NULL,
                      "missing element value at index 1 pos %zu\n", i);

        if (strcmp(el->name, "intprop") == 0)
        {
            int_found = true;
            ck_assert_msg(el->value->has_v_int64, "invalid value type at "
                          "index 1 %zu\n", i);
            ck_assert_msg(el->value->v_int64 == INT_MAX,
                          "invalid element value at index 1 pos %zu\n", i);
        }
        else if (strcmp(el->name, "uintprop") == 0)
        {
            uint_found = true;
            ck_assert_msg(el->value->has_v_uint64, "invalid value type at "
                          "index 1 %zu\n", i);
            ck_assert_msg(el->value->v_uint64 == (UINT_MAX / 2),
                          "invalid element value at index 1 pos %zu\n", i);
        } 
        else if (strcmp(el->name, "boolprop") == 0)
        {
            bool_found = true;
            ck_assert_msg(el->value->has_v_bool, "invalid value type at "
                          "index 1 %zu\n", i);
            ck_assert_msg(el->value->v_bool == true,
                          "invalid element value at index 1 pos %zu\n", i);
        }
        else if (strcmp(el->name, "doubleprop") == 0)
        {
            double_found = true;
            ck_assert_msg(el->value->has_v_double, "invalid value type at "
                          "index 1 %zu\n", i);
            ck_assert_msg(el->value->v_double == 1,
                          "invalid element value at index 1 pos %zu\n", i);
        }
        else if (strcmp(el->name, "stringprop") == 0)
        {
            str_found = true;
            ck_assert_msg(el->value->v_string != NULL, "invalid value type at "
                          "index 1 %zu\n", i);
            ck_assert_msg(strcmp(el->value->v_string, "string[1]") == 0,
                          "invalid element value at index 1 pos %zu\n", i);
        }
    }

    if (!str_found || !double_found || !bool_found || !uint_found || !int_found)
    {
        ck_abort_msg("one or more property elements not found");
    }

    dsvdc_property_free(property);
}
END_TEST

START_TEST(index_hole_property)
{
    dsvdc_property_t *property = NULL;
    
    int ret = dsvdc_property_new("testprop", 0, &property);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_new() returned %d", ret);
    ck_assert_msg(property != NULL, "dsvdc_property_new() invalid property");

    // index 1
    ret = dsvdc_property_add_int(property, 1, "intprop", INT_MAX);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_int() returned %d", ret);

    // index 2
    ret = dsvdc_property_add_int(property, 2, "intprop", INT_MIN);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_int() returned %d", ret);


    // index 0 has a "hole"
    ck_assert_msg(dsvdc_property_is_sane(property) == false,
                  "property with an \"index hole\" can not be valid");

    dsvdc_property_free(property);
}
END_TEST

START_TEST(empty_elements_property)
{
    dsvdc_property_t *property = NULL;
    
    int ret = dsvdc_property_new("testprop", 2, &property);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_new() returned %d", ret);
    ck_assert_msg(property != NULL, "dsvdc_property_new() invalid property");

    // indices 0 and 1 exist but the element lists are empty
    ck_assert_msg(dsvdc_property_is_sane(property) == false,
                  "property with zero elements can not be valid");

    dsvdc_property_free(property);
}
END_TEST

START_TEST(element_length_mismatch)
{
    dsvdc_property_t *property = NULL;
    
    int ret = dsvdc_property_new("testprop", 2, &property);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_new() returned %d", ret);
    ck_assert_msg(property != NULL, "dsvdc_property_new() invalid property");

    // index 0, adding two elements
    ret = dsvdc_property_add_int(property, 0, "intprop1", 0);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_int() returned %d", ret);

    ret = dsvdc_property_add_int(property, 0, "intprop2", 0);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_int() returned %d", ret);

    // index 1, adding 1 element
    ret = dsvdc_property_add_int(property, 1, "intprop1", 0);
    ck_assert_msg(ret == DSVDC_OK, "dsvdc_property_add_int() returned %d", ret);

    // index 0 has a different number of elements compared to index 1
    ck_assert_msg(dsvdc_property_is_sane(property) == false,
                  "property elements lenght mismatch");

    dsvdc_property_free(property);
}
END_TEST

Suite *dsvdc_suite()
{
    Suite *s = suite_create("dSvDC Properties");
    TCase *tc_valid_property = tcase_create("valid property");
    tcase_add_test(tc_valid_property, create_valid_property);
    suite_add_tcase(s, tc_valid_property);

    TCase *tc_index_hole_property = tcase_create("index hole property");
    tcase_add_test(tc_index_hole_property, index_hole_property);
    suite_add_tcase(s, tc_index_hole_property);

    TCase *tc_empty_el_property = tcase_create("empty elements property");
    tcase_add_test(tc_empty_el_property, empty_elements_property);
    suite_add_tcase(s, tc_empty_el_property);

    TCase *tc_el_length_mismatch = tcase_create("element length mismatch");
    tcase_add_test(tc_el_length_mismatch, element_length_mismatch);
    suite_add_tcase(s, tc_el_length_mismatch);

    return s;
}

int main(int argc, char **argv)
{
    Suite *dsvdc = dsvdc_suite();
    SRunner *test_runner = srunner_create(dsvdc);
    srunner_run_all(test_runner, CK_NORMAL);
    int failed = 0;
    failed = srunner_ntests_failed(test_runner);
    srunner_free(test_runner);
    return failed;
}

