// SPDX-License-Identifier: GPL-3.0-or-later
/* vp-pages.h — 页面入口 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* 首页：默认输出 / 输入、正在播放的应用 */
GtkWidget *vp_page_main_new(void);
void       vp_page_main_refresh(void);
void       vp_page_main_set_active(gboolean active); /* 可见时才开电平表 */

/* 声音设置：输出 / 输入 / 录音 / 配置 */
GtkWidget *vp_page_settings_new(void);
void       vp_page_settings_refresh(void);

G_END_DECLS
