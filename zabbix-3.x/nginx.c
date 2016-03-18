/*
** Zabbix
** Copyright (C) 2001-2014 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** Author: Cao Qingshan <caoqingshan@kingsoft.com>
**
** Version: 1.0 Last update: 2015-10-22 12:00
**
**/

#include "common.h"
#include "sysinc.h"
#include "module.h"
#include "cfg.h"
#include "log.h"
#include "zbxjson.h"
#include "comms.h"

/* the variable keeps timeout setting for item processing */
static int	item_timeout = 0;

/* the path of config file */
const char	ZBX_MODULE_NGINX_CONFIG_FILE[] = "/etc/zabbix/zbx_module_nginx.conf";

char		*CONFIG_NGINX_HOST = "127.0.0.1";
int		CONFIG_NGINX_PORT = 80;
char		*CONFIG_NGINX_DOMAIN = NULL;
char		*CONFIG_NGINX_STATUS_URI = "nginx_status";

#define ZBX_CFG_LTRIM_CHARS     "\t "
#define ZBX_CFG_RTRIM_CHARS     ZBX_CFG_LTRIM_CHARS "\r\n"

int	zbx_module_nginx_load_config(int requirement);
int	zbx_module_nginx_status(AGENT_REQUEST *request, AGENT_RESULT *result);

