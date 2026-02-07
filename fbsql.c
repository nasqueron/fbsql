
/* FastBase MySQL + TCL interface program; created 22nd Sept 2000 */
/* This program was written after extensive use of the various mysql-tcl */
/* interface programs around, hopefully this program addresses some of */
/* the issues I had with those programs - especially in relation to */
/* writing the FastBase accounting system which utilises large queries */
/* and requires the highest performance levels available */

/* fbsql provides simple access to the mysql database, the commands are */
/* very similar to the other tcl-mysql interfaces */
/* added support for handling large queries without using client memory */
/* added support for transferring results to arrays for convenience/speed */

/* Modified to handle "high" characters (> 0x7f)  */

/* Copyright 2003, Peter Campbell Software Ltd. http://www.fastbase.co.nz, mailto:pc@acs.co.nz */

/* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met: */

/* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. */
/* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer */
/* in the documentation and/or other materials provided with the distribution. */
/* Neither the name of Peter Campbell Software nor the names of its contributors may be used to endorse or promote products */
/* derived from this software without specific prior written permission. */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED */
/* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A */
/* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR */
/* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, */
/* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) */
/* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT */
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED */
/* OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <stdlib.h>
#include <string.h>

#ifdef WINDOWS
#include <WINDOWS.H>
#include <winsock.h>
#endif

#include <mysql.h>
#include <tcl.h>

char errormsg[1000];
int done;

#define SQL_COMMANDS 11
#define UTF_ENCODING 0

struct mysql_connect
{
	int CONNECTED;
	int query_flag;
	int NUMROWS;
	int use_array;
	int field_count;
	MYSQL mysql;
	MYSQL_RES *result;
	char array_name[100];
} connection[SQL_COMMANDS];

/*******************************************************************/
/* miscellaneous support routines (error handling etc) */
/*******************************************************************/

void output_error(Tcl_Interp *interp, int sql_number) {
	sprintf(errormsg,"Error %u (%s)",mysql_errno(&connection[sql_number].mysql),mysql_error(&connection[sql_number].mysql));
	Tcl_SetResult(interp,errormsg,TCL_STATIC);
}

/*******************************************************************/
/* sql connect */
/*******************************************************************/

int fbsql_connect(Tcl_Interp *interp, int sql_number, int argc, const char **argv) {
	const char *host = NULL;
	const char *user = NULL;
	const char *passwd = NULL;
	const char *db = NULL;
	unsigned int port = 0;

	/* check not already connected? */
	if (connection[sql_number].CONNECTED) {
		Tcl_SetResult(interp, "sql connect; already connected.", TCL_STATIC);
		return TCL_ERROR;
	}

	/* updated command line options: fbsql_connect host user password [database] [port] */
	if (argc > 0 && argv[0]) { host = argv[0]; }
	if (argc > 1 && argv[1]) { user = argv[1]; }
	if (argc > 2 && argv[2]) { passwd = argv[2]; }
	if (argc > 3 && argv[3]) { db = argv[3]; }
	if (argc > 4 && argv[4]) { port = atol(argv[4]); }

	mysql_init(&connection[sql_number].mysql);

	if (!(mysql_real_connect(&connection[sql_number].mysql,host,user,passwd,db,port,NULL,0))) {
		output_error(interp,sql_number);
		return TCL_ERROR;
	} else {
		connection[sql_number].CONNECTED = 1;
		return TCL_OK;
	}
}

int fbsql_disconnect(Tcl_Interp *interp, int sql_number) {
	/* check that we are connected? */
	if (!connection[sql_number].CONNECTED) {
		Tcl_SetResult(interp, "sql disconnect; not connected.", TCL_STATIC);
		return TCL_ERROR;
	}

	mysql_close(&connection[sql_number].mysql);
	connection[sql_number].CONNECTED = 0;
	return TCL_OK;
}

/*******************************************************************/
/* sql general commands */
/*******************************************************************/

