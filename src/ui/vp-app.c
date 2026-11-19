// SPDX-License-Identifier: GPL-3.0-or-later
#include "vp-app.h"
#include "vp-pages.h"
#include "vp-pulse.h"

#if HAVE_LAYER_SHELL
#include <gtk4-layer-shell.h>
#endif

#define RES_PREFIX "/dev/volpop/Volpop/"

static VpApp app_state;
VpApp *vp = &app_state;

/* layer-shell 模式下的“点外面关闭”：面板打开时在它下面铺一层全透明浮层，点到就收起 */
static GtkWindow *backdrop;

const char *vp_device_glyph(const VpDevice *d)
{
    switch (vp_device_kind(d)) {
    case VP_KIND_HEADPHONES: return VP_G_HEADPH;
    case VP_KIND_HEADSET: return VP_G_HEADSET;
    case VP_KIND_BLUETOOTH: return VP_G_BT;
    case VP_KIND_HDMI: return VP_G_HDMI;
    case VP_KIND_USB: return VP_G_USB;
    case VP_KIND_MICROPHONE: return VP_G_MIC;
    default: return VP_G_SPEAKER;
    }
}

/* ───────────── 样式 ───────────── */

static char *read_resource(const char *name)
{
    char *path = g_strconcat(RES_PREFIX, name, NULL);
    GBytes *b = g_resources_lookup_data(path, 0, NULL);
    g_free(path);
    if (!b) return g_strdup("");
    char *s = g_strndup(g_bytes_get_data(b, NULL), g_bytes_get_size(b));
    g_bytes_unref(b);
    return s;
}

static char *read_file(const char *path)
{
    char *s = NULL;
    if (path) g_file_get_contents(path, &s, NULL, NULL);
    return s ? s : g_strdup("");
}

static void load_css(void)
{
    char *palette = read_resource("palette.css"), *colors = read_file(vp->config->colors_css);
    char *style = read_resource("style.css"), *user = read_file(vp->config->style_css);
    char *css = g_strjoin("\n", palette, colors, style, user, NULL);
    if (g_strcmp0(css, vp->css_cache)) {
        gtk_css_provider_load_from_string(vp->css, css);
        g_free(vp->css_cache);
        vp->css_cache = css;
    } else {
        g_free(css);
    }
    g_free(palette);
    g_free(colors);
    g_free(style);
    g_free(user);
}

static void on_css_error(GtkCssProvider *p, GtkCssSection *section, GError *error, gpointer _)
{
    char *loc = gtk_css_section_to_string(section);
    g_warning("CSS %s: %s", loc, error->message);
    g_free(loc);
}

/* ───────────── 刷新 ───────────── */

static void refresh_all(void)
{
    vp->dirty = FALSE;
    vp_page_main_refresh();
    vp_page_settings_refresh();
}

static gboolean refresh_cb(gpointer _)
{
    vp->refresh_id = 0;
    refresh_all();
    return G_SOURCE_REMOVE;
}

void vp_queue_refresh(void)
{
    if (!vp_popup_visible()) {
        vp->dirty = TRUE; /* 隐藏时不做界面工作 */
        return;
    }
    /* 音量变化可能非常密集（拖动其他程序的滑块），合并到约 30 fps */
    if (!vp->refresh_id) vp->refresh_id = g_timeout_add(33, refresh_cb, NULL);
}

static void on_model_changed(gpointer _)
{
    vp_queue_refresh();
}

/* ───────────── 页面 / 弹出 ───────────── */

void vp_show_page(const char *page)
{
    gtk_stack_set_visible_child_name(vp->stack, page);
    vp_page_main_set_active(vp_popup_visible() && !g_strcmp0(page, VP_PAGE_MAIN));
    refresh_all();
}

gboolean vp_popup_visible(void)
{
    return vp->win && gtk_widget_get_visible(GTK_WIDGET(vp->win));
}

