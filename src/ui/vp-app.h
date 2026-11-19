// SPDX-License-Identifier: GPL-3.0-or-later
/* vp-app.h — 应用全局状态：窗口、页面、弹出 / 收起、刷新调度 */
#pragma once

#include <gtk/gtk.h>

#include "config.h"
#include "vp-config.h"
#include "vp-model.h"

G_BEGIN_DECLS

/* Nerd Font（Material Design）图标 */
#define VP_G_VOL0     "\U000F075F" /* 静音 */
#define VP_G_VOL1     "\U000F057F"
#define VP_G_VOL2     "\U000F0580"
#define VP_G_VOL3     "\U000F057E"
#define VP_G_MIC      "\U000F036C"
#define VP_G_MIC_OFF  "\U000F036D"
#define VP_G_HEADPH   "\U000F02CB"
#define VP_G_HEADSET  "\U000F02CE"
#define VP_G_BT       "\U000F00B0"
#define VP_G_HDMI     "\U000F0379"
#define VP_G_USB      "\U000F0553"
#define VP_G_SPEAKER  "\U000F04C3"
#define VP_G_APP      "\U000F0387"
#define VP_G_SETTINGS "\U000F0493"
#define VP_G_BACK     "\U000F0141"
#define VP_G_MOVE     "\U000F0142"

#define VP_PAGE_MAIN     "main"
#define VP_PAGE_SETTINGS "settings"

typedef struct {
    GtkApplication *app;
    VpConfig       *config;
    VpModel        *model;

    GtkWindow      *win;
    GtkStack       *stack;
    GtkCssProvider *css;
    char           *css_cache;
    gboolean        layer;

    gint64   shown_at;
    gboolean was_active;
    gboolean dirty;
    guint    refresh_id;
} VpApp;

extern VpApp *vp;

void vp_app_startup(GtkApplication *app);
void vp_app_shutdown(void);

gboolean vp_popup_visible(void);
void     vp_popup_show(void);
void     vp_popup_hide(void);
void     vp_popup_toggle(void);
void     vp_show_page(const char *page);

void vp_queue_refresh(void);

const char *vp_device_glyph(const VpDevice *d);

G_END_DECLS
