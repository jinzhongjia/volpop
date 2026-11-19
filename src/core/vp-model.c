// SPDX-License-Identifier: GPL-3.0-or-later
#include "vp-model.h"

#include <string.h>

/* ───────────── 构造 / 释放 ───────────── */

VpPort *vp_port_new(const char *name, const char *description, guint32 priority, gboolean available)
{
    VpPort *p = g_new0(VpPort, 1);
    p->name = g_strdup(name);
    p->description = g_strdup(description ? description : name);
    p->priority = priority;
    p->available = available;
    return p;
}

void vp_port_free(VpPort *p)
{
    if (!p) return;
    g_free(p->name);
    g_free(p->description);
    g_free(p);
}

VpDevice *vp_device_new(VpDir dir, guint32 index, const char *name, const char *description)
{
    VpDevice *d = g_new0(VpDevice, 1);
    d->dir = dir;
    d->index = index;
    d->name = g_strdup(name);
    d->description = g_strdup(description ? description : name);
    d->card = PA_INVALID_INDEX;
    d->ports = g_ptr_array_new_with_free_func((GDestroyNotify)vp_port_free);
    pa_cvolume_init(&d->volume);
    pa_channel_map_init(&d->map);
    return d;
}

void vp_device_free(VpDevice *d)
{
    if (!d) return;
    g_free(d->name);
    g_free(d->description);
    g_free(d->bus);
    g_free(d->form_factor);
    g_free(d->monitor_name);
    g_free(d->active_port);
    g_ptr_array_unref(d->ports);
    g_free(d);
}

VpStream *vp_stream_new(VpDir dir, guint32 index, guint32 device)
{
    VpStream *s = g_new0(VpStream, 1);
    s->dir = dir;
    s->index = index;
    s->device = device;
    s->has_volume = TRUE;
    pa_cvolume_init(&s->volume);
    pa_channel_map_init(&s->map);
    return s;
}

void vp_stream_free(VpStream *s)
{
    if (!s) return;
    g_free(s->app);
    g_free(s->media);
    g_free(s->icon);
    g_free(s->role);
    g_free(s->app_id);
    g_free(s);
}

VpProfile *vp_profile_new(const char *name, const char *description, guint32 priority, gboolean available)
{
    VpProfile *p = g_new0(VpProfile, 1);
    p->name = g_strdup(name);
    p->description = g_strdup(description ? description : name);
    p->priority = priority;
    p->available = available;
    return p;
}

void vp_profile_free(VpProfile *p)
{
    if (!p) return;
    g_free(p->name);
    g_free(p->description);
    g_free(p);
}

VpCard *vp_card_new(guint32 index, const char *name, const char *description)
{
    VpCard *c = g_new0(VpCard, 1);
    c->index = index;
    c->name = g_strdup(name);
    c->description = g_strdup(description ? description : name);
    c->profiles = g_ptr_array_new_with_free_func((GDestroyNotify)vp_profile_free);
    return c;
}

void vp_card_free(VpCard *c)
{
    if (!c) return;
    g_free(c->name);
    g_free(c->description);
    g_free(c->active_profile);
    g_ptr_array_unref(c->profiles);
    g_free(c);
}

VpModel *vp_model_new(const char *self_app_id)
{
    VpModel *m = g_new0(VpModel, 1);
    for (int i = 0; i < 2; i++) {
        m->devices[i] = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)vp_device_free);
        m->streams[i] = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)vp_stream_free);
    }
    m->cards = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)vp_card_free);
    m->self_app_id = g_strdup(self_app_id);
    return m;
}

void vp_model_clear(VpModel *m)
{
    for (int i = 0; i < 2; i++) {
        g_hash_table_remove_all(m->devices[i]);
        g_hash_table_remove_all(m->streams[i]);
        g_clear_pointer(&m->default_name[i], g_free);
    }
    g_hash_table_remove_all(m->cards);
}

void vp_model_free(VpModel *m)
{
    if (!m) return;
    vp_model_clear(m);
    for (int i = 0; i < 2; i++) {
        g_hash_table_unref(m->devices[i]);
        g_hash_table_unref(m->streams[i]);
    }
    g_hash_table_unref(m->cards);
    g_free(m->self_app_id);
    g_free(m);
}

/* ───────────── 更新 ───────────── */

#define KEY(i) GUINT_TO_POINTER((guint)(i) + 1) /* 序号 0 也是合法的，避免与 NULL 冲突 */

