// SPDX-License-Identifier: GPL-3.0-or-later
/* vp-widgets.h — 统一外观的小部件，以及可复用的音量控件 */
#pragma once

#include <gtk/gtk.h>

#include "vp-model.h"

G_BEGIN_DECLS

GtkWidget *vp_glyph(const char *glyph, const char *css_class);
GtkWidget *vp_label(const char *text, const char *css_class, float xalign);
GtkWidget *vp_icon_button(const char *glyph, const char *tooltip, GCallback cb, gpointer data);
GtkWidget *vp_glyph_button(const char *glyph, const char *text, const char *css_class, GCallback cb, gpointer data);
GtkWidget *vp_header(const char *title, gboolean back);
GtkWidget *vp_scroller(GtkWidget *child, int max_height);
GtkWidget *vp_section_title(const char *title);
GtkWidget *vp_card(void);
GtkWidget *vp_spacer(void);
void       vp_box_clear(GtkWidget *box);

/* 应用图标：图标主题里有就用图片，否则用音符字形 */
GtkWidget *vp_app_icon(const char *icon_name);

/* 下拉框：用字符串数组填充，内容不变时不重建；set_selected 期间不触发回调 */
GtkDropDown *vp_dropdown_new(GCallback on_selected, gpointer data);
void         vp_dropdown_set(GtkDropDown *dd, const char *const *labels, guint n, guint selected);

/* ── 音量控件：静音按钮 + 滑块 + 百分比 ──
 * 用户拖动后的短时间内忽略服务端推送的旧值，避免滑块回跳。 */
typedef struct {
    GtkWidget  *root;
    GtkWidget  *mute_btn, *mute_glyph;
    GtkScale   *scale;
    GtkLabel   *pct;
    gboolean    is_stream;
    VpDir       dir;
    guint32     index;
    pa_cvolume  volume;
    gboolean    mute;
    guint       upper;
    gboolean    updating;
    gint64      hold_until;
} VpVolumeCtl;

/* root 销毁时控件结构一起释放 */
VpVolumeCtl *vp_volume_ctl_new(gboolean is_stream, VpDir dir);
void         vp_volume_ctl_update(VpVolumeCtl *c, guint32 index, const pa_cvolume *v, gboolean mute,
                                  gboolean sensitive);

G_END_DECLS
