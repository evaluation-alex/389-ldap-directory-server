/* --- BEGIN COPYRIGHT BLOCK ---
 * Copyright (C) 2001 Sun Microsystems, Inc. Used by permission.
 * Copyright (C) 2005 Red Hat, Inc.
 * All rights reserved.
 *
 * License: GPL (version 3 or any later version).
 * See LICENSE for details.
 * --- END COPYRIGHT BLOCK --- */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/**
 * Simple Http Client API broker plugin
 */

#include <stdio.h>
#include <string.h>
#include "portable.h"
#include "nspr.h"
#include "slapi-plugin.h"
#include "slapi-private.h"
#include "http_client.h"
#include "http_impl.h"
#include <sys/stat.h>

#define HTTP_PLUGIN_SUBSYSTEM "http-client-plugin" /* used for logging */
#define HTTP_PLUGIN_VERSION 0x00050050

#define HTTP_SUCCESS 0
#define HTTP_FAILURE -1

/**
 * Implementation functions
 */
static void *api[7];

/**
 * Plugin identifiers
 */
static Slapi_PluginDesc pdesc = {"http-client",
                                 VENDOR,
                                 DS_PACKAGE_VERSION,
                                 "HTTP Client plugin"};

static Slapi_ComponentId *plugin_id = NULL;

/**
 **
 **    Http plug-in management functions
 **
 **/
int http_client_init(Slapi_PBlock *pb);
static int http_client_start(Slapi_PBlock *pb);
static int http_client_close(Slapi_PBlock *pb);

/**
 * our functions
 */
static void _http_init(Slapi_ComponentId *plugin_id);
static int _http_get_text(char *url, char **data, int *bytesRead);
static int _http_get_binary(char *url, char **data, int *bytesRead);
static int _http_get_redirected_uri(char *url, char **data, int *bytesRead);
static int _http_post(char *url, httpheader **httpheaderArray, char *body, char **data, int *bytesRead);
static void _http_shutdown(void);

/**
 *
 * Get the presence plug-in version
 *
 */
int
http_client_version(void)
{
    return HTTP_PLUGIN_VERSION;
}

int
http_client_init(Slapi_PBlock *pb)
{
    int status = HTTP_SUCCESS;
    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "http_client_init - BEGIN\n");

    if (slapi_pblock_set(pb, SLAPI_PLUGIN_VERSION,
                         SLAPI_PLUGIN_VERSION_01) != 0 ||
        slapi_pblock_set(pb, SLAPI_PLUGIN_START_FN,
                         (void *)http_client_start) != 0 ||
        slapi_pblock_set(pb, SLAPI_PLUGIN_CLOSE_FN,
                         (void *)http_client_close) != 0 ||
        slapi_pblock_set(pb, SLAPI_PLUGIN_DESCRIPTION,
                         (void *)&pdesc) != 0) {
        slapi_log_err(SLAPI_LOG_ERR, HTTP_PLUGIN_SUBSYSTEM,
                      "http_client_init - Failed to register plugin\n");
        status = HTTP_FAILURE;
    }

    /* Retrieve and save the plugin identity to later pass to
        internal operations */
    if (slapi_pblock_get(pb, SLAPI_PLUGIN_IDENTITY, &plugin_id) != 0) {
        slapi_log_err(SLAPI_LOG_ERR, HTTP_PLUGIN_SUBSYSTEM,
                      "http_client_init - Failed to retrieve SLAPI_PLUGIN_IDENTITY\n");
        return HTTP_FAILURE;
    }

    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "http_client_init - END\n");
    return status;
}

static int
http_client_start(Slapi_PBlock *pb __attribute__((unused)))
{
    int status = HTTP_SUCCESS;
    /**
     * do some init work here
     */
    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "http_client_start - BEGIN\n");

    api[0] = 0; /* reserved for api broker use, must be zero */
    api[1] = (void *)_http_init;
    api[2] = (void *)_http_get_text;
    api[3] = (void *)_http_get_binary;
    api[4] = (void *)_http_get_redirected_uri;
    api[5] = (void *)_http_shutdown;
    api[6] = (void *)_http_post;

    if (slapi_apib_register(HTTP_v1_0_GUID, api)) {
        slapi_log_err(SLAPI_LOG_ERR, HTTP_PLUGIN_SUBSYSTEM,
                      "http_client_start: failed to register functions\n");
        status = HTTP_FAILURE;
    }

    _http_init(plugin_id);

    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "http_client_start - END\n");
    return status;
}

static int
http_client_close(Slapi_PBlock *pb __attribute__((unused)))
{
    int status = HTTP_SUCCESS;
    /**
     * do cleanup
     */
    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "http_client_close - BEGIN\n");

    slapi_apib_unregister(HTTP_v1_0_GUID);

    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "http_client_close - END\n");

    return status;
}

/**
 * perform http initialization here
 */
static void
_http_init(Slapi_ComponentId *plugin_id)
{
    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "_http_init - BEGIN\n");

    http_impl_init(plugin_id);

    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "_http_init - END\n");
}

/**
 * This method gets the data in a text format based on the
 * URL send.
 */
static int
_http_get_text(char *url, char **data, int *bytesRead)
{
    int status = HTTP_SUCCESS;
    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "_http_get_text - BEGIN\n");

    status = http_impl_get_text(url, data, bytesRead);

    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "_http_get_text - END\n");
    return status;
}

/**
 * This method gets the data in a binary format based on the
 * URL send.
 */
static int
_http_get_binary(char *url, char **data, int *bytesRead)
{
    int status = HTTP_SUCCESS;
    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "_http_get_binary - BEGIN\n");

    status = http_impl_get_binary(url, data, bytesRead);

    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "_http_get_binary - END\n");
    return status;
}

/**
 * This method intercepts the redirected URI and returns the location
 * information.
 */
static int
_http_get_redirected_uri(char *url, char **data, int *bytesRead)
{
    int status = HTTP_SUCCESS;
    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "_http_get_redirected_uri -- BEGIN\n");

    status = http_impl_get_redirected_uri(url, data, bytesRead);

    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "_http_get_redirected_uri -- END\n");
    return status;
}

/**
 * This method posts the data based on the URL send.
 */
static int
_http_post(char *url, httpheader **httpheaderArray, char *body, char **data, int *bytesRead)
{
    int status = HTTP_SUCCESS;
    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "_http_post - BEGIN\n");

    status = http_impl_post(url, httpheaderArray, body, data, bytesRead);

    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "_http_post - END\n");
    return status;
}

/**
 * perform http shutdown here
 */
static void
_http_shutdown(void)
{
    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "_http_shutdown - BEGIN\n");

    http_impl_shutdown();

    slapi_log_err(SLAPI_LOG_PLUGIN, HTTP_PLUGIN_SUBSYSTEM, "_http_shutdown - END\n");
}
