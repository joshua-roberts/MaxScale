/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2024-01-15
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

/**
 * @file avro_schema.c - Avro schema related functions
 */

#include "avrorouter.hh"

#include <maxscale/mysql_utils.hh>
#include <jansson.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <maxbase/alloc.h>

/**
 * @brief Check whether the field is one that was generated by the avrorouter
 *
 * @param name Name of the field in the Avro schema
 * @return True if field was not generated by the avrorouter
 */
static inline bool not_generated_field(const char* name)
{
    return strcmp(name, avro_domain) && strcmp(name, avro_server_id)
           && strcmp(name, avro_sequence) && strcmp(name, avro_event_number)
           && strcmp(name, avro_event_type) && strcmp(name, avro_timestamp);
}

/**
 * @brief Extract the field names from a JSON Avro schema file
 *
 * This function extracts the names of the columns from the JSON format Avro
 * schema in the file @c filename. This function assumes that the field definitions
 * in @c filename are in the same order as they are in the CREATE TABLE statement.
 *
 * @param filename The Avro schema in JSON format
 * @param table The TABLE_CREATE object to populate
 * @return True on success successfully, false on error
 */
bool json_extract_field_names(const char* filename, std::vector<Column>& columns)
{
    bool rval = false;
    json_error_t err;
    err.text[0] = '\0';
    json_t* obj;
    json_t* arr = nullptr;

    if ((obj = json_load_file(filename, 0, &err)) && (arr = json_object_get(obj, "fields")))
    {
        if (json_is_array(arr))
        {
            int array_size = json_array_size(arr);
            rval = true;

            for (int i = 0; i < array_size; i++)
            {
                json_t* val = json_array_get(arr, i);

                if (json_is_object(val))
                {
                    json_t* name = json_object_get(val, "name");

                    if (name && json_is_string(name))
                    {
                        const char* name_str = json_string_value(name);

                        if (not_generated_field(name_str))
                        {
                            columns.emplace_back(name_str);

                            json_t* value;

                            if ((value = json_object_get(val, "real_type")) && json_is_string(value))
                            {
                                columns.back().type = json_string_value(value);
                            }
                            else
                            {
                                MXS_WARNING("No \"real_type\" value defined. Treating as unknown type field.");
                            }

                            if ((value = json_object_get(val, "length")) && json_is_integer(value))
                            {
                                columns.back().length = json_integer_value(value);
                            }
                            else
                            {
                                MXS_WARNING("No \"length\" value defined. Treating as default length field.");
                            }

                            if ((value = json_object_get(val, "unsigned")) && json_is_boolean(value))
                            {
                                columns.back().is_unsigned = json_boolean_value(value);
                            }
                        }
                    }
                    else
                    {
                        MXS_ERROR("JSON value for \"name\" was not a string in "
                                  "file '%s'.",
                                  filename);
                        rval = false;
                    }
                }
                else
                {
                    MXS_ERROR("JSON value for \"fields\" was not an array of objects in "
                              "file '%s'.",
                              filename);
                    rval = false;
                }
            }
        }
        else
        {
            MXS_ERROR("JSON value for \"fields\" was not an array in file '%s'.", filename);
        }
        json_decref(obj);
    }
    else
    {
        MXS_ERROR("Failed to load JSON from file '%s': %s",
                  filename,
                  obj && !arr ? "No 'fields' value in object." : err.text);
    }

    return rval;
}

TableCreateEvent* table_create_from_schema(const char* file,
                                           const char* db,
                                           const char* table,
                                           int version)
{
    TableCreateEvent* newtable = NULL;
    std::vector<Column> columns;

    if (json_extract_field_names(file, columns))
    {
        newtable = new(std::nothrow) TableCreateEvent(db, table, version, std::move(columns));
    }

    return newtable;
}
