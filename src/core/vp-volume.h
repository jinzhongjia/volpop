// SPDX-License-Identifier: GPL-3.0-or-later
/* vp-volume.h — 音量百分比、步进、平衡、显示文字（纯函数，不需要音频服务） */
#pragma once

#include <glib.h>
#include <pulse/volume.h>

G_BEGIN_DECLS

#define VP_VOLUME_MAX_LIMIT 150 /* 允许的最大放大比例（与 pavucontrol 相同） */

/* 0–max 的百分比：取各声道最大值，四舍五入 */
guint vp_volume_percent(const pa_cvolume *v);
/* 设为 pct%（截断到 [0, max_pct]），保持各声道比例（平衡）不变 */
void  vp_volume_set_percent(pa_cvolume *v, guint pct, guint max_pct);
/* 在当前值基础上加减 delta 个百分点 */
void  vp_volume_adjust(pa_cvolume *v, int delta, guint max_pct);

/* 0：静音或 0%，1：低，2：中，3：高 */
guint vp_volume_level(guint pct, gboolean mute);
char *vp_volume_label(guint pct, gboolean mute); /* “静音” 或 “45%” */
char *vp_volume_db_label(const pa_cvolume *v);   /* “-12.3 dB” 或 “-∞ dB” */

/* 平衡：-1.0（全左）到 1.0（全右）；声道布局不支持时返回 FALSE */
gboolean vp_balance_supported(const pa_channel_map *map);
float    vp_balance_get(const pa_cvolume *v, const pa_channel_map *map);
gboolean vp_balance_set(pa_cvolume *v, const pa_channel_map *map, float balance);

G_END_DECLS
