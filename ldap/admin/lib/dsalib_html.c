/** BEGIN COPYRIGHT BLOCK
 * This Program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2 of the License.
 * 
 * This Program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this Program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA.
 * 
 * In addition, as a special exception, Red Hat, Inc. gives You the additional
 * right to link the code of this Program with code not covered under the GNU
 * General Public License ("Non-GPL Code") and to distribute linked combinations
 * including the two, subject to the limitations in this paragraph. Non-GPL Code
 * permitted under this exception must only link to the code of this Program
 * through those well defined interfaces identified in the file named EXCEPTION
 * found in the source code files (the "Approved Interfaces"). The files of
 * Non-GPL Code may instantiate templates or use macros or inline functions from
 * the Approved Interfaces without causing the resulting work to be covered by
 * the GNU General Public License. Only Red Hat, Inc. may make changes or
 * additions to the list of Approved Interfaces. You must obey the GNU General
 * Public License in all respects for all of the Program code and other code used
 * in conjunction with the Program except the Non-GPL Code covered by this
 * exception. If you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you do not wish to
 * provide this exception without modification, you must delete this exception
 * statement from your version and license this file solely under the GPL without
 * exception. 
 * 
 * 
 * Copyright (C) 2001 Sun Microsystems, Inc. Used by permission.
 * Copyright (C) 2005 Red Hat, Inc.
 * All rights reserved.
 * END COPYRIGHT BLOCK **/
#if defined( XP_WIN32 )
#include <windows.h>
#endif

#include "dsalib.h"
#include "nspr.h"
#include "prprf.h"
#include "plstr.h"
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* holds the contents of the POSTed data from stdin as an array
of key/value pairs parsed from the key=value input */
static char **input = 0;

/* This variable is true if the program should assume stdout is connected
   to an HTML context e.g. if this is being run as a normal CGI.  It is
   false to indicate that output should not contain HTML formatting,
   Javascript, etc. put plain ol' ASCII only
*/
static int formattedOutput = 1;

DS_EXPORT_SYMBOL int
ds_get_formatted_output(void)
{
	return formattedOutput;
}

DS_EXPORT_SYMBOL void
ds_set_formatted_output(int val)
{
	formattedOutput = val;
}

DS_EXPORT_SYMBOL void
ds_print_file_name(char *fileptr)
{
    char        *meaning;

    fprintf(stdout, "%s", fileptr);
    if ( (meaning = ds_get_file_meaning(fileptr)) != NULL ) {
        fprintf(stdout, " (%s)", meaning);
    }
}

/*
 * Get a CGI variable.
 */
DS_EXPORT_SYMBOL char *
ds_get_cgi_var(char *cgi_var_name)
{
    char        *cgi_var_value;
 
    cgi_var_value = (char *) ds_a_get_cgi_var(cgi_var_name, NULL, NULL);
    if ( cgi_var_value == NULL ) {
	/*
	 * The ds_a_get_cgi_var() lies! It gives a NULL even if the 
	 * value is "". So assume the variable is there and 
	 * return an empty string.
	 */
        return("");
    }
    return(cgi_var_value);
}

/* parse POST input to a CGI program */
DS_EXPORT_SYMBOL int
ds_post_begin(FILE *in) 
{
    char *vars = NULL, *tmp = NULL, *decoded_vars = NULL;
    int cl;

    if(!(tmp = getenv("CONTENT_LENGTH")))
	{
        ds_report_error(DS_INCORRECT_USAGE, "Browser Error", "Your browser"
						" sent no content length with a POST command."
						"  Please be sure to use a fully compliant browser.");
		return 1;
	}

    cl = atoi(tmp);

    vars = (char *)malloc(cl+1);

    if( !(fread(vars, 1, cl, in)) )
	{
        ds_report_error(DS_SYSTEM_ERROR, "CGI error",
						"The POST variables could not be read from stdin.");
		return 1;
	}

    vars[cl] = '\0';

	decoded_vars = ds_URL_decode(vars);
	free(vars);

    input = ds_string_to_vec(decoded_vars);
	free(decoded_vars);
/*
	for (cl = 0; input[cl]; ++cl)
		printf("ds_post_begin: read cgi var=[%s]\n", input[cl]);
*/
	return 0;
}

/* parse GET input to a CGI program */
DS_EXPORT_SYMBOL void
ds_get_begin(char *query_string)
{
	char *decoded_input = ds_URL_decode(query_string);
	input = ds_string_to_vec(decoded_input);
	free(decoded_input);
}

