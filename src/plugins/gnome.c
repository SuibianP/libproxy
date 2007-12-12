/*******************************************************************************
 * libproxy - A library for proxy configuration
 * Copyright (C) 2006 Nathaniel McCallum <nathaniel@natemccallum.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <misc.h>
#include <proxy_factory.h>

#include <gconf/gconf-client.h>

pxConfig *
gconf_config_cb(pxProxyFactory *self)
{
	// Get the GConf client
	GConfClient *client = px_proxy_factory_misc_get(self, "gnome");
	if (!client)
	{
		// Create a new instance if not found
		client = gconf_client_get_default();
		if (!client) return NULL;
		px_proxy_factory_misc_set(self, "gnome", client);
	}
	g_object_ref(client);
	
	// Get the mode
	char *mode = gconf_client_get_string(client, "/system/proxy/mode", NULL);
	if (!mode) { g_object_unref(client); return NULL; }

	char *url = NULL, *ignore = NULL;
	
	// Mode is direct://
	if (!strcmp(mode, "none"))
		url = px_strdup("direct://");

	// Mode is wpad:// or pac+http://...
	else if (!strcmp(mode, "auto"))
	{
		char *tmp = gconf_client_get_string(client, "/system/proxy/autoconfig_url", NULL);
		if (px_url_is_valid(tmp))
			url = g_strdup_printf("pac+%s", tmp); 
		else
			url = px_strdup("wpad://");
		px_free(tmp);
	}
	
	// Mode is http://... or socks://...
	else if (!strcmp(mode, "manual"))
	{
		char *type = px_strdup("http");
		char *host = gconf_client_get_string(client, "/system/http_proxy/host", NULL);
		int   port = gconf_client_get_int   (client, "/system/http_proxy/port", NULL);
		if (port < 0 || port > 65535) port = 0;
		
		// If http proxy is not set, try socks
		if (!host || !strcmp(host, "") || !port)
		{
			if (type) px_free(type);
			if (host) px_free(host);
			
			type = px_strdup("socks");
			host = gconf_client_get_string(client, "/system/proxy/socks_host", NULL);
			port = gconf_client_get_int   (client, "/system/proxy/socks_port", NULL);
			if (port < 0 || port > 65535) port = 0;
		}
		
		// If host and port were found, build config url
		if (host && strcmp(host, "") && port)
			url = g_strdup_printf("%s://%s:%d", type, host, port);
		
		if (type) px_free(type);
		if (host) px_free(host);		
	}
	px_free(mode);
	
	if (url)
	{
		GSList *ignores = gconf_client_get_list(client, "/system/http_proxy/ignore_hosts", 
												GCONF_VALUE_STRING, NULL);
		if (ignores)
		{
			GSList *start = ignores;
			for ( ; ignores ; ignores = g_slist_next(ignores))
			{
				if (ignore)
				{
					char *tmp = g_strdup_printf("%s,%s", ignore, ignores->data);
					g_free(ignore);
					ignore = tmp;
				}
				else
					ignore = g_strdup(ignores->data);
				g_free(ignores->data);
			}
			g_slist_free(start);
		}
	}
	
	g_object_unref(client);
	return px_config_create(url, ignore);
}

void
gconf_on_get_proxy(pxProxyFactory *self)
{
	// If we are running in GNOME, then make sure this plugin is registered.
	// Otherwise, make sure this plugin is NOT registered.
	if (!system("xlsclients 2>/dev/null | grep -q -E '[\t ]gnome-(session|panel)$'"))
		px_proxy_factory_config_add(self, "gnome", PX_CONFIG_CATEGORY_SESSION, 
									(pxProxyFactoryPtrCallback) gconf_config_cb);
	else
		px_proxy_factory_config_del(self, "gnome");
	
	return;
}

bool
on_proxy_factory_instantiate(pxProxyFactory *self)
{
	// Note that we instantiate like this because SESSION config plugins
	// are only suppossed to remain registered while the application
	// is actually IN that session.  So for instance, if the app
	// was run in GNU screen and is taken out of the GNOME sesion
	// it was started in, we should handle that gracefully.
	g_type_init();
	px_proxy_factory_on_get_proxy_add(self, gconf_on_get_proxy);
	return true;
}

void
on_proxy_factory_destantiate(pxProxyFactory *self)
{
	px_proxy_factory_on_get_proxy_del(self, gconf_on_get_proxy);
	px_proxy_factory_config_del(self, "gnome");
	
	// Close the GConf connection, if present
	if (px_proxy_factory_misc_get(self, "gnome"))
	{
		g_object_unref(px_proxy_factory_misc_get(self, "gnome"));
		px_proxy_factory_misc_set(self, "gnome", NULL);
	}
}
