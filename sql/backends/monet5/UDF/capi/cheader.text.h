// This file was generated automatically through cmake.
// Do not edit this file directly.
const char* cheader_header_text =
"/*\n"
" * This Source Code Form is subject to the terms of the Mozilla Public\n"
" * License, v. 2.0.  If a copy of the MPL was not distributed with this\n"
" * file, You can obtain one at http://mozilla.org/MPL/2.0/.\n"
" *\n"
" * Copyright 1997 - July 2008 CWI, August 2008 - 2021 MonetDB B.V.\n"
" */\n"
"#include <stdint.h>\n"
"typedef void *(*malloc_function_ptr)(size_t);\n"
"typedef void (*free_function_ptr)(void*);\n"
"typedef struct {\n"
"	unsigned char day;\n"
"	unsigned char month;\n"
"	int year;\n"
"} cudf_data_date;\n"
"typedef struct {\n"
"	unsigned int ms;\n"
"	unsigned char seconds;\n"
"	unsigned char minutes;\n"
"	unsigned char hours;\n"
"} cudf_data_time;\n"
"typedef struct {\n"
"	cudf_data_date date;\n"
"	cudf_data_time time;\n"
"} cudf_data_timestamp;\n"
"typedef struct {\n"
"	size_t size;\n"
"	void* data;\n"
"} cudf_data_blob;\n"
"#define DEFAULT_STRUCT_DEFINITION(type, typename)                              \\\n"
"	struct cudf_data_struct_##typename                                         \\\n"
"	{                                                                          \\\n"
"		type *data;                                                            \\\n"
"		size_t count;                                                          \\\n"
"		type null_value;                                                       \\\n"
"		double scale;                                                          \\\n"
"		int (*is_null)(type value);                                            \\\n"
"		void (*initialize)(void *self, size_t count);                          \\\n"
"		void *bat;                                                             \\\n"
"	}\n"
"DEFAULT_STRUCT_DEFINITION(int8_t, bit);\n"
"DEFAULT_STRUCT_DEFINITION(int8_t, bte);\n"
"DEFAULT_STRUCT_DEFINITION(int16_t, sht);\n"
"DEFAULT_STRUCT_DEFINITION(int, int);\n"
"DEFAULT_STRUCT_DEFINITION(int64_t, lng);\n"
"DEFAULT_STRUCT_DEFINITION(float, flt);\n"
"DEFAULT_STRUCT_DEFINITION(double, dbl);\n"
"DEFAULT_STRUCT_DEFINITION(char*, str);\n"
"DEFAULT_STRUCT_DEFINITION(cudf_data_date, date);\n"
"DEFAULT_STRUCT_DEFINITION(cudf_data_time, time);\n"
"DEFAULT_STRUCT_DEFINITION(cudf_data_timestamp, timestamp);\n"
"DEFAULT_STRUCT_DEFINITION(cudf_data_blob, blob);\n"
"DEFAULT_STRUCT_DEFINITION(size_t, oid);\n"
;
