// SPDX-License-Identifier: GPL-3.0-or-later
#include "vp-widgets.h"
#include "vp-app.h"
#include "vp-pulse.h"
#include "vp-volume.h"

#include <math.h>

GtkWidget *vp_glyph(const char *g, const char *cls)
{
    GtkWidget *l = gtk_label_new(g);
    gtk_widget_add_css_class(l, "glyph");
    if (cls) gtk_widget_add_css_class(l, cls);
    return l;
}

GtkWidget *vp_label(const char *text, const char *cls, float xalign)
{
    GtkWidget *l = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(l), xalign);
    if (cls) gtk_widget_add_css_class(l, cls);
    return l;
}

GtkWidget *vp_icon_button(const char *g, const char *tip, GCallback cb, gpointer data)
{
    GtkWidget *b = gtk_button_new();
    gtk_button_set_child(GTK_BUTTON(b), vp_glyph(g, NULL));
    gtk_widget_add_css_class(b, "icon");
    gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
    if (tip) gtk_widget_set_tooltip_text(b, tip);
    if (cb) g_signal_connect_swapped(b, "clicked", cb, data);
    return b;
}

GtkWidget *vp_glyph_button(const char *g, const char *text, const char *cls, GCallback cb, gpointer data)
{
    GtkWidget *b = gtk_button_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(box), vp_glyph(g, NULL));
    gtk_box_append(GTK_BOX(box), gtk_label_new(text));
    gtk_button_set_child(GTK_BUTTON(b), box);
    if (cls) gtk_widget_add_css_class(b, cls);
    if (cb) g_signal_connect_swapped(b, "clicked", cb, data);
    return b;
}

static void go_main(void)
{
    vp_show_page(VP_PAGE_MAIN);
}

GtkWidget *vp_header(const char *title, gboolean back)
{
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(h, "header");
    if (back) gtk_box_append(GTK_BOX(h), vp_icon_button(VP_G_BACK, "返回 (Esc)", G_CALLBACK(go_main), NULL));
    GtkWidget *t = vp_label(title, "title", 0);
    gtk_widget_set_hexpand(t, TRUE);
    gtk_widget_set_margin_start(t, back ? 2 : 6);
    gtk_box_append(GTK_BOX(h), t);
    return h;
}

GtkWidget *vp_scroller(GtkWidget *child, int max_height)
{
    GtkWidget *sc = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(sc), TRUE);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(sc), max_height);
    /* 右侧 8px 留给悬浮滚动条，见 style.css 中 .panel 的内边距 */
    gtk_widget_set_margin_end(child, 8);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sc), child);
    return sc;
}

GtkWidget *vp_section_title(const char *title)
{
    return vp_label(title, "section", 0);
}

GtkWidget *vp_card(void)
{
    GtkWidget *c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(c, "card");
    return c;
}

GtkWidget *vp_spacer(void)
{
    GtkWidget *s = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(s, TRUE);
    return s;
}

void vp_box_clear(GtkWidget *box)
{
    GtkWidget *c;
    while ((c = gtk_widget_get_first_child(box))) gtk_box_remove(GTK_BOX(box), c);
}

GtkWidget *vp_app_icon(const char *icon_name)
{
    GtkIconTheme *theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
    if (icon_name && *icon_name && gtk_icon_theme_has_icon(theme, icon_name)) {
        GtkWidget *img = gtk_image_new_from_icon_name(icon_name);
        gtk_image_set_pixel_size(GTK_IMAGE(img), 22);
        gtk_widget_add_css_class(img, "app-icon");
        return img;
    }
    return vp_glyph(VP_G_APP, "app-glyph");
}

/* ───────────── 下拉框 ───────────── */

GtkDropDown *vp_dropdown_new(GCallback on_selected, gpointer data)
{
    GtkWidget *dd = gtk_drop_down_new(G_LIST_MODEL(gtk_string_list_new(NULL)), NULL);
    gtk_widget_set_hexpand(dd, TRUE);
    if (on_selected) g_signal_connect(dd, "notify::selected", on_selected, data);
    return GTK_DROP_DOWN(dd);
}

static gboolean same_labels(GListModel *m, const char *const *labels, guint n)
{
    if (g_list_model_get_n_items(m) != n) return FALSE;
    for (guint i = 0; i < n; i++)
        if (g_strcmp0(gtk_string_list_get_string(GTK_STRING_LIST(m), i), labels[i])) return FALSE;
    return TRUE;
}

void vp_dropdown_set(GtkDropDown *dd, const char *const *labels, guint n, guint selected)
{
    g_object_set_data(G_OBJECT(dd), "vp-updating", GINT_TO_POINTER(1));
    GListModel *m = gtk_drop_down_get_model(dd);
    if (!same_labels(m, labels, n)) {
        GtkStringList *l = gtk_string_list_new(NULL);
        for (guint i = 0; i < n; i++) gtk_string_list_append(l, labels[i]);
        gtk_drop_down_set_model(dd, G_LIST_MODEL(l));
        g_object_unref(l);
    }
    if (gtk_drop_down_get_selected(dd) != selected) gtk_drop_down_set_selected(dd, selected);
    gtk_widget_set_sensitive(GTK_WIDGET(dd), n > 1);
    g_object_set_data(G_OBJECT(dd), "vp-updating", NULL);
}

/* ───────────── 音量控件 ───────────── */

#define HOLD_US (600 * G_TIME_SPAN_MILLISECOND)

