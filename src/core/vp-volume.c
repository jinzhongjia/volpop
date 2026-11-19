// SPDX-License-Identifier: GPL-3.0-or-later
#include "vp-volume.h"

#include <math.h>

guint vp_volume_percent(const pa_cvolume *v)
{
    if (!v || !pa_cvolume_valid(v)) return 0;
    pa_volume_t m = pa_cvolume_max(v);
    return (guint)(((guint64)m * 100 + PA_VOLUME_NORM / 2) / PA_VOLUME_NORM);
}

void vp_volume_set_percent(pa_cvolume *v, guint pct, guint max_pct)
{
    if (!v || !pa_cvolume_valid(v)) return;
    max_pct = CLAMP(max_pct, 1, VP_VOLUME_MAX_LIMIT);
    pct = MIN(pct, max_pct);
    pa_volume_t target = (pa_volume_t)(((guint64)pct * PA_VOLUME_NORM + 50) / 100);
    /* 全部声道为 0 时 pa_cvolume_scale 会把各声道设成相同值，平衡回到居中 */
    pa_cvolume_scale(v, target);
}

void vp_volume_adjust(pa_cvolume *v, int delta, guint max_pct)
{
    int cur = (int)vp_volume_percent(v);
    vp_volume_set_percent(v, (guint)MAX(0, cur + delta), max_pct);
}

guint vp_volume_level(guint pct, gboolean mute)
{
    if (mute || pct == 0) return 0;
    if (pct < 34) return 1;
    if (pct < 67) return 2;
    return 3;
}

char *vp_volume_label(guint pct, gboolean mute)
{
    return mute ? g_strdup("静音") : g_strdup_printf("%u%%", pct);
}

char *vp_volume_db_label(const pa_cvolume *v)
{
    if (!v || !pa_cvolume_valid(v)) return g_strdup("");
    double db = pa_sw_volume_to_dB(pa_cvolume_max(v));
    if (isinf(db) || db <= PA_DECIBEL_MININFTY) return g_strdup("-∞ dB");
    return g_strdup_printf("%+.1f dB", db);
}

gboolean vp_balance_supported(const pa_channel_map *map)
{
    return map && pa_channel_map_valid(map) && pa_channel_map_can_balance(map);
}

float vp_balance_get(const pa_cvolume *v, const pa_channel_map *map)
{
    if (!v || !vp_balance_supported(map) || v->channels != map->channels) return 0.0f;
    return pa_cvolume_get_balance(v, map);
}

gboolean vp_balance_set(pa_cvolume *v, const pa_channel_map *map, float balance)
{
    if (!v || !vp_balance_supported(map) || v->channels != map->channels) return FALSE;
    balance = CLAMP(balance, -1.0f, 1.0f);
    /* 平衡与“最响声道”是分开的：先记下音量，设完平衡后再恢复 */
    pa_volume_t m = pa_cvolume_max(v);
    if (m == PA_VOLUME_MUTED) return FALSE;
    if (!pa_cvolume_set_balance(v, map, balance)) return FALSE;
    pa_cvolume_scale(v, m);
    return TRUE;
}