int fbsql_selectdb(Tcl_Interp *interp, int sql_number, int argc, const char **argv) {
	const char *database = NULL;

	/* check a database name argument has been specified */
	if (argc <= 0 || argv[0] == NULL) {
		Tcl_SetResult(interp, "sql selectdb database_name; no database name was specified.", TCL_STATIC);
		return TCL_ERROR;
	}

	/* check that we are connected to a mysql server */
	if (!connection[sql_number].CONNECTED) {
		Tcl_SetResult(interp, "sql query statement; you are not connected to a mysql server yet (sql connect).", TCL_STATIC);
		return TCL_ERROR;
	}

	database = argv[0];

	if (mysql_select_db(&connection[sql_number].mysql,database)) {
		output_error(interp,sql_number);
		return TCL_ERROR;
	} else {
		return TCL_OK;
	}
}

int fbsql_numrows(Tcl_Interp *interp, int sql_number, int argc, const char **argv) {
	Tcl_Obj *obj_result;

	/* set result object pointer */
	obj_result = Tcl_GetObjResult(interp);
	Tcl_SetIntObj(obj_result,connection[sql_number].NUMROWS);
	
	return TCL_OK;
}

int determine_field_type(int type) {
	/* 0 = char, 1 = numeric, 2 = datetime */
	switch (type) {

	case FIELD_TYPE_BLOB:
	case FIELD_TYPE_NULL:
	case FIELD_TYPE_SET:
	case FIELD_TYPE_STRING:
	case FIELD_TYPE_TIME:
	case FIELD_TYPE_TIMESTAMP:
	case FIELD_TYPE_VAR_STRING:
		return 0;
		break;

	case FIELD_TYPE_DECIMAL:
	case FIELD_TYPE_DOUBLE:
	case FIELD_TYPE_ENUM:
	case FIELD_TYPE_FLOAT:
	case FIELD_TYPE_INT24:
	case FIELD_TYPE_LONG:
	case FIELD_TYPE_LONGLONG:
	case FIELD_TYPE_SHORT:
	case FIELD_TYPE_TINY:
	case FIELD_TYPE_YEAR:
		return 1;
		break;

	case FIELD_TYPE_DATE:
	case FIELD_TYPE_DATETIME:
		return 2;
		break;
	}

	/* return 0 by default, to keep compiler warnings away */
	return 0;
}

/*******************************************************************/
/* sql query command(s) */
/*******************************************************************/

