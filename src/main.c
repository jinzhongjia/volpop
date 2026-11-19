// SPDX-License-Identifier: GPL-3.0-or-later
/* volpop — 音量面板
 *
 *   volpop              切换显示 / 隐藏（首次运行会常驻）
 *   volpop --show | --hide | --settings | --daemon | --quit | --version
 */
#include "config.h"
#include "vp-app.h"

#include <stdio.h>

/* arg_data 必须为 NULL：否则 GLib 把值写进变量而不放进 options dict，常驻实例收不到 */
static const GOptionEntry entries[] = {
    { "show", 0, 0, G_OPTION_ARG_NONE, NULL, "显示面板", NULL },
    { "hide", 0, 0, G_OPTION_ARG_NONE, NULL, "隐藏面板", NULL },
    { "settings", 0, 0, G_OPTION_ARG_NONE, NULL, "打开“声音设置”页", NULL },
    { "daemon", 0, 0, G_OPTION_ARG_NONE, NULL, "仅在后台启动（用于自启动）", NULL },
    { "quit", 0, 0, G_OPTION_ARG_NONE, NULL, "退出常驻进程", NULL },
    { "version", 0, 0, G_OPTION_ARG_NONE, NULL, "显示版本", NULL },
    { NULL },
};

static int on_local_options(GApplication *app, GVariantDict *opts, gpointer _)
{
    if (g_variant_dict_contains(opts, "version")) {
        printf("volpop %s\n", VP_VERSION);
        return 0;
    }
    return -1;
}

static gboolean has(GVariantDict *d, const char *key)
{
    gboolean v = FALSE;
    return g_variant_dict_lookup(d, key, "b", &v) && v;
}

static int on_command_line(GApplication *app, GApplicationCommandLine *cl, gpointer _)
{
    GVariantDict *d = g_application_command_line_get_options_dict(cl);
    if (has(d, "quit")) {
        g_application_release(app);
        g_application_quit(app);
    } else if (has(d, "daemon")) {
        /* 只保持常驻 */
    } else if (has(d, "hide")) {
        vp_popup_hide();
    } else if (has(d, "settings")) {
        vp_popup_show();
        vp_show_page(VP_PAGE_SETTINGS);
    } else if (has(d, "show")) {
        vp_popup_show();
    } else {
        vp_popup_toggle();
    }
    return 0;
}

static void on_startup(GApplication *app, gpointer _)
{
    g_application_hold(app);
    vp_app_startup(GTK_APPLICATION(app));
}

static void on_shutdown(GApplication *app, gpointer _)
{
    vp_app_shutdown();
}

int main(int argc, char **argv)
{
    GtkApplication *app = gtk_application_new(VP_APP_ID, G_APPLICATION_HANDLES_COMMAND_LINE);
    g_application_add_main_option_entries(G_APPLICATION(app), entries);
    g_application_set_option_context_summary(G_APPLICATION(app), "音量面板");
    g_signal_connect(app, "handle-local-options", G_CALLBACK(on_local_options), NULL);
    g_signal_connect(app, "startup", G_CALLBACK(on_startup), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_shutdown), NULL);
    g_signal_connect(app, "command-line", G_CALLBACK(on_command_line), NULL);
    int r = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return r;
}