void vp_popup_show(void)
{
    load_css();
    if (vp->refresh_id) {
        g_source_remove(vp->refresh_id);
        vp->refresh_id = 0;
    }
    vp->shown_at = g_get_monotonic_time();
    vp->was_active = FALSE;
    if (!vp_popup_visible() && backdrop) gtk_window_present(backdrop); /* 先映射浮层，面板才在它上面 */
    gtk_window_present(vp->win);
    vp_show_page(gtk_stack_get_visible_child_name(vp->stack));
}

void vp_popup_hide(void)
{
    if (!vp_popup_visible()) return;
    vp_page_main_set_active(FALSE);
    gtk_widget_set_visible(GTK_WIDGET(vp->win), FALSE);
    if (backdrop) gtk_widget_set_visible(GTK_WIDGET(backdrop), FALSE);
    gtk_stack_set_visible_child_name(vp->stack, VP_PAGE_MAIN);
}

void vp_popup_toggle(void)
{
    if (vp_popup_visible()) vp_popup_hide();
    else vp_popup_show();
}

static gboolean focus_in_popover(void)
{
    GtkWidget *f = gtk_root_get_focus(GTK_ROOT(vp->win));
    return f && gtk_widget_get_ancestor(f, GTK_TYPE_POPOVER);
}

static gboolean check_focus_lost(gpointer _)
{
    if (vp_popup_visible() && !gtk_window_is_active(vp->win) && !focus_in_popover()) vp_popup_hide();
    return G_SOURCE_REMOVE;
}

/* 普通窗口模式下靠 is-active 判断点了别处；layer-shell 模式靠浮层 */
static void on_active_changed(GObject *w, GParamSpec *p, gpointer _)
{
    if (gtk_window_is_active(vp->win)) {
        vp->was_active = TRUE;
    } else if (vp->config->hide_on_focus_loss && vp->was_active &&
               g_get_monotonic_time() - vp->shown_at > 300 * G_TIME_SPAN_MILLISECOND) {
        g_timeout_add(150, check_focus_lost, NULL);
    }
}

static gboolean on_key(GtkEventControllerKey *c, guint keyval, guint code, GdkModifierType mods, gpointer _)
{
    if (keyval != GDK_KEY_Escape) return FALSE;
    if (g_strcmp0(gtk_stack_get_visible_child_name(vp->stack), VP_PAGE_MAIN)) vp_show_page(VP_PAGE_MAIN);
    else vp_popup_hide();
    return TRUE;
}

static gboolean on_close_request(GtkWindow *w, gpointer _)
{
    vp_popup_hide();
    return TRUE;
}

#if HAVE_LAYER_SHELL
static void on_backdrop_pressed(GtkGestureClick *g, int n, double x, double y, gpointer _)
{
    vp_popup_hide();
}
#endif