void vp_model_put_device(VpModel *m, VpDevice *d) { g_hash_table_replace(m->devices[d->dir], KEY(d->index), d); }
void vp_model_put_stream(VpModel *m, VpStream *s) { g_hash_table_replace(m->streams[s->dir], KEY(s->index), s); }
void vp_model_put_card(VpModel *m, VpCard *c) { g_hash_table_replace(m->cards, KEY(c->index), c); }

gboolean vp_model_remove_device(VpModel *m, VpDir dir, guint32 i) { return g_hash_table_remove(m->devices[dir], KEY(i)); }
gboolean vp_model_remove_stream(VpModel *m, VpDir dir, guint32 i) { return g_hash_table_remove(m->streams[dir], KEY(i)); }
gboolean vp_model_remove_card(VpModel *m, guint32 i) { return g_hash_table_remove(m->cards, KEY(i)); }

void vp_model_set_default(VpModel *m, VpDir dir, const char *name)
{
    g_free(m->default_name[dir]);
    m->default_name[dir] = g_strdup(name);
}

/* ───────────── 查询 ───────────── */

VpDevice *vp_model_device(VpModel *m, VpDir dir, guint32 i) { return g_hash_table_lookup(m->devices[dir], KEY(i)); }
VpStream *vp_model_stream(VpModel *m, VpDir dir, guint32 i) { return g_hash_table_lookup(m->streams[dir], KEY(i)); }
VpCard *vp_model_card(VpModel *m, guint32 i) { return g_hash_table_lookup(m->cards, KEY(i)); }

gboolean vp_model_is_default(VpModel *m, const VpDevice *d)
{
    return d && m->default_name[d->dir] && !g_strcmp0(d->name, m->default_name[d->dir]);
}

VpDevice *vp_model_default_device(VpModel *m, VpDir dir)
{
    GHashTableIter it;
    gpointer v;
    g_hash_table_iter_init(&it, m->devices[dir]);
    while (g_hash_table_iter_next(&it, NULL, &v))
        if (vp_model_is_default(m, v)) return v;
    return NULL;
}

static VpModel *sort_model; /* 排序回调里需要访问默认设备；只在单线程同步排序期间使用 */

static gint cmp_device(gconstpointer a, gconstpointer b)
{
    const VpDevice *x = *(VpDevice *const *)a, *y = *(VpDevice *const *)b;
    gboolean dx = vp_model_is_default(sort_model, x), dy = vp_model_is_default(sort_model, y);
    if (dx != dy) return dx ? -1 : 1;
    int r = g_utf8_collate(x->description, y->description);
    return r ? r : (x->index < y->index ? -1 : x->index > y->index);
}

GPtrArray *vp_model_devices(VpModel *m, VpDir dir, gboolean include_monitors)
{
    GPtrArray *out = g_ptr_array_new();
    GHashTableIter it;
    gpointer v;
    g_hash_table_iter_init(&it, m->devices[dir]);
    while (g_hash_table_iter_next(&it, NULL, &v)) {
        VpDevice *d = v;
        if (d->is_monitor && !include_monitors) continue;
        g_ptr_array_add(out, d);
    }
    sort_model = m;
    g_ptr_array_sort(out, cmp_device);
    sort_model = NULL;
    return out;
}

static gint cmp_stream(gconstpointer a, gconstpointer b)
{
    const VpStream *x = *(VpStream *const *)a, *y = *(VpStream *const *)b;
    char *tx = vp_stream_title(x), *ty = vp_stream_title(y);
    int r = g_utf8_collate(tx, ty);
    g_free(tx);
    g_free(ty);
    return r ? r : (x->index < y->index ? -1 : x->index > y->index);
}

/* 不显示的流：自己的电平表、pavucontrol 等的峰值检测流 */
static gboolean stream_hidden(const VpModel *m, const VpStream *s)
{
    static const char *const peak_apps[] = { "org.PulseAudio.pavucontrol", "org.gnome.VolumeControl", "org.kde.kmixd" };
    if (s->app_id && m->self_app_id && !strcmp(s->app_id, m->self_app_id)) return TRUE;
    if (s->dir == VP_INPUT && s->app_id)
        for (guint i = 0; i < G_N_ELEMENTS(peak_apps); i++)
            if (!strcmp(s->app_id, peak_apps[i])) return TRUE;
    return FALSE;
}

