/*
**  Copyright (c) 2010, The OpenDKIM Project.  All rights reserved.
**
**  $Id: opendkim-importstats.c,v 1.2 2010/09/01 22:51:48 cm-msk Exp $
*/

#ifndef lint
static char opendkim_importstats_c_id[] = "$Id: opendkim-importstats.c,v 1.2 2010/09/01 22:51:48 cm-msk Exp $";
#endif /* ! lint */

/* system includes */
#include <sys/param.h>
#include <sys/types.h>
#include <sysexits.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* OpenDKIM includes */
#include "build-config.h"

/* libodbx includes */
#ifdef USE_ODBX
# include <odbx.h>
#else /* USE_ODBX */
# error OpenDBX is required for opendkim-importstats
#endif /* USE_ODBX */

/* macros, definitions */
#define	CMDLINEOPTS	"d:h:mP:p:s:u:"

#define	DEFDBHOST	"localhost"
#define	DEFDBNAME	"opendkim"
#define	DEFDBSCHEME	"mysql"
#define	DEFDBUSER	"opendkim"

#define	MAXLINE		2048
#define	MAXREPORTER	256

/* globals */
char *progname;
char reporter[MAXREPORTER + 1];

/*
**  SANITIZE -- sanitize a string
**
**  Parameters:
**  	db -- DB handle
**  	in -- input string
**  	out -- output buffer
**  	len -- bytes available at "out"
**
**  Return value:
**  	0 == string was safe
**  	1 == string was not safe
*/

int
sanitize(odbx_t *db, char *in, char *out, size_t len)
{
	size_t outlen;

	assert(db != NULL);
	assert(in != NULL);
	assert(out != NULL);

	memset(out, '\0', len);

	outlen = len;

	(void) odbx_escape(db, in, strlen(in), out, &outlen);

	return (strncmp(in, out, outlen) != 0);
}

/*
**  FINDINLIST -- see if a particular string appears in another list
**
**  Parameters:
**  	str -- string to find
**  	list -- colon-separated list of strings to search
**
**  Return value:
**  	1 -- "str" found in "list"
**  	0 -- "str" not found in "list"
*/

int
findinlist(char *str, char *list)
{
	int len;
	char *p;
	char *q;

	assert(str != NULL);
	assert(list != NULL);

	len = strlen(str);

	q = list;

	for (p = list; ; p++)
	{
		if (*p == ':' || *p == '\0')
		{
			if (p - q == len && strncasecmp(str, q, len) == 0)
				return 1;
		}

		if (*p == '\0')
			break;

		q = p + 1;
	}

	return 0;
}

/*
**  SQL_GET_INT -- retrieve a single integer from an SQL query
**
**  Parameters:
**  	db -- DB handle
**  	sql -- SQL query to perform
**
**  Return value:
**  	-1 -- error
**  	0 -- no record matched
**  	1 -- extracted value
*/

int
sql_get_int(odbx_t *db, char *sql)
{
	int out = 0;
	int err;
	size_t safelen;
	odbx_result_t *result = NULL;

	assert(db != NULL);
	assert(sql != NULL);

	err = odbx_query(db, sql, strlen(sql));
	if (err < 0)
	{
		fprintf(stderr, "%s: odbx_query(): %s\n",
		        progname, odbx_error(db, err));
		return -1;
	}

	err = odbx_result(db, &result, NULL, 0);
	if (err < 0)
	{
		fprintf(stderr, "%s: odbx_result(): %s\n",
		        progname, odbx_error(db, err));
		return -1;
	}

	for (;;)
	{
		err = odbx_row_fetch(result);
		if (err == ODBX_ROW_DONE)
		{
			break;
		}
		else if (err < 0)
		{
			fprintf(stderr, "%s: odbx_row_fetch(): %s\n",
			        progname, odbx_error(db, err));
			return -1;
		}

		if (out == 0)
		{
			char *p;

			out = strtol(odbx_field_value(result, 0), &p, 10);
			if (*p != '\0')
			{
				fprintf(stderr, "%s: malformed integer\n",
				        progname);
				odbx_result_finish(result);
				return -1;
			}
		}
	}

	odbx_result_finish(result);

	return out;
}

