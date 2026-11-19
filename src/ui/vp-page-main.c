// SPDX-License-Identifier: GPL-3.0-or-later
/* 首页：默认输出 / 输入（切换设备、音量、电平）+ 正在播放的应用
 *
 * 所有行都按序号原地更新而不是重建，避免拖动中的滑块被刷新打断。
 */
#include "vp-app.h"
#include "vp-pages.h"
#include "vp-pulse.h"
#include "vp-widgets.h"

typedef struct {
    VpDir dir;
    GtkWidget *card, *controls, *empty, *kind;
    GtkDropDown *dd;
    GPtrArray *names; /* 下拉框第 i 项对应的设备名 */
    VpVolumeCtl *ctl;
    GtkLevelBar *peak;
    VpPeak *meter;
    double level;
} DevSection;

typedef struct {
    guint32 index;
    GtkWidget *root, *icon_box, *move;
    GtkLabel *title, *sub;
    VpVolumeCtl *ctl;
    char *icon;
    char *move_sig;
} AppRow;

static struct {
    DevSection out, in;
    GtkWidget *apps_box, *apps_empty;
    GHashTable *apps; /* index → AppRow* */
    gboolean active;
} M;

/* ───────────── 设备区 ───────────── */

static void on_peak(float v, gpointer data)
{
    DevSection *s = data;
    /* 快升慢降，看起来更稳 */
    s->level = MAX((double)v, s->level * 0.82);
    gtk_level_bar_set_value(s->peak, s->level);
}

static void stop_meter(DevSection *s)
{
    g_clear_pointer(&s->meter, vp_peak_free);
    s->level = 0;
    if (s->peak) gtk_level_bar_set_value(s->peak, 0);
}

static void on_device_selected(GObject *dd, GParamSpec *ps, DevSection *s)
{
    if (g_object_get_data(dd, "vp-updating")) return;
    guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    if (i < s->names->len) vp_pulse_set_default(s->dir, s->names->pdata[i]);
}

static char *device_label(const VpDevice *d)
{
    const char *port = d->ports->len > 1 ? vp_device_active_port_label(d) : NULL;
    return port ? g_strdup_printf("%s · %s", d->description, port) : g_strdup(d->description);
}

static void refresh_section(DevSection *s)
{
    GPtrArray *devs = vp_model_devices(vp->model, s->dir, FALSE);
    VpDevice *def = vp_model_default_device(vp->model, s->dir);
    if (!def && devs->len) def = devs->pdata[0];
    gtk_widget_set_visible(s->controls, def != NULL);
    gtk_widget_set_visible(s->empty, def == NULL);
    if (!def) {
        stop_meter(s);
        g_ptr_array_unref(devs);
        return;
    }

    const char **labels = g_new0(const char *, devs->len + 1);
    g_ptr_array_set_size(s->names, 0);
    guint sel = 0;
    for (guint i = 0; i < devs->len; i++) {
        VpDevice *d = devs->pdata[i];
        labels[i] = device_label(d);
        g_ptr_array_add(s->names, g_strdup(d->name));
        if (d == def) sel = i;
    }
    vp_dropdown_set(s->dd, labels, devs->len, sel);
    for (guint i = 0; i < devs->len; i++) g_free((char *)labels[i]);
    g_free(labels);

    gtk_label_set_text(GTK_LABEL(s->kind), vp_device_glyph(def));
    vp_volume_ctl_update(s->ctl, def->index, &def->volume, def->mute, TRUE);

    /* 电平表：输出设备监听它的 monitor，输入设备直接监听 */
    const char *src = s->dir == VP_OUTPUT ? def->monitor_name : def->name;
    gboolean want = M.active && vp->config->peak_meters && src;
    gtk_widget_set_visible(GTK_WIDGET(s->peak), vp->config->peak_meters);
    if (!want) stop_meter(s);
    else if (!s->meter || g_strcmp0(vp_peak_source(s->meter), src)) {
        stop_meter(s);
        s->meter = vp_peak_new(src, on_peak, s);
    }
    g_ptr_array_unref(devs);
}

