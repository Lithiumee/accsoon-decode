/*
Plugin Name
Copyright (C) <Year> <Developer> <Email Address>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>

#include "plugin-macros.generated.h"
#include "gst.h"

static struct obs_source_info accsoon_source_info;
obs_source_t *g_source;



OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	obs_register_source(&accsoon_source_info);
	blog(LOG_INFO, "accsoon plugin loaded successfully (version %s %s)",
	     PLUGIN_VERSION,"022");
	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "accsoon plugin unloaded");
}


static const char *accsoon_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Accsoon Video Source";
}

static void *accsoon_source_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	g_source = source;
	obs_source_set_async_unbuffered(g_source, true);
	accsoon_gst_init();
	//Create Thread 
	return g_source;
}

static void accsoon_source_destroy(void *data)
{
	accsoon_gst_deinit();
	UNUSED_PARAMETER(data);
}

static struct obs_source_info accsoon_source_info = {
	.id = "accsoon_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO,
	.get_name = accsoon_source_get_name,
	.create = accsoon_source_create,
	.destroy = accsoon_source_destroy
};