/*
**  SQL_DO -- perform an SQL operation with no result
**
**  Parameters:
**  	db -- DB handle
**  	sql -- SQL action to perform
**
**  Return value:
**  	-1 -- error
**  	0 -- success
*/

int
sql_do(odbx_t *db, char *sql)
{
	int out = 0;
	int err;
	size_t safelen;
	odbx_result_t *result = NULL;

	assert(db != NULL);
	assert(sql != NULL);

	err = odbx_query(db, sql, strlen(sql));
	if (err < 0)
	{
		fprintf(stderr, "%s: odbx_query(): %s\n",
		        progname, odbx_error(db, err));
		return -1;
	}

	err = odbx_result(db, &result, NULL, 0);
	if (err < 0)
	{
		fprintf(stderr, "%s: odbx_result(): %s\n",
		        progname, odbx_error(db, err));
		return -1;
	}

	if (err != 2)
		return -1;

	odbx_result_finish(result);
}

/*
**  USAGE -- print usage message and exit
**
**  Parameters:
**  	None.
**
**  Return value:
**  	EX_USAGE
*/

int
usage(void)
{
	fprintf(stderr, "%s: usage: %s [options]\n"
	                "\t-d dbname  \tdatabase name (default: \"%s\")\n"
	                "\t-h dbhost  \tdatabase host/address (default: \"%s\")\n"
	                "\t-m         \tinput is in email format\n"
	                "\t-P dbport  \tdatabase port\n"
	                "\t-p dbpasswd\tdatabase password\n"
	                "\t-s dbscheme\tdatabase scheme (default: \"%s\")\n"
	                "\t-u dbuser  \tdatabase user (default: \"%s\")\n",
	        progname, progname, DEFDBNAME, DEFDBHOST, DEFDBSCHEME,
	        DEFDBUSER);

	return EX_USAGE;
}

/*
**  MAIN -- program mainline
**
**  Parameters:
**  	argc, argv -- the usual
**
**  Return value:
**  	Exit status.
*/