static GtkWidget *build_section(DevSection *s, VpDir dir, const char *title, const char *empty)
{
    s->dir = dir;
    s->names = g_ptr_array_new_with_free_func(g_free);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(box), vp_section_title(title));
    s->card = vp_card();
    gtk_box_append(GTK_BOX(box), s->card);

    s->empty = vp_label(empty, "placeholder-small", 0);
    gtk_box_append(GTK_BOX(s->card), s->empty);

    s->controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    s->kind = vp_glyph(dir == VP_OUTPUT ? VP_G_SPEAKER : VP_G_MIC, "kind");
    gtk_box_append(GTK_BOX(top), s->kind);
    s->dd = vp_dropdown_new(G_CALLBACK(on_device_selected), s);
    gtk_widget_set_tooltip_text(GTK_WIDGET(s->dd), dir == VP_OUTPUT ? "选择输出设备" : "选择输入设备");
    gtk_box_append(GTK_BOX(top), GTK_WIDGET(s->dd));
    gtk_box_append(GTK_BOX(s->controls), top);

    s->ctl = vp_volume_ctl_new(FALSE, dir);
    gtk_box_append(GTK_BOX(s->controls), s->ctl->root);

    s->peak = GTK_LEVEL_BAR(gtk_level_bar_new_for_interval(0, 1));
    gtk_level_bar_set_mode(s->peak, GTK_LEVEL_BAR_MODE_CONTINUOUS);
    gtk_level_bar_remove_offset_value(s->peak, GTK_LEVEL_BAR_OFFSET_LOW);
    gtk_level_bar_remove_offset_value(s->peak, GTK_LEVEL_BAR_OFFSET_HIGH);
    gtk_level_bar_remove_offset_value(s->peak, GTK_LEVEL_BAR_OFFSET_FULL);
    gtk_widget_add_css_class(GTK_WIDGET(s->peak), "peak");
    gtk_box_append(GTK_BOX(s->controls), GTK_WIDGET(s->peak));
    gtk_box_append(GTK_BOX(s->card), s->controls);
    return box;
}

/* ───────────── 应用 ───────────── */

static void app_row_free(AppRow *r)
{
    g_free(r->icon);
    g_free(r->move_sig);
    g_free(r);
}

static void on_move_row(GtkListBox *box, GtkListBoxRow *row, AppRow *r)
{
    guint32 dev = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "dev")) - 1;
    vp_pulse_move_stream(VP_OUTPUT, r->index, dev);
    gtk_menu_button_popdown(GTK_MENU_BUTTON(r->move));
}

/* 移动到其他输出设备的菜单；设备列表或当前设备变化时才重建 */
static void update_move_menu(AppRow *r, const VpStream *st, GPtrArray *devs)
{
    gtk_widget_set_visible(r->move, devs->len > 1);
    GString *sig = g_string_new(NULL);
    g_string_append_printf(sig, "%u|", st->device);
    for (guint i = 0; i < devs->len; i++) g_string_append_printf(sig, "%u:%s|", ((VpDevice *)devs->pdata[i])->index,
                                                               ((VpDevice *)devs->pdata[i])->description);
    if (!g_strcmp0(sig->str, r->move_sig)) {
        g_string_free(sig, TRUE);
        return;
    }
    g_free(r->move_sig);
    r->move_sig = g_string_free(sig, FALSE);

    GtkWidget *list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
    gtk_widget_add_css_class(list, "menu");
    for (guint i = 0; i < devs->len; i++) {
        VpDevice *d = devs->pdata[i];
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_append(GTK_BOX(h), vp_glyph(vp_device_glyph(d), "kind"));
        GtkWidget *l = vp_label(d->description, NULL, 0);
        gtk_widget_set_hexpand(l, TRUE);
        gtk_box_append(GTK_BOX(h), l);
        if (d->index == st->device) gtk_box_append(GTK_BOX(h), vp_label("当前", "badge", 1));
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), h);
        g_object_set_data(G_OBJECT(row), "dev", GUINT_TO_POINTER(d->index + 1));
        gtk_list_box_append(GTK_LIST_BOX(list), row);
    }
    g_signal_connect(list, "row-activated", G_CALLBACK(on_move_row), r);
    GtkWidget *pop = gtk_popover_new();
    gtk_popover_set_child(GTK_POPOVER(pop), list);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(r->move), pop);
}

