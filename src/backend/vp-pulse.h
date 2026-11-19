// SPDX-License-Identifier: GPL-3.0-or-later
/* vp-pulse.h — libpulse 后端（PulseAudio 或 pipewire-pulse），运行在 GLib 主循环上
 *
 * 连接后订阅所有变化并写入 VpModel，每次更新后调用 changed 回调（由调用方合并刷新）。
 * 断线会自动重连。音量设置做了节流：同一对象同时最多一个请求在途，期间只保留最新值。
 */
#pragma once

#include "vp-model.h"

G_BEGIN_DECLS

typedef void (*VpChangedFn)(gpointer data);

void     vp_pulse_start(VpModel *model, const char *app_id, VpChangedFn changed, gpointer data);
void     vp_pulse_stop(void);
gboolean vp_pulse_connected(void);

void vp_pulse_set_device_volume(VpDir dir, guint32 index, const pa_cvolume *v);
void vp_pulse_set_device_mute(VpDir dir, guint32 index, gboolean mute);
void vp_pulse_set_default(VpDir dir, const char *name);
void vp_pulse_set_port(VpDir dir, guint32 index, const char *port);
void vp_pulse_set_stream_volume(VpDir dir, guint32 index, const pa_cvolume *v);
void vp_pulse_set_stream_mute(VpDir dir, guint32 index, gboolean mute);
void vp_pulse_move_stream(VpDir dir, guint32 index, guint32 device);
void vp_pulse_set_profile(guint32 card, const char *profile);

/* 电平表：对 source（输出设备用其监听源）做峰值检测，约 25 次/秒回调 0.0–1.0 */
typedef struct VpPeak VpPeak;
typedef void (*VpPeakFn)(float level, gpointer data);
VpPeak *vp_peak_new(const char *source_name, VpPeakFn fn, gpointer data);
void    vp_peak_free(VpPeak *p);
const char *vp_peak_source(const VpPeak *p);

G_END_DECLS