/*
  Borrowed from libadmin/form_post.c
*/
DS_EXPORT_SYMBOL char *
ds_a_get_cgi_var(char *varname, char *elem_id, char *bongmsg)
{
    register int x = 0;
    int len = strlen(varname);
    char *ans = NULL;
   
    while(input[x])  {
    /*  We want to get rid of the =, so len, len+1 */
        if((!strncmp(input[x], varname, len)) && (*(input[x]+len) == '='))  {
            ans = strdup(input[x] + len + 1);
            if(!strcmp(ans, ""))
                ans = NULL;
            break;
        }  else
            x++;
    }
    if(ans == NULL)  {
        if ((bongmsg) && strlen(bongmsg))
		{
			/* prefix error with varname so output interpreters can determine */
			/* which parameter is in error */
			char *msg;
			if (!ds_get_formatted_output() && (varname != NULL))
			{
				msg = PR_smprintf("%s.error: %s %s", varname, elem_id, bongmsg);
			}
			else
			{
				msg = PR_smprintf("error: %s %s", elem_id, bongmsg);
			}
			ds_show_message(msg);
			PR_smprintf_free(msg);
		}
        else
            return NULL;
    }
    else
        return(ans);
    /* shut up gcc */
    return NULL;
}

/* (copied from adminutil/lib/libadminutil/form_post.c) */
void
form_unescape(char *str)
{
    register int x = 0, y = 0;
    int l = strlen(str);
    char digit;

    while(x < l)  {
        if((str[x] == '%') && (x < (l - 2)))  {
            ++x;
            digit = (str[x] >= 'A' ? 
                         ((str[x] & 0xdf) - 'A')+10 : (str[x] - '0'));
            digit *= 16;

            ++x;
            digit += (str[x] >= 'A' ? 
                         ((str[x] & 0xdf) - 'A')+10 : (str[x] - '0'));

            str[y] = digit;
        } 
        else if(str[x] == '+')  {
            str[y] = ' ';
        } else {
            str[y] = str[x];
        }
        x++;
        y++;
    }
    str[y] = '\0';
}

/*
 * (copied from adminutil/lib/libadminutil/form_post.c)
 * form_unescape_url_escape_html -- 1) unescape escaped chars in URL;
 *                                  2) escape unsecure chars for scripts
 * 1) "%##" is converted to one character which value is ##; so is '+' to ' '
 * 2) <, >, &, ", ' are escaped with "&XXX;" format
 */
static char *
form_unescape_url_escape_html(char *str) 
{
    register size_t x = 0, y = 0;
    size_t l = 0;
    char *rstr = NULL;

    if (NULL == str) {
        return NULL;
    }

    /* first, form_unescape to convert hex escapes to chars */
    form_unescape(str);

    /* next, allocate enough space for the escaped entities */
    for (x = 0, y = 0; str[x] != '\0'; x++) {
        if (('<' == str[x]) || ('>' == str[x]))
            y += 5;
        else if (('&' == str[x]) || ('\'' == str[x]))
            y += 6;
        else if ('"' == str[x])
            y += 7;
    }

    if (0 < y) {
        rstr = (char *)PR_Malloc(x + y + 2);
    } else {
        rstr = PL_strdup(str);
    }
    l = x; /* length of str */

    if (NULL == rstr) {
        ds_report_error(DS_SYSTEM_ERROR, "CGI error",
            "Could not allocate enough memory to escape the string.");
        return NULL;
    }

    if (y == 0) { /* no entities to escape - just return the string copy */
        return rstr;
    }

    for (x = 0, y = 0; x < l; x++, y++) {
        char digit = str[x];
        /*  see if digit (the original or the unescaped char)
            needs to be html encoded */
        if ('<' == digit) {
            memcpy(&rstr[y], "&lt;", 4);
            y += 3;
        } else if ('>' == digit) {
            memcpy(&rstr[y], "&gt;", 4);
            y += 3;
        } else if ('&' == digit) {
            memcpy(&rstr[y], "&amp;", 5);
            y += 4;
        } else if ('"' == digit) {
            memcpy(&rstr[y], "&quot;", 6);
            y += 5;
        } else if ('\'' == digit) {
            memcpy(&rstr[y], "&#39;", 5);
            y += 4;
        } else { /* just write the char to the output string */
            rstr[y] = digit;
        }
    }
    rstr[y] = '\0';
    return rstr;
}

DS_EXPORT_SYMBOL char **
ds_string_to_vec(char *in)
{
    char **ans;
    int vars = 0;
    register int x = 0;
    char *tmp;

    in = strdup(in);

    while(in[x])
        if(in[x++]=='=')
            vars++;

    
    ans = (char **)calloc(vars+1, sizeof(char *));
  
    x=0;
	/* strtok() is not MT safe, but it is okay to call here because it is used in monothreaded env */
    tmp = strtok(in, "&");
    if (!tmp || !strchr(tmp, '=')) { /* error, bail out */
        PR_Free(in);
        return(ans);
    }

    if (!(ans[x++] = form_unescape_url_escape_html(tmp))) {
        /* could not allocate enough memory */
        PR_Free(in);
        return ans;
    }

    while((tmp = strtok(NULL, "&")))  {
        if (!strchr(tmp, '=')) {
            PR_Free(in);
            return ans;
        }
        if (!(ans[x++] = form_unescape_url_escape_html(tmp))) {
            /* could not allocate enough memory */
            PR_Free(in);
            return ans;
		}
    }

    free(in);

    return(ans);
}
