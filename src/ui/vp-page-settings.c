// SPDX-License-Identifier: GPL-3.0-or-later
/* 声音设置：四个标签页
 *   输出 / 输入：每个设备的端口、音量、平衡、设为默认
 *   录音：录音应用的音量与输入设备
 *   配置：每张声卡的配置（profile）
 * 行按序号原地更新（同 vp-page-main.c），避免拖动中的滑块被刷新打断。
 */
#include "vp-app.h"
#include "vp-pages.h"
#include "vp-pulse.h"
#include "vp-volume.h"
#include "vp-widgets.h"

#include <math.h>

#define HOLD_US (600 * G_TIME_SPAN_MILLISECOND)

typedef struct {
    GtkWidget *box, *empty;
    GHashTable *rows; /* index+1 → 行结构（随行的 root widget 一起释放） */
} List;

typedef struct {
    GtkWidget *root;
    VpDir dir;
    guint32 index;
    char *name;
    GtkWidget *kind, *make_default, *ports_field, *balance_row;
    GtkLabel *title, *sub;
    GtkDropDown *ports;
    GPtrArray *port_names;
    GtkScale *balance;
    gboolean balance_updating;
    gint64 balance_hold;
    VpVolumeCtl *ctl;
} DeviceRow;

typedef struct {
    GtkWidget *root;
    guint32 index;
    GtkWidget *icon_box;
    GtkLabel *title, *sub;
    GtkDropDown *device;
    GArray *device_idx;
    VpVolumeCtl *ctl;
    char *icon;
} RecRow;

typedef struct {
    GtkWidget *root;
    guint32 index;
    GtkLabel *title, *sub;
    GtkDropDown *profile;
    GPtrArray *profile_names;
} CardRow;

static struct {
    GtkStack *tabs;
    List out, in, rec, cards;
} S;

/* ───────────── 通用 ───────────── */

static List list_new(GtkWidget *parent, const char *empty_text)
{
    List l = { 0 };
    l.empty = vp_label(empty_text, "placeholder-small", 0);
    gtk_box_append(GTK_BOX(parent), l.empty);
    l.box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(parent), l.box);
    l.rows = g_hash_table_new(NULL, NULL);
    return l;
}

/* 每种行结构的第一个成员都是 root，因此可以统一处理 */
static void list_finish(List *l, GHashTable *seen)
{
    GHashTableIter it;
    gpointer k, v;
    g_hash_table_iter_init(&it, l->rows);
    while (g_hash_table_iter_next(&it, &k, &v))
        if (!g_hash_table_contains(seen, k)) {
            gtk_box_remove(GTK_BOX(l->box), *(GtkWidget **)v);
            g_hash_table_iter_remove(&it);
        }
    gtk_widget_set_visible(l->empty, g_hash_table_size(l->rows) == 0);
}

static GtkWidget *field(GtkWidget *parent, const char *label, GtkWidget *input)
{
    GtkWidget *f = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *l = vp_label(label, "key", 0);
    gtk_label_set_width_chars(GTK_LABEL(l), 4);
    gtk_box_append(GTK_BOX(f), l);
    gtk_box_append(GTK_BOX(f), input);
    gtk_box_append(GTK_BOX(parent), f);
    return f;
}

/* ───────────── 设备 ───────────── */

static void device_row_free(DeviceRow *r)
{
    g_free(r->name);
    g_ptr_array_unref(r->port_names);
    g_free(r);
}

static void on_make_default(DeviceRow *r)
{
    vp_pulse_set_default(r->dir, r->name);
}

static void on_port(GObject *dd, GParamSpec *ps, DeviceRow *r)
{
    if (g_object_get_data(dd, "vp-updating")) return;
    guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    if (i < r->port_names->len) vp_pulse_set_port(r->dir, r->index, r->port_names->pdata[i]);
}

