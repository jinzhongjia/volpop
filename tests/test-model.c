// SPDX-License-Identifier: GPL-3.0-or-later
#include "vp-model.h"

static VpDevice *dev(VpDir dir, guint32 idx, const char *name, const char *desc)
{
    return vp_device_new(dir, idx, name, desc);
}

static void test_put_remove(void)
{
    VpModel *m = vp_model_new("dev.volpop.Volpop");
    vp_model_put_device(m, dev(VP_OUTPUT, 0, "s0", "Speakers")); /* 序号 0 合法 */
    vp_model_put_device(m, dev(VP_OUTPUT, 1, "s1", "HDMI"));
    vp_model_put_device(m, dev(VP_INPUT, 0, "m0", "Mic"));
    g_assert_nonnull(vp_model_device(m, VP_OUTPUT, 0));
    g_assert_cmpstr(vp_model_device(m, VP_INPUT, 0)->name, ==, "m0"); /* 方向分开存放 */

    /* 替换同序号 */
    vp_model_put_device(m, dev(VP_OUTPUT, 1, "s1", "HDMI renamed"));
    g_assert_cmpstr(vp_model_device(m, VP_OUTPUT, 1)->description, ==, "HDMI renamed");

    g_assert_true(vp_model_remove_device(m, VP_OUTPUT, 0));
    g_assert_false(vp_model_remove_device(m, VP_OUTPUT, 0));
    g_assert_null(vp_model_device(m, VP_OUTPUT, 0));

    vp_model_put_card(m, vp_card_new(3, "alsa_card", "Built-in"));
    g_assert_nonnull(vp_model_card(m, 3));
    g_assert_true(vp_model_remove_card(m, 3));

    vp_model_put_stream(m, vp_stream_new(VP_OUTPUT, 7, 1));
    g_assert_nonnull(vp_model_stream(m, VP_OUTPUT, 7));
    g_assert_true(vp_model_remove_stream(m, VP_OUTPUT, 7));

    vp_model_set_default(m, VP_OUTPUT, "s1");
    vp_model_clear(m);
    g_assert_null(vp_model_default_device(m, VP_OUTPUT));
    g_assert_null(m->default_name[VP_OUTPUT]);
    vp_model_free(m);
}

static void test_devices_sorted(void)
{
    VpModel *m = vp_model_new(NULL);
    vp_model_put_device(m, dev(VP_OUTPUT, 1, "b", "Bravo"));
    vp_model_put_device(m, dev(VP_OUTPUT, 2, "a", "Alpha"));
    vp_model_put_device(m, dev(VP_OUTPUT, 3, "c", "Charlie"));
    vp_model_set_default(m, VP_OUTPUT, "c");

    GPtrArray *d = vp_model_devices(m, VP_OUTPUT, FALSE);
    g_assert_cmpuint(d->len, ==, 3);
    g_assert_cmpstr(((VpDevice *)d->pdata[0])->name, ==, "c"); /* 默认设备在最前 */
    g_assert_cmpstr(((VpDevice *)d->pdata[1])->name, ==, "a");
    g_assert_cmpstr(((VpDevice *)d->pdata[2])->name, ==, "b");
    g_ptr_array_unref(d);
    g_assert_cmpstr(vp_model_default_device(m, VP_OUTPUT)->name, ==, "c");
    g_assert_true(vp_model_is_default(m, vp_model_device(m, VP_OUTPUT, 3)));
    g_assert_false(vp_model_is_default(m, vp_model_device(m, VP_OUTPUT, 1)));
    g_assert_false(vp_model_is_default(m, NULL));

    /* 监听源默认隐藏 */
    VpDevice *mon = dev(VP_INPUT, 5, "a.monitor", "Monitor of Alpha");
    mon->is_monitor = TRUE;
    vp_model_put_device(m, mon);
    vp_model_put_device(m, dev(VP_INPUT, 6, "mic", "Mic"));
    d = vp_model_devices(m, VP_INPUT, FALSE);
    g_assert_cmpuint(d->len, ==, 1);
    g_ptr_array_unref(d);
    d = vp_model_devices(m, VP_INPUT, TRUE);
    g_assert_cmpuint(d->len, ==, 2);
    g_ptr_array_unref(d);
    vp_model_free(m);
}

