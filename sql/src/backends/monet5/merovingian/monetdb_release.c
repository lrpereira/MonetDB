/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://monetdb.cwi.nl/Legal/MonetDBLicense-1.1.html
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the MonetDB Database System.
 *
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-July 2008 CWI.
 * Copyright August 2008-2009 MonetDB B.V.
 * All Rights Reserved.
 */

static void
command_release(int argc, char *argv[])
{
	int i;
	int state = 0;    /* return status */
	int hadwork = 0;  /* did we do anything useful? */

	if (argc == 1) {
		/* print help message for this command */
		command_help(argc + 1, &argv[-1]);
		exit(1);
	}

	/* do for each listed database */
	for (i = 1; i < argc; i++) {
		hadwork = 1;
		if (mero_running == 0) {
			char *e;

			if ((e = db_release(argv[i])) != NULL) {
				fprintf(stderr, "%s: %s\n", argv[0], e);
				free(e);
				state |= 1;
				continue;
			}
		} else {
			char *res;
			char *out;

			out = control_send(&res, mero_control, -1, argv[i], argv[0]);
			if (out != NULL || strcmp(res, "OK") != 0) {
				res = out == NULL ? res : out;
				fprintf(stderr, "%s: %s\n", argv[0], res);
				state |= 1;
				free(res);
				continue;
			}
			free(res);
		}
		printf("database '%s' has been taken out of maintenance mode\n",
				argv[i]);
	}

	if (hadwork == 0) {
		command_help(2, &argv[-1]);
		state |= 1;
	}
	exit(state);
}