static void on_balance(GtkRange *range, DeviceRow *r)
{
    if (r->balance_updating) return;
    VpDevice *d = vp_model_device(vp->model, r->dir, r->index);
    if (!d) return;
    pa_cvolume v = d->volume;
    if (!vp_balance_set(&v, &d->map, (float)(gtk_range_get_value(range) / 100.0))) return;
    r->balance_hold = g_get_monotonic_time() + HOLD_US;
    vp_pulse_set_device_volume(r->dir, r->index, &v);
}

static DeviceRow *device_row_new(VpDir dir, guint32 index)
{
    DeviceRow *r = g_new0(DeviceRow, 1);
    r->dir = dir;
    r->index = index;
    r->port_names = g_ptr_array_new_with_free_func(g_free);
    r->root = vp_card();

    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    r->kind = vp_glyph(VP_G_SPEAKER, "kind");
    gtk_box_append(GTK_BOX(top), r->kind);
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(v, TRUE);
    gtk_widget_set_valign(v, GTK_ALIGN_CENTER);
    r->title = GTK_LABEL(vp_label("", "name", 0));
    gtk_label_set_ellipsize(r->title, PANGO_ELLIPSIZE_END);
    r->sub = GTK_LABEL(vp_label("", "sub", 0));
    gtk_box_append(GTK_BOX(v), GTK_WIDGET(r->title));
    gtk_box_append(GTK_BOX(v), GTK_WIDGET(r->sub));
    gtk_box_append(GTK_BOX(top), v);
    r->make_default = gtk_button_new_with_label("设为默认");
    gtk_widget_add_css_class(r->make_default, "small");
    gtk_widget_set_valign(r->make_default, GTK_ALIGN_CENTER);
    g_signal_connect_swapped(r->make_default, "clicked", G_CALLBACK(on_make_default), r);
    gtk_box_append(GTK_BOX(top), r->make_default);
    gtk_box_append(GTK_BOX(r->root), top);

    r->ports = vp_dropdown_new(G_CALLBACK(on_port), r);
    r->ports_field = field(r->root, "端口", GTK_WIDGET(r->ports));

    r->ctl = vp_volume_ctl_new(FALSE, dir);
    gtk_box_append(GTK_BOX(r->root), r->ctl->root);

    r->balance = GTK_SCALE(gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -100, 100, 1));
    gtk_scale_set_draw_value(r->balance, FALSE);
    gtk_scale_add_mark(r->balance, 0, GTK_POS_BOTTOM, NULL);
    gtk_range_set_increments(GTK_RANGE(r->balance), 1, 10);
    gtk_widget_set_hexpand(GTK_WIDGET(r->balance), TRUE);
    g_signal_connect(r->balance, "value-changed", G_CALLBACK(on_balance), r);
    r->balance_row = field(r->root, "平衡", GTK_WIDGET(r->balance));

    g_object_set_data_full(G_OBJECT(r->root), "vp-row", r, (GDestroyNotify)device_row_free);
    return r;
}

static void device_row_update(DeviceRow *r, const VpDevice *d)
{
    gboolean is_default = vp_model_is_default(vp->model, d);
    g_free(r->name);
    r->name = g_strdup(d->name);
    gtk_label_set_text(r->title, d->description);
    gtk_label_set_text(GTK_LABEL(r->kind), vp_device_glyph(d));
    gtk_label_set_text(r->sub, is_default ? "默认设备" : "");
    gtk_widget_set_visible(GTK_WIDGET(r->sub), is_default);
    gtk_widget_set_visible(r->make_default, !is_default);
    if (is_default) gtk_widget_add_css_class(r->root, "current");
    else gtk_widget_remove_css_class(r->root, "current");

    /* 端口 */
    GPtrArray *ports = vp_device_ports_sorted(d);
    gtk_widget_set_visible(r->ports_field, ports->len > 1);
    const char **labels = g_new0(const char *, ports->len + 1);
    g_ptr_array_set_size(r->port_names, 0);
    guint sel = 0;
    for (guint i = 0; i < ports->len; i++) {
        VpPort *p = ports->pdata[i];
        labels[i] = p->available ? g_strdup(p->description) : g_strdup_printf("%s（未插入）", p->description);
        g_ptr_array_add(r->port_names, g_strdup(p->name));
        if (!g_strcmp0(p->name, d->active_port)) sel = i;
    }
    vp_dropdown_set(r->ports, labels, ports->len, sel);
    for (guint i = 0; i < ports->len; i++) g_free((char *)labels[i]);
    g_free(labels);
    g_ptr_array_unref(ports);

    vp_volume_ctl_update(r->ctl, d->index, &d->volume, d->mute, TRUE);

    /* 平衡 */
    gboolean can = vp_balance_supported(&d->map) && vp_volume_percent(&d->volume) > 0;
    gtk_widget_set_visible(r->balance_row, vp_balance_supported(&d->map));
    gtk_widget_set_sensitive(r->balance_row, can);
    if (g_get_monotonic_time() >= r->balance_hold) {
        r->balance_updating = TRUE;
        gtk_range_set_value(GTK_RANGE(r->balance), round(vp_balance_get(&d->volume, &d->map) * 100.0));
        r->balance_updating = FALSE;
    }
}

