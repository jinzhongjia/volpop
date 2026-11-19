// SPDX-License-Identifier: GPL-3.0-or-later
#include "vp-convert.h"

static char *prop(pa_proplist *p, const char *key)
{
    const char *v = p ? pa_proplist_gets(p, key) : NULL;
    return v && *v ? g_strdup(v) : NULL;
}

static void common_device(VpDevice *d, pa_proplist *p, const pa_cvolume *v, const pa_channel_map *map, int mute,
                          guint32 card, gboolean decibel)
{
    d->bus = prop(p, PA_PROP_DEVICE_BUS);
    d->form_factor = prop(p, PA_PROP_DEVICE_FORM_FACTOR);
    d->volume = *v;
    d->map = *map;
    d->mute = !!mute;
    d->card = card;
    d->decibel = decibel;
}

#define ADD_PORTS(d, info)                                                                                  \
    do {                                                                                                    \
        for (guint32 k = 0; k < (info)->n_ports; k++) {                                                     \
            g_ptr_array_add((d)->ports, vp_port_new((info)->ports[k]->name, (info)->ports[k]->description,  \
                                                    (info)->ports[k]->priority,                             \
                                                    (info)->ports[k]->available != PA_PORT_AVAILABLE_NO));  \
        }                                                                                                   \
        if ((info)->active_port) (d)->active_port = g_strdup((info)->active_port->name);                   \
    } while (0)

VpDevice *vp_device_from_sink(const pa_sink_info *i)
{
    VpDevice *d = vp_device_new(VP_OUTPUT, i->index, i->name, i->description);
    common_device(d, i->proplist, &i->volume, &i->channel_map, i->mute, i->card, !!(i->flags & PA_SINK_DECIBEL_VOLUME));
    d->monitor_name = g_strdup(i->monitor_source_name);
    ADD_PORTS(d, i);
    return d;
}

VpDevice *vp_device_from_source(const pa_source_info *i)
{
    VpDevice *d = vp_device_new(VP_INPUT, i->index, i->name, i->description);
    common_device(d, i->proplist, &i->volume, &i->channel_map, i->mute, i->card,
                  !!(i->flags & PA_SOURCE_DECIBEL_VOLUME));
    d->is_monitor = i->monitor_of_sink != PA_INVALID_INDEX;
    ADD_PORTS(d, i);
    return d;
}

static void common_stream(VpStream *s, pa_proplist *p, const char *name, const pa_cvolume *v,
                          const pa_channel_map *map, int mute, int corked, int has_volume, int writable)
{
    s->app = prop(p, PA_PROP_APPLICATION_NAME);
    s->media = prop(p, PA_PROP_MEDIA_NAME);
    if (!s->media && name && *name) s->media = g_strdup(name);
    s->icon = prop(p, PA_PROP_APPLICATION_ICON_NAME);
    if (!s->icon) s->icon = prop(p, PA_PROP_MEDIA_ICON_NAME);
    s->role = prop(p, PA_PROP_MEDIA_ROLE);
    s->app_id = prop(p, PA_PROP_APPLICATION_ID);
    s->volume = *v;
    s->map = *map;
    s->mute = !!mute;
    s->corked = !!corked;
    s->has_volume = has_volume && writable;
}

VpStream *vp_stream_from_sink_input(const pa_sink_input_info *i)
{
    VpStream *s = vp_stream_new(VP_OUTPUT, i->index, i->sink);
    common_stream(s, i->proplist, i->name, &i->volume, &i->channel_map, i->mute, i->corked, i->has_volume,
                  i->volume_writable);
    return s;
}

VpStream *vp_stream_from_source_output(const pa_source_output_info *i)
{
    VpStream *s = vp_stream_new(VP_INPUT, i->index, i->source);
    common_stream(s, i->proplist, i->name, &i->volume, &i->channel_map, i->mute, i->corked, i->has_volume,
                  i->volume_writable);
    return s;
}

VpCard *vp_card_from_info(const pa_card_info *i)
{
    char *desc = prop(i->proplist, PA_PROP_DEVICE_DESCRIPTION);
    VpCard *c = vp_card_new(i->index, i->name, desc);
    g_free(desc);
    for (guint32 k = 0; k < i->n_profiles; k++) {
        pa_card_profile_info2 *p = i->profiles2[k];
        VpProfile *vp = vp_profile_new(p->name, p->description, p->priority, p->available != 0);
        vp->n_sinks = p->n_sinks;
        vp->n_sources = p->n_sources;
        g_ptr_array_add(c->profiles, vp);
    }
    if (i->active_profile2) c->active_profile = g_strdup(i->active_profile2->name);
    return c;
}
