// SPDX-License-Identifier: GPL-3.0-or-later
/* vp-config.h — 用户配置（$XDG_CONFIG_HOME/volpop/volpop.conf，GKeyFile 格式）
 *
 * [appearance]
 * colors = ~/.config/theme/current/waybar.css   ; 定义 @bg @surface @fg @subtle @muted @blue @red @green
 * style  = ~/.config/volpop/style.css
 * width  = 368
 *
 * [popup]
 * anchor = top-right
 * margin-top = 6
 * margin-right = 8
 * hide-on-focus-loss = true
 *
 * [audio]
 * max-volume = 100          ; 滑块上限，100–150（超过 100% 为软件放大）
 * peak-meters = true        ; 面板可见时显示电平表
 */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
    char    *colors_css;
    char    *style_css;
    int      width;
    gboolean anchor_top, anchor_bottom, anchor_left, anchor_right;
    int      margin_top, margin_right, margin_bottom, margin_left;
    gboolean hide_on_focus_loss;
    int      max_volume;
    gboolean peak_meters;
} VpConfig;

#define VP_CONFIG_ERROR (vp_config_error_quark())
GQuark vp_config_error_quark(void);

VpConfig *vp_config_new(void);
void      vp_config_free(VpConfig *cfg);
char     *vp_config_default_path(void);
char     *vp_expand_home(const char *path);

gboolean vp_config_set_anchor(VpConfig *cfg, const char *anchor, GError **error);
gboolean vp_config_load_data(VpConfig *cfg, const char *data, gsize len, GError **error);
gboolean vp_config_load_file(VpConfig *cfg, const char *path, GError **error); /* 文件不存在视为成功 */

G_DEFINE_AUTOPTR_CLEANUP_FUNC(VpConfig, vp_config_free)

G_END_DECLS