static void refresh_devices(List *l, VpDir dir)
{
    GPtrArray *devs = vp_model_devices(vp->model, dir, FALSE);
    GHashTable *seen = g_hash_table_new(NULL, NULL);
    GtkWidget *prev = NULL;
    for (guint i = 0; i < devs->len; i++) {
        VpDevice *d = devs->pdata[i];
        gpointer key = GUINT_TO_POINTER(d->index + 1);
        DeviceRow *r = g_hash_table_lookup(l->rows, key);
        if (!r) {
            r = device_row_new(dir, d->index);
            g_hash_table_insert(l->rows, key, r);
            gtk_box_append(GTK_BOX(l->box), r->root);
        }
        device_row_update(r, d);
        gtk_box_reorder_child_after(GTK_BOX(l->box), r->root, prev);
        prev = r->root;
        g_hash_table_add(seen, key);
    }
    list_finish(l, seen);
    g_hash_table_unref(seen);
    g_ptr_array_unref(devs);
}

/* ───────────── 录音应用 ───────────── */

static void rec_row_free(RecRow *r)
{
    g_array_unref(r->device_idx);
    g_free(r->icon);
    g_free(r);
}

static void on_rec_device(GObject *dd, GParamSpec *ps, RecRow *r)
{
    if (g_object_get_data(dd, "vp-updating")) return;
    guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    if (i < r->device_idx->len) vp_pulse_move_stream(VP_INPUT, r->index, g_array_index(r->device_idx, guint32, i));
}

static RecRow *rec_row_new(guint32 index)
{
    RecRow *r = g_new0(RecRow, 1);
    r->index = index;
    r->device_idx = g_array_new(FALSE, FALSE, sizeof(guint32));
    r->root = vp_card();
    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    r->icon_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_valign(r->icon_box, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(top), r->icon_box);
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(v, TRUE);
    r->title = GTK_LABEL(vp_label("", "name", 0));
    gtk_label_set_ellipsize(r->title, PANGO_ELLIPSIZE_END);
    r->sub = GTK_LABEL(vp_label("", "sub", 0));
    gtk_label_set_ellipsize(r->sub, PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(v), GTK_WIDGET(r->title));
    gtk_box_append(GTK_BOX(v), GTK_WIDGET(r->sub));
    gtk_box_append(GTK_BOX(top), v);
    gtk_box_append(GTK_BOX(r->root), top);
    r->device = vp_dropdown_new(G_CALLBACK(on_rec_device), r);
    field(r->root, "输入", GTK_WIDGET(r->device));
    r->ctl = vp_volume_ctl_new(TRUE, VP_INPUT);
    gtk_box_append(GTK_BOX(r->root), r->ctl->root);
    g_object_set_data_full(G_OBJECT(r->root), "vp-row", r, (GDestroyNotify)rec_row_free);
    return r;
}