int fbsql_query(Tcl_Interp *interp, int sql_number, int argc, const char **argv) {
	const char *query;
	int i, length, field_count;
	MYSQL_RES *result;
	MYSQL_ROW row;
	Tcl_Obj *obj_result;
	Tcl_Obj *obj_row;
	Tcl_Obj *obj_col;
#if UTF_ENCODING
	Tcl_DString ds;
#endif

	if (!connection[sql_number].CONNECTED) {
		Tcl_SetResult(interp, "sql query statement; you are not connected to a mysql server yet (sql connect).", TCL_STATIC);
		return TCL_ERROR;
	}

	/* check there is no other active query? */
	if (connection[sql_number].query_flag) {
		Tcl_SetResult(interp, "Another query cannot be made until the current query has been finished with \"sql endquery\".", TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc <= 0 || argv[0] == NULL) {
		Tcl_SetResult(interp, "sql query statement; no sql query was specified.", TCL_STATIC);
		return TCL_ERROR;
	}

#if UTF_ENCODING
	Tcl_DStringInit(&ds);
	Tcl_UtfToExternalDString(NULL, argv[0], strlen(argv[0]), &ds);
	query = Tcl_DStringValue(&ds);

	/* execute the sql query statement */
	if (mysql_query(&connection[sql_number].mysql,query)) {
		output_error(interp,sql_number);
		Tcl_DStringFree(&ds);
		return TCL_ERROR;
	}
	
	Tcl_DStringFree(&ds);
#else
	query = argv[0];

	/* execute the sql query statement */
	if (mysql_query(&connection[sql_number].mysql,query)) {
		output_error(interp,sql_number);
		return TCL_ERROR;
	}
#endif

	result = mysql_use_result(&connection[sql_number].mysql);
	field_count = mysql_field_count(&connection[sql_number].mysql);

	/* if no results were found and none expected then all ok */
	/* otherwise we assume the query worked ok, now get the results */
	if (result == NULL) {
		if (field_count) {
			output_error(interp,sql_number);
			return TCL_ERROR;
		}
		connection[sql_number].NUMROWS = (int)mysql_affected_rows(&connection[sql_number].mysql);
		return TCL_OK;
	} else {
		/* set result object pointer */
		obj_result = Tcl_GetObjResult(interp);

		/* process all rows from query */
		while ((row = mysql_fetch_row(result)) != NULL) {
			obj_row = Tcl_NewListObj(0,NULL);
			for (i = 0; i < field_count; i++) {
				if (row[i] == NULL) {
					length = 0;
				} else {
					length = strlen(row[i]);
				}
#if UTF_ENCODING
				Tcl_DStringInit(&ds);
				Tcl_ExternalToUtfDString(NULL, row[i], length, &ds);
				obj_col = Tcl_NewStringObj(Tcl_DStringValue(&ds),
						Tcl_DStringLength(&ds));
				Tcl_ListObjAppendElement(interp, obj_row, obj_col);
				Tcl_DStringFree(&ds);
#else
				obj_col = Tcl_NewStringObj(row[i],length);
				Tcl_ListObjAppendElement(interp,obj_row,obj_col);
#endif
			}
			Tcl_ListObjAppendElement(interp,obj_result,obj_row);
		}
		connection[sql_number].NUMROWS = (int)mysql_num_rows(result);
		mysql_free_result(result);
		return TCL_OK;
	}
}

/*******************************************************************/
/* sql query command(s) - non standard enhancements for FastBase */
/*******************************************************************/

int fbsql_startquery(Tcl_Interp *interp, int sql_number, int argc, const char **argv) {
	const char *query = NULL;
	int i;
	int sql_huge = 0;
#if UTF_ENCODING
	Tcl_DString ds;
#endif

	if (!connection[sql_number].CONNECTED) {
		Tcl_SetResult(interp, "sql startquery statement; you are not connected to a mysql server yet (sql connect).", TCL_STATIC);
		return TCL_ERROR;
	}

	/* check there is no other active query? */
	if (connection[sql_number].query_flag) {
		Tcl_SetResult(interp, "Another query cannot be made until the current query has been finished with \"sql endquery\".", TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc <= 0 || argv[0] == NULL) {
		Tcl_SetResult(interp, "sql startquery statement; no sql query was specified.", TCL_STATIC);
		return TCL_ERROR;
	}

	connection[sql_number].use_array = 0;

	/* process additional "command-line" options */
	for (i = 1; i < argc; i++) {
		if (argv[i] != NULL) {
			if (strcmp(argv[i],"-huge") == 0) {
				sql_huge = 1;
			} else if (strcmp(argv[i],"-array") == 0) {
				if (argc <= i || argv[i + 1] == NULL) {
					Tcl_SetResult(interp, "sql startquery; no array name specified with -array option.", TCL_STATIC);
					return TCL_ERROR;
				}
				connection[sql_number].use_array = 1;
				strcpy(connection[sql_number].array_name,argv[i+1]);
				i++;
			} else {
				Tcl_SetResult(interp, "sql startquery; invalid option on command line.", TCL_STATIC);
				return TCL_ERROR;
			}
		}
	}

#if UTF_ENCODING
	Tcl_DStringInit(&ds);
	Tcl_UtfToExternalDString(NULL, argv[0], strlen(argv[0]), &ds);
	query = Tcl_DStringValue(&ds);
#else
	query = argv[0];
#endif

	/* execute the sql query statement */
	if (mysql_query(&connection[sql_number].mysql,query)) {
		output_error(interp,sql_number);
#if UTF_ENCODING
		Tcl_DStringFree(&ds);
#endif
		return TCL_ERROR;
	}

#if UTF_ENCODING
	Tcl_DStringFree(&ds);
#endif

	/* use or store the result? */
	/* the default is to store it locally */
	/* for large queries use "-huge" option */
	if (sql_huge) {
		connection[sql_number].result = mysql_use_result(&connection[sql_number].mysql);
	} else {
		connection[sql_number].result = mysql_store_result(&connection[sql_number].mysql);
		connection[sql_number].NUMROWS = (int)mysql_num_rows(connection[sql_number].result);
	}
	connection[sql_number].field_count = mysql_field_count(&connection[sql_number].mysql);

	/* if no results were found and none expected then all ok */
	/* otherwise we assume the query worked ok, now get the results */
	if (connection[sql_number].result == NULL) {
		if (connection[sql_number].field_count) {
			output_error(interp,sql_number);
			return TCL_ERROR;
		}
		Tcl_SetResult(interp, "sql startquery; query executed ok but returned no results.", TCL_STATIC);
		return TCL_ERROR;
	}

	/* return all ok now, see fetchrow & endquery for more details */
	connection[sql_number].query_flag = 1;
	return TCL_OK;
}

int fbsql_fetchrow(Tcl_Interp *interp, int sql_number, int argc, const char **argv) {
	int i, field_type, length;
	MYSQL_ROW row;
	MYSQL_FIELD *field;
	Tcl_Obj *obj_result;
	Tcl_Obj *obj_col;
	Tcl_Obj *obj_array;
	Tcl_Obj *obj_element;
#if UTF_ENCODING
	Tcl_DString ds;
#endif

	/* check we are connected? */
	if (!connection[sql_number].CONNECTED) {
		Tcl_SetResult(interp, "Not connected to a server.", TCL_STATIC);
		return TCL_ERROR;
	}
	/* check we had a query started? */
	if (!connection[sql_number].query_flag) {
		Tcl_SetResult(interp, "No query has been started.", TCL_STATIC);
		return TCL_ERROR;
	}

	/* fetch some data? */
	row = mysql_fetch_row(connection[sql_number].result);

	if (row == NULL) {
		/* if we come to the end of the data return nothing */
		/* if using an array set the array elements to default/NULL values */
		if (connection[sql_number].use_array) {
			obj_array = Tcl_NewStringObj(connection[sql_number].array_name,strlen(connection[sql_number].array_name));

			for (i = 0; i < connection[sql_number].field_count; i++) {
				/* get field information */
				field = mysql_fetch_field_direct(connection[sql_number].result,i);
				if (field == NULL) continue;

				obj_element = Tcl_NewStringObj(field->name,strlen(field->name));

				/* 0 = char, 1 = numeric, 2 = datetime */
				field_type = determine_field_type(field->type);
				/* if the type is numeric then store 0.00 as the result */
				if (field_type == 1) {
					obj_col = Tcl_NewDoubleObj(0);
				} else {
					obj_col = Tcl_NewStringObj(NULL,0);
				}

				Tcl_ObjSetVar2(interp,obj_array,obj_element,obj_col,0);
				/* we no longer have any use for the element object */
				Tcl_DecrRefCount(obj_element);
			}
		/* we no longer have any use for the array object */
		Tcl_DecrRefCount(obj_array);
		}
		return TCL_OK;
	}

	/* if an array name was specified on the sql command line then we */
	/* transfer all results to that array - special FastBase conversions */
	/* otherwise return the row as a list of columns */
	if (connection[sql_number].use_array) {
		obj_array = Tcl_NewStringObj(connection[sql_number].array_name,strlen(connection[sql_number].array_name));

		for (i = 0; i < connection[sql_number].field_count; i++) {
			/* get field information */
			field = mysql_fetch_field_direct(connection[sql_number].result,i);
			if (field == NULL) continue;

			obj_element = Tcl_NewStringObj(field->name,strlen(field->name));

			/* 0 = char, 1 = numeric, 2 = datetime */
			field_type = determine_field_type(field->type);

			if (row[i] == NULL) {
				/* if the field is NULL and the type is numeric then store 0.00 as the result */
				if (field_type == 1) {
					obj_col = Tcl_NewDoubleObj(0);
				} else {
					obj_col = Tcl_NewStringObj(NULL,0);
				}
			} else {
				/* if date field check for the value "0000-00-00" */
				/* replace this with NULL */
				if (field_type == 2 && strlen(row[i]) >= 10) {
					if (strncmp(row[i],"0000-00-00",10) == 0) {
						obj_col = Tcl_NewStringObj(NULL,0);
					} else {
						obj_col = Tcl_NewStringObj(row[i],strlen(row[i]));
					}
				} else {
#if UTF_ENCODING
					Tcl_DStringInit(&ds);
					Tcl_ExternalToUtfDString(NULL, row[i], strlen(row[i]), &ds);
					obj_col = Tcl_NewStringObj(Tcl_DStringValue(&ds),
							Tcl_DStringLength(&ds));
					Tcl_DStringFree(&ds);
#else
					obj_col = Tcl_NewStringObj(row[i],strlen(row[i]));
#endif
				}
			}

			Tcl_ObjSetVar2(interp,obj_array,obj_element,obj_col,0);
			/* we no longer have any use for the element object */
			Tcl_DecrRefCount(obj_element);
		}
		/* we no longer have any use for the array object */
		Tcl_DecrRefCount(obj_array);
		/* set result object pointer */
		obj_result = Tcl_GetObjResult(interp);
		/* return result = array name */
		Tcl_SetStringObj(obj_result,connection[sql_number].array_name,strlen(connection[sql_number].array_name));
		return TCL_OK;
	} else {
		/* set result object pointer */
		obj_result = Tcl_GetObjResult(interp);
		/* process all columns from query row */
		for (i = 0; i < connection[sql_number].field_count; i++) {
			if (row[i] == NULL) {
				length = 0;
			} else {
				length = strlen(row[i]);
			}
#if UTF_ENCODING
			Tcl_DStringInit(&ds);
			Tcl_ExternalToUtfDString(NULL, row[i], length, &ds);
			obj_col = Tcl_NewStringObj(Tcl_DStringValue(&ds),
					Tcl_DStringLength(&ds));
			Tcl_ListObjAppendElement(interp, obj_result, obj_col);
			Tcl_DStringFree(&ds);
#else
			obj_col = Tcl_NewStringObj(row[i],length);
			Tcl_ListObjAppendElement(interp,obj_result,obj_col);
#endif
		}
		return TCL_OK;
	}
}

int fbsql_endquery(Tcl_Interp *interp, int sql_number, int argc, const char **argv) {
	/* check we are connected? */
	if (!connection[sql_number].CONNECTED) {
		Tcl_SetResult(interp, "Not connected to a server.", TCL_STATIC);
		return TCL_ERROR;
	}
	/* check we had a query started? */
	if (!connection[sql_number].query_flag) {
		Tcl_SetResult(interp, "No query has been started.", TCL_STATIC);
		return TCL_ERROR;
	}

	connection[sql_number].NUMROWS = (int)mysql_num_rows(connection[sql_number].result);
	mysql_free_result(connection[sql_number].result);
	connection[sql_number].query_flag = 0;
	return TCL_OK;
}

/*******************************************************************/
/* tcl sql command */
/*******************************************************************/

int SqlCmd (ClientData sql_word, Tcl_Interp *interp, int argc, const char **argv) {
	int sql_number = (int)sql_word;

	/* any command specified */
	if (argc <= 1) {
		Tcl_SetResult(interp, "Usage: sql connect|selectdb|query|numrows|disconnect|version; please try again.", TCL_STATIC);
		return TCL_ERROR;
	}

	/* check command list */
	if (strcmp(argv[1],"connect") == 0) {
		return fbsql_connect(interp,sql_number,argc-2,argv+2);
	} else if (strcmp(argv[1],"selectdb") == 0) {
		return fbsql_selectdb(interp,sql_number,argc-2,argv+2);
	} else if (strcmp(argv[1],"query") == 0) {
		return fbsql_query(interp,sql_number,argc-2,argv+2);
	} else if (strcmp(argv[1],"numrows") == 0) {
		return fbsql_numrows(interp,sql_number,argc-2,argv+2);
	} else if (strcmp(argv[1],"disconnect") == 0) {
		return fbsql_disconnect(interp,sql_number);
	} else if (strcmp(argv[1],"version") == 0) {
		Tcl_SetResult(interp, "FastBase MySQL Interface for Tcl; version 1.06", TCL_STATIC);
		return TCL_OK;
	} else if (strcmp(argv[1],"startquery") == 0) {
		return fbsql_startquery(interp,sql_number,argc-2,argv+2);
	} else if (strcmp(argv[1],"fetchrow") == 0) {
		return fbsql_fetchrow(interp,sql_number,argc-2,argv+2);
	} else if (strcmp(argv[1],"endquery") == 0) {
		return fbsql_endquery(interp,sql_number,argc-2,argv+2);
	} else {
		/* assume query command shortform: sql STATEMENT */
		return fbsql_query(interp,sql_number,argc-1,argv+1);
	}
}

/*******************************************************************/
/* tcl fbwait command */
/*******************************************************************/

static char *		WaitVariableProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));