static VpStream *stream(VpDir dir, guint32 idx, const char *app, const char *media, const char *app_id)
{
    VpStream *s = vp_stream_new(dir, idx, 0);
    s->app = g_strdup(app);
    s->media = g_strdup(media);
    s->app_id = g_strdup(app_id);
    return s;
}

static void test_streams(void)
{
    VpModel *m = vp_model_new("dev.volpop.Volpop");
    vp_model_put_stream(m, stream(VP_OUTPUT, 1, "mpv", "song.flac", NULL));
    vp_model_put_stream(m, stream(VP_OUTPUT, 2, "Firefox", "YouTube", "org.mozilla.firefox"));
    vp_model_put_stream(m, stream(VP_OUTPUT, 3, NULL, "notification", NULL));
    vp_model_put_stream(m, stream(VP_INPUT, 4, "volpop", "peak", "dev.volpop.Volpop")); /* 自己的电平表 */
    vp_model_put_stream(m, stream(VP_INPUT, 5, "pavucontrol", "peak", "org.PulseAudio.pavucontrol"));
    vp_model_put_stream(m, stream(VP_INPUT, 6, "OBS", "Mic capture", NULL));

    GPtrArray *s = vp_model_streams(m, VP_OUTPUT);
    g_assert_cmpuint(s->len, ==, 3);
    char *t0 = vp_stream_title(s->pdata[0]);
    g_assert_cmpstr(t0, ==, "Firefox");
    g_free(t0);
    g_ptr_array_unref(s);

    s = vp_model_streams(m, VP_INPUT);
    g_assert_cmpuint(s->len, ==, 1);
    g_assert_cmpuint(((VpStream *)s->pdata[0])->index, ==, 6);
    g_ptr_array_unref(s);
    vp_model_free(m);
}

static void test_stream_titles(void)
{
    VpStream *s = stream(VP_OUTPUT, 1, "mpv", "song.flac", NULL);
    char *t = vp_stream_title(s), *sub = vp_stream_subtitle(s);
    g_assert_cmpstr(t, ==, "mpv");
    g_assert_cmpstr(sub, ==, "song.flac");
    g_free(t);
    g_free(sub);
    vp_stream_free(s);

    s = stream(VP_OUTPUT, 1, "Same", "Same", NULL);
    sub = vp_stream_subtitle(s);
    g_assert_cmpstr(sub, ==, "");
    g_free(sub);
    vp_stream_free(s);

    s = stream(VP_OUTPUT, 1, NULL, "only media", NULL);
    t = vp_stream_title(s);
    sub = vp_stream_subtitle(s);
    g_assert_cmpstr(t, ==, "only media");
    g_assert_cmpstr(sub, ==, "");
    g_free(t);
    g_free(sub);
    vp_stream_free(s);

    s = stream(VP_OUTPUT, 1, NULL, NULL, NULL);
    t = vp_stream_title(s);
    g_assert_cmpstr(t, ==, "未知应用");
    g_free(t);
    vp_stream_free(s);
}

static void test_kind(void)
{
    VpDevice *d = dev(VP_OUTPUT, 1, "alsa_output.pci.analog-stereo", "Speakers");
    g_assert_cmpint(vp_device_kind(d), ==, VP_KIND_SPEAKER);
    d->active_port = g_strdup("analog-output-headphones");
    g_assert_cmpint(vp_device_kind(d), ==, VP_KIND_HEADPHONES);
    g_free(d->active_port);
    d->active_port = g_strdup("hdmi-output-0");
    g_assert_cmpint(vp_device_kind(d), ==, VP_KIND_HDMI);
    d->form_factor = g_strdup("headset");
    g_assert_cmpint(vp_device_kind(d), ==, VP_KIND_HEADSET);
    d->bus = g_strdup("bluetooth");
    g_assert_cmpint(vp_device_kind(d), ==, VP_KIND_BLUETOOTH); /* 蓝牙优先 */
    vp_device_free(d);

    d = dev(VP_INPUT, 2, "alsa_input.usb", "USB Mic");
    d->bus = g_strdup("usb");
    g_assert_cmpint(vp_device_kind(d), ==, VP_KIND_MICROPHONE);
    vp_device_free(d);
    d = dev(VP_OUTPUT, 3, "alsa_output.usb", "USB DAC");
    d->bus = g_strdup("usb");
    g_assert_cmpint(vp_device_kind(d), ==, VP_KIND_USB);
    vp_device_free(d);
}

