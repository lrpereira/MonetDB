/*
 * This code was created by Peter Harvey (mostly during Christmas 98/99).
 * This code is LGPL. Please ensure that this message remains in future
 * distributions and uses of this code (thats about all I get out of it).
 * - Peter Harvey pharvey@codebydesign.com
 * 
 * This file has been modified for the MonetDB project.  See the file
 * Copyright in this directory for more information.
 */

/**********************************************************************
 * SQLProcedures()
 * CLI Compliance: ODBC (Microsoft)
 *
 * Note: this function is not implemented (it only sets an error),
 * because monetDB SQL frontend does not support stored procedures.
 *
 * Author: Martin van Dinther
 * Date  : 30 aug 2002
 *
 **********************************************************************/

#include "ODBCGlobal.h"
#include "ODBCStmt.h"
#include "ODBCUtil.h"


SQLRETURN SQL_API
SQLProcedures(SQLHSTMT hStmt,
	      SQLCHAR *szCatalogName, SQLSMALLINT nCatalogNameLength,
	      SQLCHAR *szSchemaName, SQLSMALLINT nSchemaNameLength,
	      SQLCHAR *szProcName, SQLSMALLINT nProcNameLength)
{
	ODBCStmt *stmt = (ODBCStmt *) hStmt;

#ifdef ODBCDEBUG
	ODBCLOG("SQLProcedures\n");
#endif

	if (!isValidStmt(stmt))
		 return SQL_INVALID_HANDLE;

	clearStmtErrors(stmt);

	/* check statement cursor state, no query should be prepared or executed */
	if (stmt->State != INITED) {
		/* 24000 = Invalid cursor state */
		addStmtError(stmt, "24000", NULL, 0);
		return SQL_ERROR;
	}

	fixODBCstring(szCatalogName, nCatalogNameLength, addStmtError, stmt);
	fixODBCstring(szSchemaName, nSchemaNameLength, addStmtError, stmt);
	fixODBCstring(szProcName, nProcNameLength, addStmtError, stmt);

	/* SQLProcedures returns a table with the following columns:
	   VARCHAR	procedure_cat
	   VARCHAR	procedure_schem
	   VARCHAR	procedure_name NOT NULL
	   n/a		num_input_params (reserved for future use)
	   n/a		num_output_params (reserved for future use)
	   n/a		num_result_sets (reserved for future use)
	   VARCHAR	remarks
	   SMALLINT	procedure_type
	*/

	/* for now return dummy result set */
	return SQLExecDirect_(stmt,
			      (SQLCHAR *) "select "
			      "cast('' as varchar) as procedure_cat, "
			      "cast('' as varchar) as procedure_schem, "
			      "cast('' as varchar) as procedure_name, "
			      "0 as num_input_params, "
			      "0 as num_output_params, "
			      "0 as num_result_sets, "
			      "cast('' as varchar) as remarks, "
			      "cast(0 as smallint) as procedure_type "
			      "where 0 = 1", SQL_NTS);
}