GPtrArray *vp_model_streams(VpModel *m, VpDir dir)
{
    GPtrArray *out = g_ptr_array_new();
    GHashTableIter it;
    gpointer v;
    g_hash_table_iter_init(&it, m->streams[dir]);
    while (g_hash_table_iter_next(&it, NULL, &v))
        if (!stream_hidden(m, v)) g_ptr_array_add(out, v);
    g_ptr_array_sort(out, cmp_stream);
    return out;
}

static gint cmp_card(gconstpointer a, gconstpointer b)
{
    const VpCard *x = *(VpCard *const *)a, *y = *(VpCard *const *)b;
    return g_utf8_collate(x->description, y->description);
}

GPtrArray *vp_model_cards(VpModel *m)
{
    GPtrArray *out = g_ptr_array_new();
    GHashTableIter it;
    gpointer v;
    g_hash_table_iter_init(&it, m->cards);
    while (g_hash_table_iter_next(&it, NULL, &v)) g_ptr_array_add(out, v);
    g_ptr_array_sort(out, cmp_card);
    return out;
}

/* ───────────── 显示规则 ───────────── */

static gboolean contains(const char *hay, const char *needle)
{
    if (!hay) return FALSE;
    char *l = g_ascii_strdown(hay, -1);
    gboolean r = strstr(l, needle) != NULL;
    g_free(l);
    return r;
}

VpDeviceKind vp_device_kind(const VpDevice *d)
{
    if (!g_strcmp0(d->bus, "bluetooth")) return VP_KIND_BLUETOOTH;
    const char *ff = d->form_factor;
    if (!g_strcmp0(ff, "headset")) return VP_KIND_HEADSET;
    if (!g_strcmp0(ff, "headphone")) return VP_KIND_HEADPHONES;
    if (contains(d->active_port, "headset")) return VP_KIND_HEADSET;
    if (contains(d->active_port, "headphone")) return VP_KIND_HEADPHONES;
    if (contains(d->active_port, "hdmi") || contains(d->name, "hdmi") || contains(d->active_port, "displayport"))
        return VP_KIND_HDMI;
    if (!g_strcmp0(d->bus, "usb")) return d->dir == VP_INPUT ? VP_KIND_MICROPHONE : VP_KIND_USB;
    return d->dir == VP_INPUT ? VP_KIND_MICROPHONE : VP_KIND_SPEAKER;
}

const char *vp_device_active_port_label(const VpDevice *d)
{
    for (guint i = 0; i < d->ports->len; i++) {
        VpPort *p = d->ports->pdata[i];
        if (!g_strcmp0(p->name, d->active_port)) return p->description;
    }
    return NULL;
}

static gint cmp_port(gconstpointer a, gconstpointer b)
{
    const VpPort *x = *(VpPort *const *)a, *y = *(VpPort *const *)b;
    if (x->available != y->available) return x->available ? -1 : 1;
    return x->priority > y->priority ? -1 : x->priority < y->priority;
}

GPtrArray *vp_device_ports_sorted(const VpDevice *d)
{
    GPtrArray *out = g_ptr_array_new();
    for (guint i = 0; i < d->ports->len; i++) g_ptr_array_add(out, d->ports->pdata[i]);
    g_ptr_array_sort(out, cmp_port);
    return out;
}

static gint cmp_profile(gconstpointer a, gconstpointer b)
{
    const VpProfile *x = *(VpProfile *const *)a, *y = *(VpProfile *const *)b;
    gboolean ox = !strcmp(x->name, "off"), oy = !strcmp(y->name, "off");
    if (ox != oy) return ox ? 1 : -1;
    if (x->available != y->available) return x->available ? -1 : 1;
    return x->priority > y->priority ? -1 : x->priority < y->priority;
}

GPtrArray *vp_card_profiles_sorted(const VpCard *c)
{
    GPtrArray *out = g_ptr_array_new();
    for (guint i = 0; i < c->profiles->len; i++) g_ptr_array_add(out, c->profiles->pdata[i]);
    g_ptr_array_sort(out, cmp_profile);
    return out;
}

char *vp_stream_title(const VpStream *s)
{
    if (s->app && *s->app) return g_strdup(s->app);
    if (s->media && *s->media) return g_strdup(s->media);
    return g_strdup("未知应用");
}

char *vp_stream_subtitle(const VpStream *s)
{
    if (!s->media || !*s->media || !s->app || !*s->app || !strcmp(s->media, s->app)) return g_strdup("");
    return g_strdup(s->media);
}