static void test_ports(void)
{
    VpDevice *d = dev(VP_OUTPUT, 1, "s", "S");
    g_ptr_array_add(d->ports, vp_port_new("speaker", "扬声器", 100, TRUE));
    g_ptr_array_add(d->ports, vp_port_new("headphones", "耳机", 200, FALSE));
    g_ptr_array_add(d->ports, vp_port_new("line", "线路输出", 300, TRUE));
    d->active_port = g_strdup("speaker");
    g_assert_cmpstr(vp_device_active_port_label(d), ==, "扬声器");

    GPtrArray *p = vp_device_ports_sorted(d);
    g_assert_cmpstr(((VpPort *)p->pdata[0])->name, ==, "line");       /* 可用 + 优先级最高 */
    g_assert_cmpstr(((VpPort *)p->pdata[1])->name, ==, "speaker");
    g_assert_cmpstr(((VpPort *)p->pdata[2])->name, ==, "headphones"); /* 未插入的放最后 */
    g_ptr_array_unref(p);

    g_free(d->active_port);
    d->active_port = g_strdup("gone");
    g_assert_null(vp_device_active_port_label(d));
    vp_device_free(d);

    /* 没有描述时用名称 */
    VpPort *bare = vp_port_new("x", NULL, 0, TRUE);
    g_assert_cmpstr(bare->description, ==, "x");
    vp_port_free(bare);
}

static void test_profiles(void)
{
    VpCard *c = vp_card_new(1, "card", NULL);
    g_assert_cmpstr(c->description, ==, "card");
    g_ptr_array_add(c->profiles, vp_profile_new("off", "关闭", 0, TRUE));
    g_ptr_array_add(c->profiles, vp_profile_new("hdmi", "HDMI", 5900, FALSE));
    g_ptr_array_add(c->profiles, vp_profile_new("analog", "模拟立体声", 6500, TRUE));
    g_ptr_array_add(c->profiles, vp_profile_new("pro", "Pro Audio", 1, TRUE));
    GPtrArray *p = vp_card_profiles_sorted(c);
    const char *order[] = { "analog", "pro", "hdmi", "off" };
    for (guint i = 0; i < G_N_ELEMENTS(order); i++) g_assert_cmpstr(((VpProfile *)p->pdata[i])->name, ==, order[i]);
    g_ptr_array_unref(p);
    vp_card_free(c);
}

static void test_cards_sorted(void)
{
    VpModel *m = vp_model_new(NULL);
    vp_model_put_card(m, vp_card_new(1, "b", "Zeta"));
    vp_model_put_card(m, vp_card_new(2, "a", "Alpha"));
    GPtrArray *c = vp_model_cards(m);
    g_assert_cmpstr(((VpCard *)c->pdata[0])->description, ==, "Alpha");
    g_ptr_array_unref(c);
    vp_model_free(m);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/model/put-remove", test_put_remove);
    g_test_add_func("/model/devices-sorted", test_devices_sorted);
    g_test_add_func("/model/streams", test_streams);
    g_test_add_func("/model/stream-titles", test_stream_titles);
    g_test_add_func("/model/kind", test_kind);
    g_test_add_func("/model/ports", test_ports);
    g_test_add_func("/model/profiles", test_profiles);
    g_test_add_func("/model/cards-sorted", test_cards_sorted);
    return g_test_run();
}