static char *
WaitVariableProc(clientData, interp, name1, name2, flags)
ClientData clientData;	/* Pointer to integer to set to 1. */
Tcl_Interp *interp;		/* Interpreter containing variable. */
char *name1;		/* Name of variable. */
char *name2;		/* Second part of variable name. */
int flags;			/* Information about what happened. */
{
	done = 1;
	return (char *) NULL;
}

int fbWaitCmd(clientData, interp, argc, argv)
ClientData clientData;					/* Main window associated with interpreter. */
Tcl_Interp *interp;					/* Current interpreter. */
int argc;					/* Number of arguments. */
const char **argv;					/* Argument strings. */
{
	if (argc != 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " variable_name\" (based on tkwait var name)", (char *) NULL);
		return TCL_ERROR;
	}

	if (Tcl_TraceVar(interp, argv[1],
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		WaitVariableProc,
		(ClientData) 0) != TCL_OK) {
		return TCL_ERROR;
	}

	done = 0;
	while (!done) {
		/* process file and timer events only, ie: exclude window and idle */
		Tcl_DoOneEvent(TCL_FILE_EVENTS | TCL_TIMER_EVENTS);
	}

	Tcl_UntraceVar(interp, argv[1],
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		WaitVariableProc,
		(ClientData) 0);

	/* Clear out the interpreter's result, since it may have been set by event handlers */
	Tcl_ResetResult(interp);

	return TCL_OK;
}

