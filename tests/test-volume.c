// SPDX-License-Identifier: GPL-3.0-or-later
#include "vp-volume.h"

#include <string.h>

static pa_cvolume stereo(pa_volume_t l, pa_volume_t r)
{
    pa_cvolume v;
    pa_cvolume_init(&v);
    v.channels = 2;
    v.values[0] = l;
    v.values[1] = r;
    return v;
}

static pa_channel_map stereo_map(void)
{
    pa_channel_map m;
    pa_channel_map_init_stereo(&m);
    return m;
}

static void test_percent(void)
{
    pa_cvolume v = stereo(PA_VOLUME_NORM, PA_VOLUME_NORM);
    g_assert_cmpuint(vp_volume_percent(&v), ==, 100);
    v = stereo(PA_VOLUME_NORM / 2, PA_VOLUME_NORM / 4);
    g_assert_cmpuint(vp_volume_percent(&v), ==, 50); /* 取最大声道 */
    v = stereo(PA_VOLUME_MUTED, PA_VOLUME_MUTED);
    g_assert_cmpuint(vp_volume_percent(&v), ==, 0);
    pa_cvolume invalid;
    pa_cvolume_init(&invalid);
    g_assert_cmpuint(vp_volume_percent(&invalid), ==, 0);
    g_assert_cmpuint(vp_volume_percent(NULL), ==, 0);
}

static void test_set_percent(void)
{
    pa_cvolume v = stereo(PA_VOLUME_NORM, PA_VOLUME_NORM / 2);
    vp_volume_set_percent(&v, 40, 100);
    g_assert_cmpuint(vp_volume_percent(&v), ==, 40);
    /* 比例（平衡）保持不变 */
    g_assert_cmpfloat_with_epsilon((double)v.values[1] / v.values[0], 0.5, 0.01);

    vp_volume_set_percent(&v, 130, 100);
    g_assert_cmpuint(vp_volume_percent(&v), ==, 100); /* 截断到上限 */
    vp_volume_set_percent(&v, 130, 150);
    g_assert_cmpuint(vp_volume_percent(&v), ==, 130);
    vp_volume_set_percent(&v, 500, 500);
    g_assert_cmpuint(vp_volume_percent(&v), ==, VP_VOLUME_MAX_LIMIT); /* 上限本身也受限 */

    /* 从 0 恢复：各声道相同 */
    pa_cvolume z = stereo(0, 0);
    vp_volume_set_percent(&z, 30, 100);
    g_assert_cmpuint(vp_volume_percent(&z), ==, 30);
    g_assert_cmpuint(z.values[0], ==, z.values[1]);

    /* 每个百分比往返一致 */
    for (guint p = 0; p <= 150; p++) {
        pa_cvolume w = stereo(PA_VOLUME_NORM, PA_VOLUME_NORM);
        vp_volume_set_percent(&w, p, 150);
        g_assert_cmpuint(vp_volume_percent(&w), ==, p);
    }
}

static void test_adjust(void)
{
    pa_cvolume v = stereo(PA_VOLUME_NORM / 2, PA_VOLUME_NORM / 2);
    vp_volume_adjust(&v, 5, 100);
    g_assert_cmpuint(vp_volume_percent(&v), ==, 55);
    vp_volume_adjust(&v, -100, 100);
    g_assert_cmpuint(vp_volume_percent(&v), ==, 0);
    vp_volume_adjust(&v, 200, 120);
    g_assert_cmpuint(vp_volume_percent(&v), ==, 120);
}

static void test_level_label(void)
{
    g_assert_cmpuint(vp_volume_level(50, TRUE), ==, 0);
    g_assert_cmpuint(vp_volume_level(0, FALSE), ==, 0);
    g_assert_cmpuint(vp_volume_level(10, FALSE), ==, 1);
    g_assert_cmpuint(vp_volume_level(50, FALSE), ==, 2);
    g_assert_cmpuint(vp_volume_level(90, FALSE), ==, 3);
    g_assert_cmpuint(vp_volume_level(140, FALSE), ==, 3);

    char *s = vp_volume_label(42, FALSE);
    g_assert_cmpstr(s, ==, "42%");
    g_free(s);
    s = vp_volume_label(42, TRUE);
    g_assert_cmpstr(s, ==, "静音");
    g_free(s);

    pa_cvolume v = stereo(PA_VOLUME_NORM, PA_VOLUME_NORM);
    s = vp_volume_db_label(&v);
    g_assert_cmpstr(s, ==, "+0.0 dB");
    g_free(s);
    v = stereo(0, 0);
    s = vp_volume_db_label(&v);
    g_assert_cmpstr(s, ==, "-∞ dB");
    g_free(s);
    v = stereo(pa_sw_volume_from_dB(-12.0), pa_sw_volume_from_dB(-12.0));
    s = vp_volume_db_label(&v);
    g_assert_cmpstr(s, ==, "-12.0 dB");
    g_free(s);
}

static void test_balance(void)
{
    pa_channel_map m = stereo_map();
    g_assert_true(vp_balance_supported(&m));
    pa_cvolume v = stereo(PA_VOLUME_NORM, PA_VOLUME_NORM);
    g_assert_cmpfloat_with_epsilon(vp_balance_get(&v, &m), 0.0, 0.001);

    g_assert_true(vp_balance_set(&v, &m, 0.5));
    g_assert_cmpfloat_with_epsilon(vp_balance_get(&v, &m), 0.5, 0.01);
    g_assert_cmpuint(vp_volume_percent(&v), ==, 100); /* 改平衡不改总音量 */
    g_assert_cmpuint(v.values[1], >, v.values[0]);

    g_assert_true(vp_balance_set(&v, &m, -2.0)); /* 截断到 -1 */
    g_assert_cmpfloat_with_epsilon(vp_balance_get(&v, &m), -1.0, 0.01);

    pa_cvolume z = stereo(0, 0);
    g_assert_false(vp_balance_set(&z, &m, 0.3)); /* 0 音量无法表达平衡 */

    pa_channel_map mono;
    pa_channel_map_init_mono(&mono);
    g_assert_false(vp_balance_supported(&mono));
    pa_cvolume one;
    pa_cvolume_set(&one, 1, PA_VOLUME_NORM);
    g_assert_false(vp_balance_set(&one, &mono, 0.5));
    g_assert_cmpfloat(vp_balance_get(&one, &mono), ==, 0.0);
    /* 声道数不一致 */
    g_assert_false(vp_balance_set(&one, &m, 0.5));
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/volume/percent", test_percent);
    g_test_add_func("/volume/set-percent", test_set_percent);
    g_test_add_func("/volume/adjust", test_adjust);
    g_test_add_func("/volume/level-label", test_level_label);
    g_test_add_func("/volume/balance", test_balance);
    return g_test_run();
}
