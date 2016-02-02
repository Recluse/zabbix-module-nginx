Description
===========

This directory contains a sample module, which extends functionality of Zabbix Agent. 

Status
======

This module is testing.

Installation
============

```bash

	$ git clone https://github.com/cnshawncao/zabbix-module-nginx.git
	$ cp -r zabbix-module-nginx zabbix-x.x.x/src/modules/nginx	# zabbix-x.x.x is zabbix version
```

1. run 'make' to build it. It should produce nginx.so.

1. copy nginx.so to the module directory, like LoadModulePath=/etc/zabbix/modules

1. change config file add line : LoadModule=nginx.so

1. cp zbx_module_nginx.conf to /etc/zabbix, modify it (example)

	NginxHost = 127.0.0.1

	NginxPort = 80

	NginxDomain = www.a.com

	NginxStatusUri = nginx_status

	*status check url: http://127.0.0.1/nginx_status -H 'host: www.a.com'*

1. restart zabbix_agent daemon

1. import the zabbix template *zbx_template_nginx_active.xml*

1. link template *zbx_template_nginx_active.xml*

Synopsis
========
    
**key:** *nginx.status[ac|rd|wr|wa]*

*ac		-- current number of active client connections*

*rd		-- current number of connections where nginx is reading the request header*

*wr		-- current number of connections where nginx is writing the response back to the client*

*wa		-- current number of idle client connections waiting for a request*

Note
===

Note: this module only support the keys [ac|rd|wr|wa], other key will return: ZBX_NOTSUPPORTED: Not supported key
