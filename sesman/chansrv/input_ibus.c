/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2009-2013
 * Copyright (C) Laxmikant Rashinkar 2009-2012
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <ibus.h>
#include "input.h"
#include "thread_calls.h"

static IBusBus *bus;
static IBusEngine *g_engine;
/* This is the engine name enabled before unicode engine enabled */
static const gchar *last_input_name;
static int id = 0;

static int
xrdp_input_enable(void)
{
    IBusEngineDesc *desc;
    const gchar *name;

    if (last_input_name)
    {
        /* already enabled */
        return 0;
    }

    if (!bus)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_ibus_init: input method switched failed, ibus not connected");
        return 1;
    }

    desc = ibus_bus_get_global_engine(bus);
    name = ibus_engine_desc_get_name (desc);
    if (!g_ascii_strcasecmp(name, "XrdpIme"))
    {
        return 0;
    }

    /* remember user's input method, will switch back when disconnect */
    last_input_name = name;

    if (!ibus_bus_set_global_engine(bus, "XrdpIme"))
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_input_enable: input method enable failed");
        return 1;
    }

    LOG(LOG_LEVEL_INFO, "xrdp_ibus_init: input method switched sucessfully, old input name: %s", last_input_name);

    return 0;
}

int
xrdp_input_send_unicode(char32_t unicode)
{
    LOG(LOG_LEVEL_DEBUG, "xrdp_input_send_unicode: received unicode input %i", unicode);

    if (xrdp_input_enable())
    {
        return 1;
    }

    gunichar chr = unicode;
    ibus_engine_commit_text(g_engine, ibus_text_new_from_unichar(chr));

    return 0;
}

static void
xrdp_input_ibus_engine_enable(IBusEngine *engine)
{
    LOG(LOG_LEVEL_INFO, "xrdp_ibus_engine_enable: IM enabled");
    g_engine = engine;
}

static void
xrdp_input_ibus_engine_disable(IBusEngine *engine)
{
    LOG(LOG_LEVEL_INFO, "xrdp_ibus_engine_disable: IM disabled");
}

static void
xrdp_input_ibus_disconnect(IBusEngine *engine)
{
    LOG(LOG_LEVEL_INFO, "xrdp_ibus_engine_disable: IM disabled");
    g_object_unref(g_engine);
    g_object_unref(bus);
}

static gboolean
engine_process_key_event_cb(IBusEngine *engine,
                            guint keyval,
                            guint keycode,
                            guint state)
{
    /* Pass the keyboard event to system */
    return FALSE;
}

static IBusEngine *
xrdp_input_ibus_create_engine(IBusFactory *factory,
                              gchar *engine_name,
                              gpointer user_data)
{
    IBusEngine *engine;
    gchar *path = g_strdup_printf("/org/freedesktop/IBus/Engine/%i", 1);
    engine = ibus_engine_new(engine_name,
                             path,
                             ibus_bus_get_connection(bus));

    LOG(LOG_LEVEL_DEBUG, "xrdp_input_ibus_create_engine: Creating IM Engine with name:%s and id:%d\n", engine_name, ++id);

    g_signal_connect(engine, "process-key-event", G_CALLBACK(engine_process_key_event_cb), NULL);
    g_signal_connect(engine, "enable", G_CALLBACK(xrdp_input_ibus_engine_enable), NULL);
    g_signal_connect(engine, "disable", G_CALLBACK(xrdp_input_ibus_engine_disable), NULL);

    return engine;
}

/*****************************************************************************/
static THREAD_RV THREAD_CC
xrdp_input_main_loop(void *in_val)
{
    IBusFactory *factory;
    IBusComponent *component;
    IBusEngineDesc *desc;
    THREAD_RV rv = 0;

    LOG(LOG_LEVEL_DEBUG, "xrdp_input_main_loop: Entering ibus loop");

    g_signal_connect(bus, "disconnected", G_CALLBACK(xrdp_input_ibus_disconnect), NULL);

    factory = ibus_factory_new(ibus_bus_get_connection(bus));
    g_object_ref_sink(factory);
    g_signal_connect(factory, "create-engine", G_CALLBACK(xrdp_input_ibus_create_engine), NULL);

    ibus_factory_add_engine(factory, "XrdpIme", IBUS_TYPE_ENGINE);

    component = ibus_component_new("org.freedesktop.IBus.XrdpIme", /* name */
                                   "Xrdp input method",            /* description */
                                   "1.1",                          /* version */
                                   "MIT",                          /* license */
                                   "seflerZ",                      /* author */
                                   "default",                      /* homepage */
                                   "default",                      /* cmd */
                                   "xrdpime");                     /* text domain */

    desc = ibus_engine_desc_new("XrdpIme",
                                "unicode input method for xrdp",
                                "unicode input method for xrdp",
                                "unicode",
                                "MIT",
                                "seflerZ",
                                "default",  /* icon */
                                "default"); /* layout */

    ibus_component_add_engine(component, desc);
    ibus_bus_register_component(bus, component);

    ibus_main();

    g_object_unref(desc);
    g_object_unref(component);
    g_object_unref(factory);

    return rv;
}

int
xrdp_input_unicode_destroy(void)
{
    LOG(LOG_LEVEL_DEBUG, "xrdp_input_unicode_destory: ibus input is under destory");
    if (last_input_name)
    {
        LOG(LOG_LEVEL_INFO, "xrdp_input_unicode_destory: ibus engine rolling back to origin: %s", last_input_name);
        ibus_bus_set_global_engine(bus, last_input_name);
    }

    g_object_unref(g_engine);
    g_object_unref(bus);

    last_input_name = NULL;
    bus = NULL;
    g_engine = NULL;

    return 0;
}

int
xrdp_input_unicode_init(void)
{
    if (bus)
    {
        /* Already initialized, just re-enable it */
        xrdp_input_enable();
        return 0;
    }

    /* Wait because the ibus daemon may not be ready on first login */
    const char *addr = ibus_get_address();
    unsigned int cnt = 0;
    while (!addr && cnt < 10)
    {
        usleep(500 * 1000); // half a second
        addr = ibus_get_address();
        ++cnt;
    }
    if (!addr)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_ibus_init: Timed out waiting for iBus daemon");
        return 1;
    }

    LOG(LOG_LEVEL_INFO, "xrdp_ibus_init: Initializing the iBus engine");
    ibus_init();
    bus = ibus_bus_new();
    g_object_ref_sink(bus);

    if (!ibus_bus_is_connected(bus))
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_ibus_init: Connect to iBus failed");
        return 1;
    }

    LOG(LOG_LEVEL_INFO, "xrdp_ibus_init: iBus connected");

    tc_thread_create(xrdp_input_main_loop, NULL);

    if (!ibus_bus_get_global_engine(bus))
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_ibus_init: failed to get origin global engine");
        return 1;
    }

    return 0;
}
