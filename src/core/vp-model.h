// SPDX-License-Identifier: GPL-3.0-or-later
/* vp-model.h — 音频对象的数据模型
 *
 * 后端把 libpulse 的 pa_*_info 转成这里的结构体存进 VpModel；界面只读取模型。
 * 过滤、排序、显示名称、图标分类等规则都在这里，因此可以脱离音频服务单元测试。
 */
#pragma once

#include <glib.h>
#include <pulse/channelmap.h>
#include <pulse/def.h>
#include <pulse/volume.h>

G_BEGIN_DECLS

typedef enum {
    VP_OUTPUT, /* sink / sink-input（播放） */
    VP_INPUT,  /* source / source-output（录音） */
} VpDir;

typedef enum {
    VP_KIND_SPEAKER,
    VP_KIND_HEADPHONES,
    VP_KIND_HEADSET,
    VP_KIND_BLUETOOTH,
    VP_KIND_HDMI,
    VP_KIND_USB,
    VP_KIND_MICROPHONE,
} VpDeviceKind;

typedef struct {
    char    *name;
    char    *description;
    guint32  priority;
    gboolean available; /* FALSE：明确未插入 */
} VpPort;

typedef struct {
    VpDir          dir;
    guint32        index;
    char          *name;
    char          *description;
    char          *bus;         /* device.bus：pci / usb / bluetooth … */
    char          *form_factor; /* device.form_factor：headphone / headset / speaker … */
    pa_cvolume     volume;
    pa_channel_map map;
    gboolean       mute;
    gboolean       decibel;      /* 支持 dB 音量 */
    gboolean       is_monitor;   /* 输入设备是某个输出的监听源 */
    char          *monitor_name; /* 输出设备的监听源名称（用于电平表） */
    guint32        card;
    GPtrArray     *ports; /* VpPort* */
    char          *active_port;
} VpDevice;

typedef struct {
    VpDir          dir;
    guint32        index;
    guint32        device; /* 所在的 sink / source 序号 */
    char          *app;
    char          *media;
    char          *icon;
    char          *role;
    char          *app_id;
    pa_cvolume     volume;
    pa_channel_map map;
    gboolean       mute;
    gboolean       corked;
    gboolean       has_volume;
} VpStream;

typedef struct {
    char    *name;
    char    *description;
    guint32  priority;
    gboolean available;
    guint32  n_sinks, n_sources;
} VpProfile;

typedef struct {
    guint32    index;
    char      *name;
    char      *description;
    GPtrArray *profiles; /* VpProfile* */
    char      *active_profile;
} VpCard;

typedef struct {
    GHashTable *devices[2]; /* index → VpDevice*，按 VpDir 分开 */
    GHashTable *streams[2]; /* index → VpStream* */
    GHashTable *cards;      /* index → VpCard* */
    char       *default_name[2];
    char       *self_app_id; /* 自己的流（电平表）不显示 */
} VpModel;

/* ── 构造 / 释放 ── */
VpPort    *vp_port_new(const char *name, const char *description, guint32 priority, gboolean available);
void       vp_port_free(VpPort *p);
VpDevice  *vp_device_new(VpDir dir, guint32 index, const char *name, const char *description);
void       vp_device_free(VpDevice *d);
VpStream  *vp_stream_new(VpDir dir, guint32 index, guint32 device);
void       vp_stream_free(VpStream *s);
VpProfile *vp_profile_new(const char *name, const char *description, guint32 priority, gboolean available);
void       vp_profile_free(VpProfile *p);
VpCard    *vp_card_new(guint32 index, const char *name, const char *description);
void       vp_card_free(VpCard *c);

VpModel *vp_model_new(const char *self_app_id);
void     vp_model_free(VpModel *m);
void     vp_model_clear(VpModel *m);

/* ── 更新（upsert 接管所有权） ── */
void vp_model_put_device(VpModel *m, VpDevice *d);
void vp_model_put_stream(VpModel *m, VpStream *s);
void vp_model_put_card(VpModel *m, VpCard *c);
gboolean vp_model_remove_device(VpModel *m, VpDir dir, guint32 index);
gboolean vp_model_remove_stream(VpModel *m, VpDir dir, guint32 index);
gboolean vp_model_remove_card(VpModel *m, guint32 index);
void vp_model_set_default(VpModel *m, VpDir dir, const char *name);

/* ── 查询（返回的数组只是容器，元素归模型所有） ── */
VpDevice  *vp_model_device(VpModel *m, VpDir dir, guint32 index);
VpDevice  *vp_model_default_device(VpModel *m, VpDir dir);
VpStream  *vp_model_stream(VpModel *m, VpDir dir, guint32 index);
VpCard    *vp_model_card(VpModel *m, guint32 index);
/* 默认设备在前，其余按描述排序；输入设备默认不含监听源 */
GPtrArray *vp_model_devices(VpModel *m, VpDir dir, gboolean include_monitors);
/* 去掉自己的流和系统提示音以外的过滤项，按应用名排序 */
GPtrArray *vp_model_streams(VpModel *m, VpDir dir);
GPtrArray *vp_model_cards(VpModel *m);
gboolean   vp_model_is_default(VpModel *m, const VpDevice *d);

/* ── 显示规则 ── */
VpDeviceKind vp_device_kind(const VpDevice *d);
const char  *vp_device_active_port_label(const VpDevice *d);
GPtrArray   *vp_device_ports_sorted(const VpDevice *d);  /* 可用的在前，按优先级降序 */
GPtrArray   *vp_card_profiles_sorted(const VpCard *c);   /* 可用的在前，按优先级降序，“关闭”放最后 */
char        *vp_stream_title(const VpStream *s);         /* 应用名，没有则用媒体名 */
char        *vp_stream_subtitle(const VpStream *s);      /* 媒体名（与标题相同时为空） */

G_END_DECLS