static AppRow *app_row_new(guint32 index)
{
    AppRow *r = g_new0(AppRow, 1);
    r->index = index;
    r->root = vp_card();
    gtk_widget_add_css_class(r->root, "app");
    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    r->icon_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_valign(r->icon_box, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(top), r->icon_box);
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(v, TRUE);
    gtk_widget_set_valign(v, GTK_ALIGN_CENTER);
    r->title = GTK_LABEL(vp_label("", "name", 0));
    gtk_label_set_ellipsize(r->title, PANGO_ELLIPSIZE_END);
    r->sub = GTK_LABEL(vp_label("", "sub", 0));
    gtk_label_set_ellipsize(r->sub, PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(v), GTK_WIDGET(r->title));
    gtk_box_append(GTK_BOX(v), GTK_WIDGET(r->sub));
    gtk_box_append(GTK_BOX(top), v);
    r->move = gtk_menu_button_new();
    gtk_menu_button_set_child(GTK_MENU_BUTTON(r->move), vp_glyph(VP_G_MOVE, NULL));
    gtk_widget_add_css_class(r->move, "icon");
    gtk_widget_set_valign(r->move, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(r->move, "输出到…");
    gtk_box_append(GTK_BOX(top), r->move);
    gtk_box_append(GTK_BOX(r->root), top);
    r->ctl = vp_volume_ctl_new(TRUE, VP_OUTPUT);
    gtk_box_append(GTK_BOX(r->root), r->ctl->root);
    g_object_set_data_full(G_OBJECT(r->root), "vp-row", r, (GDestroyNotify)app_row_free);
    return r;
}

static void update_app_row(AppRow *r, const VpStream *st, GPtrArray *devs)
{
    char *t = vp_stream_title(st), *s = vp_stream_subtitle(st);
    gtk_label_set_text(r->title, t);
    gtk_label_set_text(r->sub, s);
    gtk_widget_set_visible(GTK_WIDGET(r->sub), *s != '\0');
    g_free(t);
    g_free(s);
    if (!r->icon || g_strcmp0(r->icon, st->icon)) {
        g_free(r->icon);
        r->icon = g_strdup(st->icon ? st->icon : "");
        vp_box_clear(r->icon_box);
        gtk_box_append(GTK_BOX(r->icon_box), vp_app_icon(st->icon));
    }
    if (st->corked) gtk_widget_add_css_class(r->root, "paused");
    else gtk_widget_remove_css_class(r->root, "paused");
    vp_volume_ctl_update(r->ctl, st->index, &st->volume, st->mute, st->has_volume);
    update_move_menu(r, st, devs);
}

static void refresh_apps(void)
{
    GPtrArray *streams = vp_model_streams(vp->model, VP_OUTPUT);
    GPtrArray *devs = vp_model_devices(vp->model, VP_OUTPUT, FALSE);
    GHashTable *seen = g_hash_table_new(NULL, NULL);
    GtkWidget *prev = NULL;
    for (guint i = 0; i < streams->len; i++) {
        VpStream *st = streams->pdata[i];
        gpointer key = GUINT_TO_POINTER(st->index + 1);
        AppRow *r = g_hash_table_lookup(M.apps, key);
        if (!r) {
            r = app_row_new(st->index);
            g_hash_table_insert(M.apps, key, r);
            gtk_box_append(GTK_BOX(M.apps_box), r->root);
        }
        update_app_row(r, st, devs);
        gtk_box_reorder_child_after(GTK_BOX(M.apps_box), r->root, prev); /* 按模型的顺序排列 */
        prev = r->root;
        g_hash_table_add(seen, key);
    }
    GHashTableIter it;
    gpointer k, v;
    g_hash_table_iter_init(&it, M.apps);
    while (g_hash_table_iter_next(&it, &k, &v))
        if (!g_hash_table_contains(seen, k)) {
            gtk_box_remove(GTK_BOX(M.apps_box), ((AppRow *)v)->root); /* 行结构随 widget 一起释放 */
            g_hash_table_iter_remove(&it);
        }
    gtk_widget_set_visible(M.apps_empty, streams->len == 0);
    g_hash_table_unref(seen);
    g_ptr_array_unref(devs);
    g_ptr_array_unref(streams);
}

/* ───────────── 入口 ───────────── */

void vp_page_main_refresh(void)
{
    if (!vp->model) return;
    refresh_section(&M.out);
    refresh_section(&M.in);
    refresh_apps();
}

void vp_page_main_set_active(gboolean active)
{
    M.active = active;
    if (!active) {
        stop_meter(&M.out);
        stop_meter(&M.in);
    }
}

static void on_settings(void)
{
    vp_show_page(VP_PAGE_SETTINGS);
}

GtkWidget *vp_page_main_new(void)
{
    M.apps = g_hash_table_new(NULL, NULL);
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *h = vp_header("声音", FALSE);
    gtk_box_append(GTK_BOX(h), vp_icon_button(VP_G_SETTINGS, "声音设置", G_CALLBACK(on_settings), NULL));
    gtk_box_append(GTK_BOX(page), h);

    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(body), build_section(&M.out, VP_OUTPUT, "输出", "没有可用的输出设备"));
    gtk_box_append(GTK_BOX(body), build_section(&M.in, VP_INPUT, "输入", "没有可用的输入设备"));
    gtk_box_append(GTK_BOX(body), vp_section_title("应用"));
    M.apps_empty = vp_label("没有正在播放声音的应用", "placeholder-small", 0);
    gtk_box_append(GTK_BOX(body), M.apps_empty);
    M.apps_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(body), M.apps_box);
    gtk_box_append(GTK_BOX(page), vp_scroller(body, 560));
    return page;
}
