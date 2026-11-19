// SPDX-License-Identifier: GPL-3.0-or-later
#include "vp-pulse.h"
#include "vp-convert.h"

#include <pulse/glib-mainloop.h>
#include <pulse/pulseaudio.h>

static struct {
    pa_glib_mainloop *loop;
    pa_context *ctx;
    VpModel *model;
    char *app_id;
    VpChangedFn changed;
    gpointer data;
    guint reconnect_id;
    GHashTable *pending; /* key → Pending*（音量节流） */
} P;

static void changed(void)
{
    if (P.changed) P.changed(P.data);
}

static void unref_op(pa_operation *o)
{
    if (o) pa_operation_unref(o);
}

/* ───────────── 读取 ───────────── */

static void on_sink(pa_context *c, const pa_sink_info *i, int eol, void *u)
{
    if (eol || !i) return;
    vp_model_put_device(P.model, vp_device_from_sink(i));
    changed();
}

static void on_source(pa_context *c, const pa_source_info *i, int eol, void *u)
{
    if (eol || !i) return;
    vp_model_put_device(P.model, vp_device_from_source(i));
    changed();
}

static void on_sink_input(pa_context *c, const pa_sink_input_info *i, int eol, void *u)
{
    if (eol || !i) return;
    vp_model_put_stream(P.model, vp_stream_from_sink_input(i));
    changed();
}

static void on_source_output(pa_context *c, const pa_source_output_info *i, int eol, void *u)
{
    if (eol || !i) return;
    vp_model_put_stream(P.model, vp_stream_from_source_output(i));
    changed();
}

static void on_card(pa_context *c, const pa_card_info *i, int eol, void *u)
{
    if (eol || !i) return;
    vp_model_put_card(P.model, vp_card_from_info(i));
    changed();
}

static void on_server(pa_context *c, const pa_server_info *i, void *u)
{
    if (!i) return;
    vp_model_set_default(P.model, VP_OUTPUT, i->default_sink_name);
    vp_model_set_default(P.model, VP_INPUT, i->default_source_name);
    changed();
}

static void on_event(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *u)
{
    pa_subscription_event_type_t fac = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
    gboolean removed = (t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE;
    switch (fac) {
    case PA_SUBSCRIPTION_EVENT_SINK:
        if (removed) { vp_model_remove_device(P.model, VP_OUTPUT, idx); changed(); }
        else unref_op(pa_context_get_sink_info_by_index(c, idx, on_sink, NULL));
        break;
    case PA_SUBSCRIPTION_EVENT_SOURCE:
        if (removed) { vp_model_remove_device(P.model, VP_INPUT, idx); changed(); }
        else unref_op(pa_context_get_source_info_by_index(c, idx, on_source, NULL));
        break;
    case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
        if (removed) { vp_model_remove_stream(P.model, VP_OUTPUT, idx); changed(); }
        else unref_op(pa_context_get_sink_input_info(c, idx, on_sink_input, NULL));
        break;
    case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
        if (removed) { vp_model_remove_stream(P.model, VP_INPUT, idx); changed(); }
        else unref_op(pa_context_get_source_output_info(c, idx, on_source_output, NULL));
        break;
    case PA_SUBSCRIPTION_EVENT_CARD:
        if (removed) { vp_model_remove_card(P.model, idx); changed(); }
        else unref_op(pa_context_get_card_info_by_index(c, idx, on_card, NULL));
        break;
    case PA_SUBSCRIPTION_EVENT_SERVER:
        unref_op(pa_context_get_server_info(c, on_server, NULL));
        break;
    default:
        break;
    }
}

static void fetch_all(pa_context *c)
{
    unref_op(pa_context_get_server_info(c, on_server, NULL));
    unref_op(pa_context_get_sink_info_list(c, on_sink, NULL));
    unref_op(pa_context_get_source_info_list(c, on_source, NULL));
    unref_op(pa_context_get_sink_input_info_list(c, on_sink_input, NULL));
    unref_op(pa_context_get_source_output_info_list(c, on_source_output, NULL));
    unref_op(pa_context_get_card_info_list(c, on_card, NULL));
}

/* ───────────── 连接 ───────────── */

static void connect_ctx(void);

static gboolean reconnect_cb(gpointer _)
{
    P.reconnect_id = 0;
    connect_ctx();
    return G_SOURCE_REMOVE;
}

static void drop_ctx(void)
{
    if (!P.ctx) return;
    pa_context_set_state_callback(P.ctx, NULL, NULL);
    pa_context_set_subscribe_callback(P.ctx, NULL, NULL);
    pa_context_disconnect(P.ctx);
    pa_context_unref(P.ctx);
    P.ctx = NULL;
}

static void on_state(pa_context *c, void *u)
{
    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_READY:
        pa_context_set_subscribe_callback(c, on_event, NULL);
        unref_op(pa_context_subscribe(c,
                                      PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE |
                                          PA_SUBSCRIPTION_MASK_SINK_INPUT | PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT |
                                          PA_SUBSCRIPTION_MASK_CARD | PA_SUBSCRIPTION_MASK_SERVER,
                                      NULL, NULL));
        fetch_all(c);
        changed();
        break;
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
        g_message("sound server connection lost: %s", pa_strerror(pa_context_errno(c)));
        drop_ctx();
        g_hash_table_remove_all(P.pending);
        vp_model_clear(P.model);
        changed();
        if (!P.reconnect_id) P.reconnect_id = g_timeout_add_seconds(1, reconnect_cb, NULL);
        break;
    default:
        break;
    }
}