static void rec_row_update(RecRow *r, const VpStream *st, GPtrArray *devs)
{
    char *t = vp_stream_title(st), *s = vp_stream_subtitle(st);
    gtk_label_set_text(r->title, t);
    gtk_label_set_text(r->sub, s);
    gtk_widget_set_visible(GTK_WIDGET(r->sub), *s != '\0');
    g_free(t);
    g_free(s);
    if (!r->icon || g_strcmp0(r->icon, st->icon ? st->icon : "")) {
        g_free(r->icon);
        r->icon = g_strdup(st->icon ? st->icon : "");
        vp_box_clear(r->icon_box);
        gtk_box_append(GTK_BOX(r->icon_box), vp_app_icon(st->icon));
    }
    const char **labels = g_new0(const char *, devs->len + 1);
    g_array_set_size(r->device_idx, 0);
    guint sel = 0;
    for (guint i = 0; i < devs->len; i++) {
        VpDevice *d = devs->pdata[i];
        labels[i] = d->description;
        g_array_append_val(r->device_idx, d->index);
        if (d->index == st->device) sel = i;
    }
    vp_dropdown_set(r->device, labels, devs->len, sel);
    g_free(labels);
    vp_volume_ctl_update(r->ctl, st->index, &st->volume, st->mute, st->has_volume);
}

static void refresh_recording(void)
{
    GPtrArray *streams = vp_model_streams(vp->model, VP_INPUT);
    GPtrArray *devs = vp_model_devices(vp->model, VP_INPUT, FALSE);
    GHashTable *seen = g_hash_table_new(NULL, NULL);
    GtkWidget *prev = NULL;
    for (guint i = 0; i < streams->len; i++) {
        VpStream *st = streams->pdata[i];
        gpointer key = GUINT_TO_POINTER(st->index + 1);
        RecRow *r = g_hash_table_lookup(S.rec.rows, key);
        if (!r) {
            r = rec_row_new(st->index);
            g_hash_table_insert(S.rec.rows, key, r);
            gtk_box_append(GTK_BOX(S.rec.box), r->root);
        }
        rec_row_update(r, st, devs);
        gtk_box_reorder_child_after(GTK_BOX(S.rec.box), r->root, prev);
        prev = r->root;
        g_hash_table_add(seen, key);
    }
    list_finish(&S.rec, seen);
    g_hash_table_unref(seen);
    g_ptr_array_unref(devs);
    g_ptr_array_unref(streams);
}

/* ───────────── 声卡配置 ───────────── */

static void card_row_free(CardRow *r)
{
    g_ptr_array_unref(r->profile_names);
    g_free(r);
}

static void on_profile(GObject *dd, GParamSpec *ps, CardRow *r)
{
    if (g_object_get_data(dd, "vp-updating")) return;
    guint i = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
    if (i < r->profile_names->len) vp_pulse_set_profile(r->index, r->profile_names->pdata[i]);
}