static ZBX_METRIC keys[] =
/*      KEY                     FLAG		        FUNCTION        	          TEST PARAMETERS */
{
	{"nginx.status",	CF_HAVEPARAMS,		zbx_module_nginx_status,	  "ac"},
	{NULL}
};

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_api_version                                           *
 *                                                                            *
 * Purpose: returns version number of the module interface                    *
 *                                                                            *
 * Return value: ZBX_MODULE_API_VERSION_ONE - the only version supported by   *
 *               Zabbix currently                                             *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_api_version()
{
	return ZBX_MODULE_API_VERSION_ONE;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_timeout                                          *
 *                                                                            *
 * Purpose: set timeout value for processing of items                         *
 *                                                                            *
 * Parameters: timeout - timeout in seconds, 0 - no timeout set               *
 *                                                                            *
 ******************************************************************************/
void	zbx_module_item_timeout(int timeout)
{
	item_timeout = timeout;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_list                                             *
 *                                                                            *
 * Purpose: returns list of item keys supported by the module                 *
 *                                                                            *
 * Return value: list of item keys                                            *
 *                                                                            *
 ******************************************************************************/
ZBX_METRIC	*zbx_module_item_list()
{
	return keys;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_nginx_load_config                                       *
 *                                                                            *
 * Purpose: load configuration from config file                               *
 *                                                                            *
 * Parameters: requirement - produce error if config file missing or not      *
 *                                                                            *
 ******************************************************************************/
int zbx_module_nginx_load_config(int requirement)
{
	int     ret = SYSINFO_RET_FAIL;

	char	*config = NULL;

	struct cfg_line cfg[] =
	{
		/* PARAMETER,               VAR,                                      TYPE,
			MANDATORY,  MIN,        MAX */
		{"NginxHost",    &CONFIG_NGINX_HOST,          TYPE_STRING,
			PARM_OPT,   0,          16},
		{"NginxPort",    &CONFIG_NGINX_PORT,          TYPE_INT,
			PARM_OPT,   0,          32767},
		{"NginxDomain",    &CONFIG_NGINX_DOMAIN,          TYPE_STRING,
			PARM_OPT,   0,          1024},
		{"NginxStatusUri",    &CONFIG_NGINX_STATUS_URI,          	  TYPE_STRING,
			PARM_OPT,   0,          1024},
		{NULL}
	};

	parse_cfg_file(ZBX_MODULE_NGINX_CONFIG_FILE, cfg, requirement, ZBX_CFG_STRICT);

	/* for dev
	zabbix_log(LOG_LEVEL_INFORMATION, "module [nginx], func [zbx_module_nginx_load_config], parse config NginxHost:[%s] NginxPort:[%d] NginxStatusUri:[%s]", CONFIG_NGINX_HOST, CONFIG_NGINX_PORT, CONFIG_NGINX_STATUS_URI);
	*/

	ret = SYSINFO_RET_OK;

}

int	zbx_module_nginx_status(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	char		*CONFIG_SOURCE_IP = NULL;	
	zbx_socket_t	s;
	char		*key;
	const char      *buf;
	int		ret = SYSINFO_RET_FAIL;
	int		find = 0;
	int		net_error = 0;
	char		r[MAX_STRING_LEN];

	zbx_uint64_t	ac = 0;    /* current number of active client connections */
	zbx_uint64_t	rd = 0;    /* current number of connections where nginx is reading the request header */
	zbx_uint64_t	wr = 0;    /* current number of connections where nginx is writing the response back to the client */
	zbx_uint64_t	wa = 0;    /* current number of idle client connections waiting for a request */

	if (request->nparam == 1)
	{
		key = get_rparam(request, 0);
	}
	else
	{
		/* set optional error message */
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid number of parameters"));
		return ret;
	}

	/* for dev
	zabbix_log(LOG_LEVEL_INFORMATION, "module [nginx], func [zbx_module_nginx_status], request key:[%s]", key);
	*/

	if (SUCCEED == zbx_tcp_connect(&s, CONFIG_SOURCE_IP, CONFIG_NGINX_HOST, (unsigned short)CONFIG_NGINX_PORT, 0, ZBX_TCP_SEC_UNENCRYPTED, NULL, NULL))
	{
		/* for dev
		zabbix_log(LOG_LEVEL_INFORMATION, "module [nginx], func [zbx_module_nginx_status], connect to [%s:%d] successful", CONFIG_NGINX_HOST, CONFIG_NGINX_PORT);
		*/

		if (NULL == CONFIG_NGINX_DOMAIN)
		{
			zbx_snprintf(r, sizeof(r),
				"GET /%s HTTP/1.1\r\n"
				"Host: %s\r\n"
				"Connection: close\r\n"
				"\r\n",
				CONFIG_NGINX_STATUS_URI, CONFIG_NGINX_HOST);
		}
		else
		{
			zbx_snprintf(r, sizeof(r),
				"GET /%s HTTP/1.1\r\n"
				"Host: %s\r\n"
				"Connection: close\r\n"
				"\r\n",
				CONFIG_NGINX_STATUS_URI, CONFIG_NGINX_DOMAIN);
		}

		/* for dev
		zabbix_log(LOG_LEVEL_INFORMATION, "module [nginx], func [zbx_module_nginx_status], r:[%s]", r);
		*/

		if (SUCCEED == zbx_tcp_send_raw(&s, r))
		{
			/* for dev
			zabbix_log(LOG_LEVEL_INFORMATION, "module [nginx], func [zbx_module_nginx_status], send request successful");
			*/

			while (NULL != (buf = zbx_tcp_recv_line(&s)))
			{
				/* for dev
				zabbix_log(LOG_LEVEL_INFORMATION, "module [nginx], func [zbx_module_nginx_status], got [%s]", buf);
				*/

				if (0 == strcmp(key, "ac") && 0 == strncmp(buf, "Active connections", 18))
				{
					if(1 == sscanf(buf, "%*s" "%*s" ZBX_FS_UI64, &ac))
					{
						/* for dev
						zabbix_log(LOG_LEVEL_INFORMATION, "module [nginx], func [zbx_module_nginx_status], key:[%s] value:[%d]", key, ac);
						*/
						find = 1;
						SET_UI64_RESULT(result, ac);
						ret = SYSINFO_RET_OK;
					}
				}
				else if (0 == strcmp(key, "rd") && 3 == sscanf(buf, "%*s" ZBX_FS_UI64 "%*s" ZBX_FS_UI64 "%*s" ZBX_FS_UI64, &rd, &wr, &wa))
				{
					/* for dev
					zabbix_log(LOG_LEVEL_INFORMATION, "module [nginx], func [zbx_module_nginx_status], key:[%s] value:[%d]", key, rd);
					*/
					find = 1;
					SET_UI64_RESULT(result, rd);
					ret = SYSINFO_RET_OK;
				}
				else if (0 == strcmp(key, "wr") && 3 == sscanf(buf, "%*s" ZBX_FS_UI64 "%*s" ZBX_FS_UI64 "%*s" ZBX_FS_UI64, &rd, &wr, &wa))
				{
					/* for dev
					zabbix_log(LOG_LEVEL_INFORMATION, "module [nginx], func [zbx_module_nginx_status], key:[%s] value:[%d]", key, wr);
					*/
					find = 1;
					SET_UI64_RESULT(result, wr);
					ret = SYSINFO_RET_OK;
				}
				else if (0 == strcmp(key, "wa") && 3 == sscanf(buf, "%*s" ZBX_FS_UI64 "%*s" ZBX_FS_UI64 "%*s" ZBX_FS_UI64, &rd, &wr, &wa))
				{
					/* for dev
					zabbix_log(LOG_LEVEL_INFORMATION, "module [nginx], func [zbx_module_nginx_status], key:[%s] value:[%d]", key, wa);
					*/
					find = 1;
					SET_UI64_RESULT(result, wa);
					ret = SYSINFO_RET_OK;
				}

			}
		}
		else
		{
			net_error = 1;
			zabbix_log(LOG_LEVEL_WARNING, "module [nginx], func [zbx_module_nginx_status], get nginx status error: [%s]", zbx_socket_strerror());
			SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Get nginx status error [%s]", zbx_socket_strerror()));
		}

		zbx_tcp_close(&s);
	}
	else
	{
		net_error = 1;
		zabbix_log(LOG_LEVEL_WARNING, "module [nginx], func [zbx_module_nginx_status], get nginx status error: [%s]", zbx_socket_strerror());
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Get nginx status error [%s]", zbx_socket_strerror()));
	}

	if (1 != find && 0 == net_error)
	{
		zabbix_log(LOG_LEVEL_WARNING, "module [nginx], func [zbx_module_nginx_status], not supported key: [%s]", key);
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Not supported key [%s]", key));
	}

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_init                                                  *
 *                                                                            *
 * Purpose: the function is called on agent startup                           *
 *          It should be used to call any initialization routines             *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - module initialization failed               *
 *                                                                            *
 * Comment: the module won't be loaded in case of ZBX_MODULE_FAIL             *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_init()
{
	zabbix_log(LOG_LEVEL_INFORMATION, "module [nginx], func [zbx_module_init], using configuration file: [%s]", ZBX_MODULE_NGINX_CONFIG_FILE);

	if (SYSINFO_RET_OK != zbx_module_nginx_load_config(ZBX_CFG_FILE_REQUIRED))
		return ZBX_MODULE_FAIL;

	return ZBX_MODULE_OK;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_uninit                                                *
 *                                                                            *
 * Purpose: the function is called on agent shutdown                          *
 *          It should be used to cleanup used resources if there are any      *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - function failed                            *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_uninit()
{
	return ZBX_MODULE_OK;
}
