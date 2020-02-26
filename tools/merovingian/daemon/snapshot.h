/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2020 MonetDB B.V.
 */

#ifndef _SNAPSHOT_H
#define _SNAPSHOT_H 1

#include "merovingian.h"

err snapshot_database_to(char *dname, char *dest);
err snapshot_default_filename(char **filename_buf, const char *dbname);

#endif

/* vim:set ts=4 sw=4 noexpandtab: */