static const char *mute_glyph(const VpVolumeCtl *c)
{
    if (c->dir == VP_INPUT) return c->mute ? VP_G_MIC_OFF : VP_G_MIC;
    static const char *g[] = { VP_G_VOL0, VP_G_VOL1, VP_G_VOL2, VP_G_VOL3 };
    return g[vp_volume_level(vp_volume_percent(&c->volume), c->mute)];
}

static void refresh_labels(VpVolumeCtl *c)
{
    guint pct = vp_volume_percent(&c->volume);
    char *s = vp_volume_label(pct, c->mute);
    gtk_label_set_text(c->pct, s);
    g_free(s);
    gtk_label_set_text(GTK_LABEL(c->mute_glyph), mute_glyph(c));
    if (c->mute) gtk_widget_add_css_class(c->root, "muted");
    else gtk_widget_remove_css_class(c->root, "muted");
    gtk_widget_set_tooltip_text(c->mute_btn, c->mute ? "取消静音" : "静音");
}

static void send_volume(VpVolumeCtl *c)
{
    if (c->is_stream) vp_pulse_set_stream_volume(c->dir, c->index, &c->volume);
    else vp_pulse_set_device_volume(c->dir, c->index, &c->volume);
}

static void send_mute(VpVolumeCtl *c)
{
    if (c->is_stream) vp_pulse_set_stream_mute(c->dir, c->index, c->mute);
    else vp_pulse_set_device_mute(c->dir, c->index, c->mute);
}

static void on_scale(GtkRange *r, VpVolumeCtl *c)
{
    if (c->updating || !pa_cvolume_valid(&c->volume)) return;
    guint pct = (guint)lround(gtk_range_get_value(r));
    if (pct == vp_volume_percent(&c->volume)) return;
    vp_volume_set_percent(&c->volume, pct, c->upper);
    c->hold_until = g_get_monotonic_time() + HOLD_US;
    send_volume(c);
    if (c->mute && pct > 0) { /* 调大音量时自动取消静音 */
        c->mute = FALSE;
        send_mute(c);
    }
    refresh_labels(c);
}

static void on_mute(VpVolumeCtl *c)
{
    c->mute = !c->mute;
    c->hold_until = g_get_monotonic_time() + HOLD_US;
    send_mute(c);
    refresh_labels(c);
}

static void set_upper(VpVolumeCtl *c, guint upper)
{
    if (upper == c->upper) return;
    c->upper = upper;
    gtk_range_set_range(GTK_RANGE(c->scale), 0, upper);
    gtk_scale_clear_marks(c->scale);
    if (upper > 100) gtk_scale_add_mark(c->scale, 100, GTK_POS_BOTTOM, NULL);
}

VpVolumeCtl *vp_volume_ctl_new(gboolean is_stream, VpDir dir)
{
    VpVolumeCtl *c = g_new0(VpVolumeCtl, 1);
    c->is_stream = is_stream;
    c->dir = dir;
    pa_cvolume_init(&c->volume);

    c->root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(c->root, "volume");
    c->mute_glyph = vp_glyph(VP_G_VOL3, NULL);
    c->mute_btn = gtk_button_new();
    gtk_button_set_child(GTK_BUTTON(c->mute_btn), c->mute_glyph);
    gtk_widget_add_css_class(c->mute_btn, "icon");
    gtk_widget_add_css_class(c->mute_btn, "mute");
    gtk_widget_set_valign(c->mute_btn, GTK_ALIGN_CENTER);
    g_signal_connect_swapped(c->mute_btn, "clicked", G_CALLBACK(on_mute), c);
    gtk_box_append(GTK_BOX(c->root), c->mute_btn);

    c->scale = GTK_SCALE(gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, NULL));
    gtk_scale_set_draw_value(c->scale, FALSE);
    gtk_range_set_increments(GTK_RANGE(c->scale), 1, 5);
    gtk_widget_set_hexpand(GTK_WIDGET(c->scale), TRUE);
    set_upper(c, vp->config->max_volume);
    g_signal_connect(c->scale, "value-changed", G_CALLBACK(on_scale), c);
    gtk_box_append(GTK_BOX(c->root), GTK_WIDGET(c->scale));

    c->pct = GTK_LABEL(vp_label("", "pct", 1));
    gtk_label_set_width_chars(c->pct, 4);
    gtk_box_append(GTK_BOX(c->root), GTK_WIDGET(c->pct));

    g_object_set_data_full(G_OBJECT(c->root), "vp-ctl", c, g_free);
    return c;
}

void vp_volume_ctl_update(VpVolumeCtl *c, guint32 index, const pa_cvolume *v, gboolean mute, gboolean sensitive)
{
    gboolean same_target = c->index == index;
    c->index = index;
    gtk_widget_set_sensitive(c->root, sensitive);
    /* 刚操作过：服务端可能还在推送旧值，先不覆盖本地状态 */
    if (same_target && g_get_monotonic_time() < c->hold_until) return;

    c->volume = *v;
    c->mute = mute;
    guint pct = vp_volume_percent(v);
    c->updating = TRUE;
    set_upper(c, MAX((guint)vp->config->max_volume, pct));
    if ((guint)lround(gtk_range_get_value(GTK_RANGE(c->scale))) != pct) gtk_range_set_value(GTK_RANGE(c->scale), pct);
    c->updating = FALSE;
    refresh_labels(c);
}