static CardRow *card_row_new(guint32 index)
{
    CardRow *r = g_new0(CardRow, 1);
    r->index = index;
    r->profile_names = g_ptr_array_new_with_free_func(g_free);
    r->root = vp_card();
    r->title = GTK_LABEL(vp_label("", "name", 0));
    gtk_label_set_ellipsize(r->title, PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(r->root), GTK_WIDGET(r->title));
    r->sub = GTK_LABEL(vp_label("", "sub", 0));
    gtk_label_set_ellipsize(r->sub, PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_append(GTK_BOX(r->root), GTK_WIDGET(r->sub));
    r->profile = vp_dropdown_new(G_CALLBACK(on_profile), r);
    field(r->root, "配置", GTK_WIDGET(r->profile));
    g_object_set_data_full(G_OBJECT(r->root), "vp-row", r, (GDestroyNotify)card_row_free);
    return r;
}

static void card_row_update(CardRow *r, const VpCard *c)
{
    gtk_label_set_text(r->title, c->description);
    gtk_label_set_text(r->sub, c->name);
    GPtrArray *profiles = vp_card_profiles_sorted(c);
    const char **labels = g_new0(const char *, profiles->len + 1);
    g_ptr_array_set_size(r->profile_names, 0);
    guint sel = 0;
    for (guint i = 0; i < profiles->len; i++) {
        VpProfile *p = profiles->pdata[i];
        labels[i] = p->available ? g_strdup(p->description) : g_strdup_printf("%s（不可用）", p->description);
        g_ptr_array_add(r->profile_names, g_strdup(p->name));
        if (!g_strcmp0(p->name, c->active_profile)) sel = i;
    }
    vp_dropdown_set(r->profile, labels, profiles->len, sel);
    for (guint i = 0; i < profiles->len; i++) g_free((char *)labels[i]);
    g_free(labels);
    g_ptr_array_unref(profiles);
}

static void refresh_cards(void)
{
    GPtrArray *cards = vp_model_cards(vp->model);
    GHashTable *seen = g_hash_table_new(NULL, NULL);
    GtkWidget *prev = NULL;
    for (guint i = 0; i < cards->len; i++) {
        VpCard *c = cards->pdata[i];
        gpointer key = GUINT_TO_POINTER(c->index + 1);
        CardRow *r = g_hash_table_lookup(S.cards.rows, key);
        if (!r) {
            r = card_row_new(c->index);
            g_hash_table_insert(S.cards.rows, key, r);
            gtk_box_append(GTK_BOX(S.cards.box), r->root);
        }
        card_row_update(r, c);
        gtk_box_reorder_child_after(GTK_BOX(S.cards.box), r->root, prev);
        prev = r->root;
        g_hash_table_add(seen, key);
    }
    list_finish(&S.cards, seen);
    g_hash_table_unref(seen);
    g_ptr_array_unref(cards);
}

/* ───────────── 入口 ───────────── */

void vp_page_settings_refresh(void)
{
    if (!vp->model || !S.tabs) return;
    /* 只刷新当前标签页；切换标签时会再刷新一次 */
    const char *tab = gtk_stack_get_visible_child_name(S.tabs);
    if (!g_strcmp0(tab, "out")) refresh_devices(&S.out, VP_OUTPUT);
    else if (!g_strcmp0(tab, "in")) refresh_devices(&S.in, VP_INPUT);
    else if (!g_strcmp0(tab, "rec")) refresh_recording();
    else refresh_cards();
}

static void on_tab(GtkToggleButton *b, const char *name)
{
    if (!gtk_toggle_button_get_active(b)) return;
    gtk_stack_set_visible_child_name(S.tabs, name);
    vp_page_settings_refresh();
}

static GtkWidget *tab_page(const char *name, const char *empty, List *l)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    *l = list_new(box, empty);
    gtk_stack_add_named(S.tabs, vp_scroller(box, 540), name);
    return box;
}

GtkWidget *vp_page_settings_new(void)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(page), vp_header("声音设置", TRUE));

    static const char *const names[] = { "out", "in", "rec", "cards" };
    static const char *const titles[] = { "输出", "输入", "录音", "配置" };
    GtkWidget *seg = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(seg, "segmented");
    gtk_widget_set_margin_end(seg, 8);
    gtk_box_set_homogeneous(GTK_BOX(seg), TRUE);
    GtkToggleButton *first = NULL;
    for (guint i = 0; i < G_N_ELEMENTS(names); i++) {
        GtkWidget *b = gtk_toggle_button_new_with_label(titles[i]);
        if (first) gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(b), first);
        else first = GTK_TOGGLE_BUTTON(b);
        g_signal_connect(b, "toggled", G_CALLBACK(on_tab), (gpointer)names[i]);
        gtk_box_append(GTK_BOX(seg), b);
    }
    gtk_box_append(GTK_BOX(page), seg);

    S.tabs = GTK_STACK(gtk_stack_new());
    gtk_stack_set_vhomogeneous(S.tabs, FALSE);
    gtk_stack_set_hhomogeneous(S.tabs, TRUE);
    tab_page("out", "没有输出设备", &S.out);
    tab_page("in", "没有输入设备", &S.in);
    tab_page("rec", "没有正在录音的应用", &S.rec);
    tab_page("cards", "没有声卡", &S.cards);
    gtk_box_append(GTK_BOX(page), GTK_WIDGET(S.tabs));
    gtk_toggle_button_set_active(first, TRUE);
    return page;
}