/*******************************************************************/
/* define the TCL initialisation code to create the "fbsql" command */
/*******************************************************************/

/* haven't tested this yet (for building on Unix) */
#ifdef WINDOWS
extern __declspec(dllexport) int Fbsql_Init(Tcl_Interp *interp);
#else
extern int Fbsql_Init(Tcl_Interp *interp);
#endif

int Fbsql_Init(Tcl_Interp *interp) {
	int i;
	char command_name[10];

	/* stubs? */
	if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
		return TCL_ERROR;
	}

	/* create the main "sql" command */
	Tcl_CreateCommand (interp, "sql", SqlCmd, (ClientData) 0, (Tcl_CmdDeleteProc*) NULL);

	/* create new "fbwait" command */
	Tcl_CreateCommand (interp, "fbwait", fbWaitCmd, (ClientData) 0, (Tcl_CmdDeleteProc*) NULL);

	/* initialise the connection structure */
	for (i = 0; i < SQL_COMMANDS; i++) {
		connection[i].CONNECTED = 0;
		connection[i].query_flag = 0;
		if (i > 0) {
			/* create the extra commands "sql1, sql2, sql3, sql4, sql5" */
			/* these commands are the same as "sql" but use separate mysql connections */
			sprintf(command_name,"sql%d",i);
			Tcl_CreateCommand (interp, command_name, SqlCmd, (ClientData) i, (Tcl_CmdDeleteProc*) NULL);
		}
	}

	return (Tcl_PkgProvide(interp,"Fbsql","1.06") == TCL_ERROR ? TCL_ERROR : TCL_OK);
}