static void connect_ctx(void)
{
    pa_proplist *pl = pa_proplist_new();
    pa_proplist_sets(pl, PA_PROP_APPLICATION_NAME, "volpop");
    pa_proplist_sets(pl, PA_PROP_APPLICATION_ID, P.app_id);
    pa_proplist_sets(pl, PA_PROP_APPLICATION_ICON_NAME, "audio-volume-high");
    P.ctx = pa_context_new_with_proplist(pa_glib_mainloop_get_api(P.loop), "volpop", pl);
    pa_proplist_free(pl);
    pa_context_set_state_callback(P.ctx, on_state, NULL);
    if (pa_context_connect(P.ctx, NULL, PA_CONTEXT_NOFAIL, NULL) < 0) on_state(P.ctx, NULL);
}

void vp_pulse_start(VpModel *model, const char *app_id, VpChangedFn fn, gpointer data)
{
    P.model = model;
    P.app_id = g_strdup(app_id);
    P.changed = fn;
    P.data = data;
    P.loop = pa_glib_mainloop_new(NULL);
    P.pending = g_hash_table_new_full(g_int64_hash, g_int64_equal, g_free, g_free);
    connect_ctx();
}

void vp_pulse_stop(void)
{
    if (P.reconnect_id) g_source_remove(P.reconnect_id);
    P.reconnect_id = 0;
    drop_ctx();
    if (P.loop) pa_glib_mainloop_free(P.loop);
    P.loop = NULL;
    g_clear_pointer(&P.pending, g_hash_table_unref);
    g_clear_pointer(&P.app_id, g_free);
}

gboolean vp_pulse_connected(void)
{
    return P.ctx && pa_context_get_state(P.ctx) == PA_CONTEXT_READY;
}

/* ───────────── 写入：音量节流 ───────────── */

typedef enum { T_DEVICE, T_STREAM } Target;

typedef struct {
    gboolean inflight;
    gboolean has_next;
    pa_cvolume next;
} Pending;

static gint64 pkey(Target t, VpDir dir, guint32 idx)
{
    return ((gint64)t << 40) | ((gint64)dir << 32) | idx;
}

static void send_volume(Target t, VpDir dir, guint32 idx, const pa_cvolume *v);

static void volume_done(pa_context *c, int success, void *u)
{
    gint64 key = GPOINTER_TO_SIZE(u) - 1;
    Pending *p = P.pending ? g_hash_table_lookup(P.pending, &key) : NULL;
    if (!p) return;
    p->inflight = FALSE;
    if (p->has_next) {
        p->has_next = FALSE;
        pa_cvolume v = p->next;
        send_volume((Target)(key >> 40), (VpDir)((key >> 32) & 0xff), (guint32)key, &v);
    } else {
        g_hash_table_remove(P.pending, &key);
    }
}

static void send_volume(Target t, VpDir dir, guint32 idx, const pa_cvolume *v)
{
    if (!vp_pulse_connected()) return;
    gint64 key = pkey(t, dir, idx);
    Pending *p = g_hash_table_lookup(P.pending, &key);
    if (!p) {
        p = g_new0(Pending, 1);
        gint64 *k = g_new(gint64, 1);
        *k = key;
        g_hash_table_insert(P.pending, k, p);
    }
    if (p->inflight) { /* 只保留最新值 */
        p->next = *v;
        p->has_next = TRUE;
        return;
    }
    p->inflight = TRUE;
    gpointer ud = GSIZE_TO_POINTER((gsize)key + 1);
    pa_operation *o;
    if (t == T_DEVICE)
        o = dir == VP_OUTPUT ? pa_context_set_sink_volume_by_index(P.ctx, idx, v, volume_done, ud)
                             : pa_context_set_source_volume_by_index(P.ctx, idx, v, volume_done, ud);
    else
        o = dir == VP_OUTPUT ? pa_context_set_sink_input_volume(P.ctx, idx, v, volume_done, ud)
                             : pa_context_set_source_output_volume(P.ctx, idx, v, volume_done, ud);
    if (!o) p->inflight = FALSE;
    unref_op(o);
}

void vp_pulse_set_device_volume(VpDir dir, guint32 idx, const pa_cvolume *v) { send_volume(T_DEVICE, dir, idx, v); }
void vp_pulse_set_stream_volume(VpDir dir, guint32 idx, const pa_cvolume *v) { send_volume(T_STREAM, dir, idx, v); }