static void setup_layer_shell(void)
{
#if HAVE_LAYER_SHELL
    if (!gtk_layer_is_supported()) return;
    VpConfig *c = vp->config;
    vp->layer = TRUE;
    gtk_layer_init_for_window(vp->win);
    gtk_layer_set_namespace(vp->win, "volpop");
    gtk_layer_set_layer(vp->win, GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor(vp->win, GTK_LAYER_SHELL_EDGE_TOP, c->anchor_top);
    gtk_layer_set_anchor(vp->win, GTK_LAYER_SHELL_EDGE_BOTTOM, c->anchor_bottom);
    gtk_layer_set_anchor(vp->win, GTK_LAYER_SHELL_EDGE_LEFT, c->anchor_left);
    gtk_layer_set_anchor(vp->win, GTK_LAYER_SHELL_EDGE_RIGHT, c->anchor_right);
    gtk_layer_set_margin(vp->win, GTK_LAYER_SHELL_EDGE_TOP, c->margin_top);
    gtk_layer_set_margin(vp->win, GTK_LAYER_SHELL_EDGE_BOTTOM, c->margin_bottom);
    gtk_layer_set_margin(vp->win, GTK_LAYER_SHELL_EDGE_LEFT, c->margin_left);
    gtk_layer_set_margin(vp->win, GTK_LAYER_SHELL_EDGE_RIGHT, c->margin_right);
    gtk_layer_set_keyboard_mode(vp->win, GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    if (!c->hide_on_focus_loss) return;
    backdrop = GTK_WINDOW(gtk_window_new());
    gtk_window_set_application(backdrop, vp->app);
    gtk_window_set_decorated(backdrop, FALSE);
    gtk_widget_add_css_class(GTK_WIDGET(backdrop), "volpop-backdrop");
    gtk_layer_init_for_window(backdrop);
    gtk_layer_set_namespace(backdrop, "volpop-backdrop");
    gtk_layer_set_layer(backdrop, GTK_LAYER_SHELL_LAYER_TOP);
    for (int e = GTK_LAYER_SHELL_EDGE_LEFT; e <= GTK_LAYER_SHELL_EDGE_BOTTOM; e++)
        gtk_layer_set_anchor(backdrop, e, TRUE);
    gtk_layer_set_keyboard_mode(backdrop, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    GtkGesture *click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(click, "pressed", G_CALLBACK(on_backdrop_pressed), NULL);
    gtk_widget_add_controller(GTK_WIDGET(backdrop), GTK_EVENT_CONTROLLER(click));
#endif
}

static void build_window(void)
{
    vp->css = gtk_css_provider_new();
    g_signal_connect(vp->css, "parsing-error", G_CALLBACK(on_css_error), NULL);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(vp->css),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 10);

    vp->win = GTK_WINDOW(gtk_application_window_new(vp->app));
    gtk_window_set_title(vp->win, "Volpop");
    gtk_window_set_decorated(vp->win, FALSE);
    gtk_window_set_resizable(vp->win, FALSE);
    gtk_widget_add_css_class(GTK_WIDGET(vp->win), "volpop");
    setup_layer_shell();
    gtk_widget_add_css_class(GTK_WIDGET(vp->win), vp->layer ? "layer" : "toplevel");

    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(panel, "panel");
    gtk_widget_set_size_request(panel, vp->config->width, -1);
    vp->stack = GTK_STACK(gtk_stack_new());
    gtk_stack_set_vhomogeneous(vp->stack, FALSE);
    gtk_stack_set_transition_type(vp->stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(vp->stack, 90);
    gtk_stack_add_named(vp->stack, vp_page_main_new(), VP_PAGE_MAIN);
    gtk_stack_add_named(vp->stack, vp_page_settings_new(), VP_PAGE_SETTINGS);
    gtk_box_append(GTK_BOX(panel), GTK_WIDGET(vp->stack));
    gtk_window_set_child(vp->win, panel);

    GtkEventController *kc = gtk_event_controller_key_new();
    g_signal_connect(kc, "key-pressed", G_CALLBACK(on_key), NULL);
    gtk_widget_add_controller(GTK_WIDGET(vp->win), kc);
    g_signal_connect(vp->win, "close-request", G_CALLBACK(on_close_request), NULL);
    g_signal_connect(vp->win, "notify::is-active", G_CALLBACK(on_active_changed), NULL);
    load_css();
}

/* ───────────── 生命周期 ───────────── */

void vp_app_startup(GtkApplication *app)
{
    vp->app = app;
    vp->config = vp_config_new();
    char *path = vp_config_default_path();
    GError *e = NULL;
    if (!vp_config_load_file(vp->config, path, &e)) {
        g_warning("%s", e->message);
        g_error_free(e);
    }
    g_free(path);
    vp->model = vp_model_new(VP_APP_ID);
    build_window();
    vp->dirty = TRUE;
    vp_pulse_start(vp->model, VP_APP_ID, on_model_changed, NULL);
}

void vp_app_shutdown(void)
{
    vp_page_main_set_active(FALSE);
    if (vp->refresh_id) g_source_remove(vp->refresh_id);
    vp->refresh_id = 0;
    vp_pulse_stop();
    g_clear_pointer(&vp->model, vp_model_free);
    g_clear_pointer(&vp->config, vp_config_free);
    g_clear_pointer(&vp->css_cache, g_free);
}
