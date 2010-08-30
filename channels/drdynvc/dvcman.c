/*
   Copyright (c) 2010 Vic Lee

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "drdynvc_types.h"
#include "dvcman.h"

#define MAX_PLUGINS 10

typedef struct _DVCMAN DVCMAN;
struct _DVCMAN
{
	IWTSVirtualChannelManager iface;

	IWTSPlugin * plugins[MAX_PLUGINS];
	int num_plugins;

	IWTSListener * listeners[MAX_PLUGINS];
	int num_listeners;
};

typedef struct _DVCMAN_LISTENER DVCMAN_LISTENER;
struct _DVCMAN_LISTENER
{
	IWTSListener iface;

	DVCMAN * dvcman;
	char * channel_name;
	uint32 flags;
	IWTSListenerCallback * listener_callback;
};

typedef struct _DVCMAN_ENTRY_POINTS DVCMAN_ENTRY_POINTS;
struct _DVCMAN_ENTRY_POINTS
{
	IDRDYNVC_ENTRY_POINTS iface;

	DVCMAN * dvcman;
};

static int
dvcman_get_configuration(IWTSListener * pListener,
	void ** ppPropertyBag)
{
	*ppPropertyBag = NULL;
	return 1;
}

static int
dvcman_create_listener(IWTSVirtualChannelManager * pChannelMgr,
	const char * pszChannelName,
	uint32 ulFlags,
	IWTSListenerCallback * pListenerCallback,
	IWTSListener ** ppListener)
{
	DVCMAN * dvcman = (DVCMAN *) pChannelMgr;
	DVCMAN_LISTENER * listener;

	if (dvcman->num_listeners < MAX_PLUGINS)
	{
		LLOGLN(10, ("dvcman_create_listener: %d.%s.", dvcman->num_listeners, pszChannelName));
		listener = (DVCMAN_LISTENER *) malloc(sizeof(DVCMAN_LISTENER));
		memset(listener, 0, sizeof(DVCMAN_LISTENER));
		listener->iface.GetConfiguration = dvcman_get_configuration;
		listener->dvcman = dvcman;
		listener->channel_name = strdup(pszChannelName);
		listener->flags = ulFlags;
		listener->listener_callback = pListenerCallback;

		if (ppListener)
			*ppListener = (IWTSListener *) listener;
		dvcman->listeners[dvcman->num_listeners++] = (IWTSListener *) listener;
		return 0;
	}
	else
	{
		LLOGLN(0, ("dvcman_create_listener: Maximum DVC listener number reached."));
		return 1;
	}
}

static int
dvcman_register_plugin(IDRDYNVC_ENTRY_POINTS * pEntryPoints,
	IWTSPlugin * pPlugin)
{
	DVCMAN * dvcman = ((DVCMAN_ENTRY_POINTS *)pEntryPoints)->dvcman;

	if (dvcman->num_plugins < MAX_PLUGINS)
	{
		LLOGLN(0, ("dvcman_register_plugin: %d", dvcman->num_plugins));
		dvcman->plugins[dvcman->num_plugins++] = pPlugin;
		return 0;
	}
	else
	{
		LLOGLN(0, ("dvcman_register_plugin: Maximum DVC plugin number reached."));
		return 1;
	}
}

IWTSVirtualChannelManager *
dvcman_new(void)
{
	DVCMAN * dvcman;

	dvcman = (DVCMAN *) malloc(sizeof(DVCMAN));
	memset(dvcman, 0, sizeof(DVCMAN));
	dvcman->iface.CreateListener = dvcman_create_listener;
	dvcman->num_plugins = 0;
	dvcman->num_listeners = 0;

	return (IWTSVirtualChannelManager *) dvcman;
}

int
dvcman_load_plugin(IWTSVirtualChannelManager * pChannelMgr, char* filename)
{
	DVCMAN_ENTRY_POINTS entryPoints;
	void* dl;
	char* fn;
	PDVC_PLUGIN_ENTRY pDVCPluginEntry = NULL;

	if (strchr(filename, '/'))
	{
		fn = strdup(filename);
	}
	else
	{
		fn = malloc(strlen(PLUGIN_PATH) + strlen(filename) + 10);
		sprintf(fn, PLUGIN_PATH "/%s.so", filename);
	}
	dl = dlopen(fn, RTLD_LOCAL | RTLD_LAZY);

	pDVCPluginEntry = (PDVC_PLUGIN_ENTRY)dlsym(dl, "DVCPluginEntry");

	if(pDVCPluginEntry != NULL)
	{
		entryPoints.iface.RegisterPlugin = dvcman_register_plugin;
		entryPoints.dvcman = (DVCMAN *) pChannelMgr;
		pDVCPluginEntry((IDRDYNVC_ENTRY_POINTS *) &entryPoints);
		LLOGLN(0, ("loaded DVC plugin: %s", fn));
	}
	free(fn);

	return 0;
}

void
dvcman_free(IWTSVirtualChannelManager * pChannelMgr)
{
	DVCMAN * dvcman = (DVCMAN *) pChannelMgr;
	int i;
	IWTSPlugin * pPlugin;
	DVCMAN_LISTENER * listener;

	for (i = 0; i < dvcman->num_plugins; i++)
	{
		pPlugin = dvcman->plugins[i];
		if (pPlugin->Terminated)
			pPlugin->Terminated(pPlugin);
	}
	for (i = 0; i < dvcman->num_listeners; i++)
	{
		listener = (DVCMAN_LISTENER *) dvcman->listeners[i];
		free(listener->channel_name);
		free(listener);
	}
	free(dvcman);
}

int
dvcman_initialize(IWTSVirtualChannelManager * pChannelMgr)
{
	DVCMAN * dvcman = (DVCMAN *) pChannelMgr;
	int i;
	IWTSPlugin * pPlugin;

	for (i = 0; i < dvcman->num_plugins; i++)
	{
		pPlugin = dvcman->plugins[i];
		if (pPlugin->Initialize)
			pPlugin->Initialize(pPlugin, pChannelMgr);
	}
	return 0;
}