/* ───────────── 写入：其他 ───────────── */

#define CONNECTED_OR_RETURN() \
    do {                      \
        if (!vp_pulse_connected()) return; \
    } while (0)

void vp_pulse_set_device_mute(VpDir dir, guint32 idx, gboolean mute)
{
    CONNECTED_OR_RETURN();
    unref_op(dir == VP_OUTPUT ? pa_context_set_sink_mute_by_index(P.ctx, idx, mute, NULL, NULL)
                              : pa_context_set_source_mute_by_index(P.ctx, idx, mute, NULL, NULL));
}

void vp_pulse_set_default(VpDir dir, const char *name)
{
    CONNECTED_OR_RETURN();
    unref_op(dir == VP_OUTPUT ? pa_context_set_default_sink(P.ctx, name, NULL, NULL)
                              : pa_context_set_default_source(P.ctx, name, NULL, NULL));
}

void vp_pulse_set_port(VpDir dir, guint32 idx, const char *port)
{
    CONNECTED_OR_RETURN();
    unref_op(dir == VP_OUTPUT ? pa_context_set_sink_port_by_index(P.ctx, idx, port, NULL, NULL)
                              : pa_context_set_source_port_by_index(P.ctx, idx, port, NULL, NULL));
}

void vp_pulse_set_stream_mute(VpDir dir, guint32 idx, gboolean mute)
{
    CONNECTED_OR_RETURN();
    unref_op(dir == VP_OUTPUT ? pa_context_set_sink_input_mute(P.ctx, idx, mute, NULL, NULL)
                              : pa_context_set_source_output_mute(P.ctx, idx, mute, NULL, NULL));
}

void vp_pulse_move_stream(VpDir dir, guint32 idx, guint32 device)
{
    CONNECTED_OR_RETURN();
    unref_op(dir == VP_OUTPUT ? pa_context_move_sink_input_by_index(P.ctx, idx, device, NULL, NULL)
                              : pa_context_move_source_output_by_index(P.ctx, idx, device, NULL, NULL));
}

void vp_pulse_set_profile(guint32 card, const char *profile)
{
    CONNECTED_OR_RETURN();
    unref_op(pa_context_set_card_profile_by_index(P.ctx, card, profile, NULL, NULL));
}

/* ───────────── 电平表 ───────────── */

struct VpPeak {
    pa_stream *stream;
    char *source;
    VpPeakFn fn;
    gpointer data;
};

static void peak_read(pa_stream *s, size_t n, void *u)
{
    VpPeak *p = u;
    const void *buf = NULL;
    if (pa_stream_peek(s, &buf, &n) < 0) return;
    if (!buf) { /* 空洞 */
        if (n) pa_stream_drop(s);
        return;
    }
    float v = n >= sizeof(float) ? ((const float *)buf)[n / sizeof(float) - 1] : 0.0f;
    pa_stream_drop(s);
    if (p->fn) p->fn(CLAMP(v, 0.0f, 1.0f), p->data);
}

VpPeak *vp_peak_new(const char *source_name, VpPeakFn fn, gpointer data)
{
    if (!vp_pulse_connected() || !source_name) return NULL;
    static const pa_sample_spec ss = { .format = PA_SAMPLE_FLOAT32NE, .rate = 25, .channels = 1 };
    pa_proplist *pl = pa_proplist_new();
    pa_proplist_sets(pl, PA_PROP_APPLICATION_ID, P.app_id);
    pa_proplist_sets(pl, PA_PROP_MEDIA_ROLE, "abstract"); /* 不触发按角色的音量逻辑 */
    VpPeak *p = g_new0(VpPeak, 1);
    p->stream = pa_stream_new_with_proplist(P.ctx, "volpop peak detect", &ss, NULL, pl);
    pa_proplist_free(pl);
    if (!p->stream) {
        g_free(p);
        return NULL;
    }
    p->source = g_strdup(source_name);
    p->fn = fn;
    p->data = data;
    pa_stream_set_read_callback(p->stream, peak_read, p);
    pa_buffer_attr attr = { .maxlength = (uint32_t)-1, .fragsize = sizeof(float) };
    if (pa_stream_connect_record(p->stream, source_name, &attr,
                                 PA_STREAM_DONT_MOVE | PA_STREAM_PEAK_DETECT | PA_STREAM_ADJUST_LATENCY) < 0) {
        vp_peak_free(p);
        return NULL;
    }
    return p;
}

void vp_peak_free(VpPeak *p)
{
    if (!p) return;
    if (p->stream) {
        pa_stream_set_read_callback(p->stream, NULL, NULL);
        if (pa_stream_get_state(p->stream) == PA_STREAM_READY || pa_stream_get_state(p->stream) == PA_STREAM_CREATING)
            pa_stream_disconnect(p->stream);
        pa_stream_unref(p->stream);
    }
    g_free(p->source);
    g_free(p);
}

const char *vp_peak_source(const VpPeak *p)
{
    return p ? p->source : NULL;
}