int
main(int argc, char **argv)
{
	int c;
	int n;
	int nfields = 0;
	int line;
	int err;
	int mail = 0;
	int skipsigs = 0;
	int repid;
	int domid;
	int msgid;
	int sigid;
	int hdrid;
	char *p;
	char *dbhost = DEFDBHOST;
	char *dbname = DEFDBNAME;
	char *dbscheme = DEFDBSCHEME;
	char *dbuser = DEFDBUSER;
	char *dbpassword = NULL;
	char *dbport = NULL;
	char **fields = NULL;
	odbx_t *db = NULL;
	char buf[MAXLINE + 1];
	char sql[MAXLINE + 1];
	char safesql[MAXLINE * 2 + 1];

	progname = (p = strrchr(argv[0], '/')) == NULL ? argv[0] : p + 1;

	while ((c = getopt(argc, argv, CMDLINEOPTS)) != -1)
	{
		switch (c)
		{
		  case 'd':
			dbname = optarg;
			break;

		  case 'h':
			dbhost = optarg;
			break;

		  case 'm':
			mail = 1;
			break;

		  case 'P':
			dbport = optarg;
			break;

		  case 'p':
			dbpassword = optarg;
			break;

		  case 's':
			dbscheme = optarg;
			break;

		  case 'u':
			dbuser = optarg;
			break;

		  default:
			return usage();
		}
	}

	/* try to connect to the database */
	if (odbx_init(&db, dbscheme, dbhost, dbport) < 0)
	{
		fprintf(stderr, "%s: odbx_init() failed\n", progname);
		return EX_SOFTWARE;
	}

	/* bind with user, password, database information */
	err = odbx_bind(db, dbname, dbuser, dbpassword, ODBX_BIND_SIMPLE);
	if (err < 0)
	{
		fprintf(stderr, "%s: odbx_bind(): %s\n", progname,
		        odbx_error(db, err));
		(void) odbx_finish(db);
		return EX_SOFTWARE;
	}

	/* initialize stuff */
	memset(buf, '\0', sizeof buf);
	memset(reporter, '\0', sizeof reporter);
	line = 0;
	repid = 0;
	msgid = 0;
	sigid = 0;

	/* read lines from stdin */
	while (fgets(buf, sizeof buf - 1, stdin) != NULL)
	{
		line++;

		/* eat the newline */
		for (p = buf; *p != '\0'; p++)
		{
			if (*p == '\n')
			{
				*p = '\0';
				break;
			}
		}

		if (mail == 1)
		{
			if (strlen(buf) > 0)
				continue;

			mail = 0;
			continue;
		}

		/* first byte identifies the record type */
		c = buf[0];

		/* reset fields array */
		if (fields != NULL)
			memset(fields, '\0', sizeof(char *) * nfields);

		/* now break out the fields */
		n = 0;
		for (p = strtok(buf + 1, "\t");
		     p != NULL;
		     p = strtok(NULL, "\t"))
		{
			if (nfields == n)
			{
				int newnf;
				size_t newsz;
				char **new;

				newnf = MAX(nfields * 2, 8);
				newsz = sizeof(char *) * newnf;

				if (nfields == 0)
					new = (char **) malloc(newsz);
				else
					new = (char **) realloc(fields, newsz);

				if (new == NULL)
				{
					fprintf(stderr,
					        "%s: %salloc(): %s\n",
					        progname,
					        fields == NULL ? "m" : "re",
					        strerror(errno));
					(void) odbx_finish(db);
					return EX_OSERR;
				}

				nfields = newnf;
				fields = new;
			}

			fields[n++] = p;
		}

		sigid = 0;
		hdrid = 0;

		/* processing section for messages */
		if (c == '\0')
		{
			continue;
		}
		else if (c == 'M')
		{
			if (n != 17)
			{
				fprintf(stderr,
				        "%s: unexpected field count at input line %d\n",
				        progname, line);
				continue;
			}

			skipsigs = 0;

			/* get, or create, the reporter ID if needed */
			if (strcasecmp(reporter, fields[1]) != 0)
			{
				(void) sanitize(db, fields[1], safesql,
				                sizeof safesql);

				snprintf(sql, sizeof sql,
				         "SELECT id FROM reporters WHERE name = '%s'",
				         safesql);

				repid = sql_get_int(db, sql);
				if (repid == -1)
				{
					(void) odbx_finish(db);
					return EX_SOFTWARE;
				}
				else if (repid == 0)
				{
					snprintf(sql, sizeof sql,
					         "INSERT INTO reporters (name) VALUES ('%s')",
					         safesql);

					repid = sql_do(db, sql);
					if (repid == -1)
					{
						(void) odbx_finish(db);
						return EX_SOFTWARE;
					}

					snprintf(sql, sizeof sql,
					         "SELECT LAST_INSERT_ID()");

					repid = sql_get_int(db, sql);
					if (repid == -1)
					{
						(void) odbx_finish(db);
						return EX_SOFTWARE;
					}
					else if (repid == 0)
					{
						fprintf(stderr,
						        "%s: failed to create reporter record for `%s'\n",
						        progname,
						        fields[1]);
						(void) odbx_finish(db);
						return EX_SOFTWARE;
					}
				}

				strlcpy(reporter, fields[1], sizeof reporter);
			}

			/* get, or create, the domain ID if needed */
			(void) sanitize(db, fields[2], safesql,
			                sizeof safesql);

			snprintf(sql, sizeof sql,
			         "SELECT id FROM domains WHERE name = '%s'",
			         safesql);

			domid = sql_get_int(db, sql);
			if (domid == -1)
			{
				(void) odbx_finish(db);
				return EX_SOFTWARE;
			}
			else if (domid == 0)
			{
				snprintf(sql, sizeof sql,
				         "INSERT INTO domains (name) VALUES ('%s')",
				         safesql);

				domid = sql_do(db, sql);
				if (domid == -1)
				{
					(void) odbx_finish(db);
					return EX_SOFTWARE;
				}

				snprintf(sql, sizeof sql,
				         "SELECT LAST_INSERT_ID()");

				domid = sql_get_int(db, sql);
				if (domid == -1)
				{
					(void) odbx_finish(db);
					return EX_SOFTWARE;
				}
				else if (domid == 0)
				{
					fprintf(stderr,
					        "%s: failed to create domain record for `%s'\n",
					        progname, fields[2]);
					(void) odbx_finish(db);
					return EX_SOFTWARE;
				}
			}

			/* verify data safety */
			if (sanitize(db, fields[0], safesql, sizeof safesql) ||
			    sanitize(db, fields[3], safesql, sizeof safesql) ||
			    sanitize(db, fields[4], safesql, sizeof safesql) ||
			    sanitize(db, fields[5], safesql, sizeof safesql) ||
			    sanitize(db, fields[6], safesql, sizeof safesql) ||
			    sanitize(db, fields[7], safesql, sizeof safesql) ||
			    sanitize(db, fields[8], safesql, sizeof safesql) ||
			    sanitize(db, fields[9], safesql, sizeof safesql) ||
			    sanitize(db, fields[10], safesql, sizeof safesql) ||
			    sanitize(db, fields[11], safesql, sizeof safesql) ||
			    sanitize(db, fields[12], safesql, sizeof safesql) ||
			    sanitize(db, fields[13], safesql, sizeof safesql) ||
			    sanitize(db, fields[14], safesql, sizeof safesql) ||
			    sanitize(db, fields[15], safesql, sizeof safesql) ||
			    sanitize(db, fields[16], safesql, sizeof safesql))
			{
				fprintf(stderr,
				        "%s: unsafe data at input line %d\n",
				        progname, line);
				continue;
			}

			/* see if this is a duplicate */
			snprintf(sql, sizeof sql,
			         "SELECT id FROM messages WHERE jobid = '%s' and reporter = %d and msgtime = from_unixtime(%s)",
			         fields[0], repid, fields[5]);

			msgid = sql_get_int(db, sql);
			if (msgid == -1)
			{
				(void) odbx_finish(db);
				return EX_SOFTWARE;
			}
			else if (msgid != 0)
			{
				fprintf(stderr,
				        "%s: skipping duplicate message at line %d\n",
				        progname, line);
				skipsigs = 1;
				continue;
			}

			snprintf(sql, sizeof sql,
			         "INSERT INTO messages (jobid, reporter, from_domain, ipaddr, anonymized, msgtime, size, sigcount, adsp_found, adsp_unknown, adsp_all, adsp_discardable, adsp_fail, mailing_list, received_count, content_type, content_encoding) VALUES ('%s', %d, %d, '%s', %s, from_unixtime(%s), %s, %s, %s, %s, %s, %s, %s, %s, %s, '%s', '%s')",
			         fields[0],		/* jobid */
			         repid,			/* reporter */
			         domid,			/* from_domain */
			         fields[3],		/* ipaddr */
			         fields[4],		/* anonymized */
			         fields[5],		/* msgtime */
			         fields[6],		/* size */
			         fields[7],		/* sigcount */
			         fields[8],		/* adsp_found */
			         fields[9],		/* adsp_unknown */
			         fields[10],		/* adsp_all */
			         fields[11],		/* adsp_discardable */
			         fields[12],		/* adsp_fail */
			         fields[13],		/* mailing_list */
			         fields[14],		/* received_count */
			         fields[15],		/* content_type */
			         fields[16]);		/* content_encoding */

			msgid = sql_do(db, sql);
			if (msgid == -1)
			{
				(void) odbx_finish(db);
				return EX_SOFTWARE;
			}

			/* get back the message ID */
			snprintf(sql, sizeof sql, "SELECT LAST_INSERT_ID()");

			msgid = sql_get_int(db, sql);
			if (msgid == -1)
			{
				(void) odbx_finish(db);
				return EX_SOFTWARE;
			}
			else if (msgid == 0)
			{
				fprintf(stderr,
				        "%s: failed to create message record for `%s'\n",
				        progname, fields[0]);
				(void) odbx_finish(db);
				return EX_SOFTWARE;
			}
		}

		/* processing section for signatures */
		else if (c == 'S')
		{
			int changed;

			if (n != 19)
			{
				fprintf(stderr,
				        "%s: unexpected field count at input line %d\n",
				        progname, line);
				continue;
			}
			else if (msgid <= 0)
			{
				fprintf(stderr,
				        "%s: signature record before message record at input line %d\n",
				        progname, line);
				continue;
			}
			else if (skipsigs == 1)
			{
				continue;
			}

			/* get, or create, the domain ID if needed */
			(void) sanitize(db, fields[0], safesql,
			                sizeof safesql);

			snprintf(sql, sizeof sql,
			         "SELECT id FROM domains WHERE name = '%s'",
			         safesql);

			domid = sql_get_int(db, sql);
			if (domid == -1)
			{
				(void) odbx_finish(db);
				return EX_SOFTWARE;
			}
			else if (domid == 0)
			{
				snprintf(sql, sizeof sql,
				         "INSERT INTO domains (name) VALUES ('%s')",
				         safesql);

				domid = sql_do(db, sql);
				if (domid == -1)
				{
					(void) odbx_finish(db);
					return EX_SOFTWARE;
				}

				snprintf(sql, sizeof sql,
				         "SELECT LAST_INSERT_ID()");

				domid = sql_get_int(db, sql);
				if (domid == -1)
				{
					(void) odbx_finish(db);
					return EX_SOFTWARE;
				}
				else if (domid == 0)
				{
					fprintf(stderr,
					        "%s: failed to create domain record for `%s'\n",
					        progname, fields[0]);
					(void) odbx_finish(db);
					return EX_SOFTWARE;
				}
			}

			if (sanitize(db, fields[1], safesql, sizeof safesql) ||
			    sanitize(db, fields[2], safesql, sizeof safesql) ||
			    sanitize(db, fields[3], safesql, sizeof safesql) ||
			    sanitize(db, fields[4], safesql, sizeof safesql) ||
			    sanitize(db, fields[5], safesql, sizeof safesql) ||
			    sanitize(db, fields[6], safesql, sizeof safesql) ||
			    sanitize(db, fields[7], safesql, sizeof safesql) ||
			    sanitize(db, fields[8], safesql, sizeof safesql) ||
			    sanitize(db, fields[9], safesql, sizeof safesql) ||
			    sanitize(db, fields[10], safesql, sizeof safesql) ||
			    sanitize(db, fields[11], safesql, sizeof safesql) ||
			    sanitize(db, fields[12], safesql, sizeof safesql) ||
			    sanitize(db, fields[13], safesql, sizeof safesql) ||
			    sanitize(db, fields[14], safesql, sizeof safesql) ||
			    sanitize(db, fields[15], safesql, sizeof safesql) ||
			    sanitize(db, fields[16], safesql, sizeof safesql) ||
			    sanitize(db, fields[17], safesql, sizeof safesql) ||
			    sanitize(db, fields[18], safesql, sizeof safesql))
			{
				fprintf(stderr,
				        "%s: unsafe data at input line %d\n",
				        progname, line);
				continue;
			}

			snprintf(sql, sizeof sql,
			         "INSERT INTO signatures (message, domain, algorithm, hdr_canon, body_canon, ignored, pass, fail_body, siglength, key_t, key_g, key_g_name, key_dk_compat, sigerror, sig_t, sig_x, sig_z, dnssec) VALUES (%d, %d, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)",
			         msgid,			/* message */
			         domid,			/* domain */
			         fields[1],		/* algorithm */
			         fields[2],		/* hdr_canon */
			         fields[3],		/* body_canon */
			         fields[4],		/* ignored */
			         fields[5],		/* pass */
			         fields[6],		/* fail_body */
			         fields[7],		/* siglength */
			         fields[8],		/* key_t */
			         fields[9],		/* key_g */
			         fields[10],		/* key_g_name */
			         fields[11],		/* key_dk_compat */
			         fields[12],		/* sigerror */
			         fields[13],		/* sig_t */
			         fields[14],		/* sig_x */
			         fields[15],		/* sig_z */
			         fields[16]);		/* dnssec */

			sigid = sql_do(db, sql);
			if (sigid == -1)
			{
				(void) odbx_finish(db);
				return EX_SOFTWARE;
			}

			/* get back the signature ID */
			snprintf(sql, sizeof sql, "SELECT LAST_INSERT_ID()");

			sigid = sql_get_int(db, sql);
			if (sigid == -1)
			{
				(void) odbx_finish(db);
				return EX_SOFTWARE;
			}
			else if (sigid == 0)
			{
				fprintf(stderr,
				        "%s: failed to create signature record for input line %d\n",
				        progname, line);
				(void) odbx_finish(db);
				return EX_SOFTWARE;
			}

			snprintf(sql, sizeof sql, "SELECT LAST_INSERT_ID()");

			sigid = sql_get_int(db, sql);
			if (sigid == -1)
			{
				(void) odbx_finish(db);
				return EX_SOFTWARE;
			}
			else if (sigid == 0)
			{
				fprintf(stderr,
				        "%s: failed to create domain record for `%s'\n",
				        progname, fields[0]);
				(void) odbx_finish(db);
				return EX_SOFTWARE;
			}

			for (p = strtok(fields[17], ":");
			     p != NULL;
			     p = strtok(NULL, ":"))
			{
				(void) sanitize(db, p, safesql,
				                sizeof safesql);

				/* get, or create, the header ID if needed */
				snprintf(sql, sizeof sql,
				         "SELECT id FROM headernames WHERE name = '%s'",
				         safesql);

				hdrid = sql_get_int(db, sql);
				if (hdrid == -1)
				{
					(void) odbx_finish(db);
					return EX_SOFTWARE;
				}
				else if (hdrid == 0)
				{
					snprintf(sql, sizeof sql,
					         "INSERT INTO headernames (name) VALUES ('%s')",
					         safesql);

					hdrid = sql_do(db, sql);
					if (hdrid == -1)
					{
						(void) odbx_finish(db);
						return EX_SOFTWARE;
					}

					snprintf(sql, sizeof sql,
					         "SELECT LAST_INSERT_ID()");

					hdrid = sql_get_int(db, sql);
					if (hdrid == -1)
					{
						(void) odbx_finish(db);
						return EX_SOFTWARE;
					}
					else if (hdrid == 0)
					{
						fprintf(stderr,
						        "%s: failed to create header record for `%s'\n",
						        progname, p);
						(void) odbx_finish(db);
						return EX_SOFTWARE;
					}
				}

				changed = findinlist(p, fields[18]);

				snprintf(sql, sizeof sql,
				         "INSERT INTO signed_headers (signature, header, changed) VALUES (%d, %d, %d)",
				         sigid, hdrid, changed);

				hdrid = sql_do(db, sql);
				if (hdrid == -1)
				{
					(void) odbx_finish(db);
					return EX_SOFTWARE;
				}
			}
		}

		/* unknown record type */
		else
		{
			fprintf(stderr,
			        "%s: unknown record type '%c' at input line %d\n",
			        progname, c, line);
		}
	}

	if (fields != NULL)
		free(fields);

	if (ferror(stdin))
	{
		fprintf(stderr, "%s: fgets(): %s\n", progname,
		        strerror(errno));
		(void) odbx_finish(db);
		return EX_OSERR;
	}

	/* unbind */
	err = odbx_unbind(db);
	if (err < 0)
	{
		fprintf(stderr, "%s: odbx_unbind(): %s\n", progname,
		        odbx_error(db, err));
		(void) odbx_finish(db);
		return EX_SOFTWARE;
	}

	/* shut down */
	if (odbx_finish(db) < 0)
	{
		fprintf(stderr, "%s: odbx_finish() failed\n", progname);
		return EX_SOFTWARE;
	}

	return EX_OK;
}