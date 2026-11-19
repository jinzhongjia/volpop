// SPDX-License-Identifier: GPL-3.0-or-later
#include "vp-config.h"

#include <glib/gstdio.h>
#include <string.h>

static void test_defaults(void)
{
    g_autoptr(VpConfig) c = vp_config_new();
    g_assert_null(c->colors_css);
    g_assert_true(g_str_has_suffix(c->style_css, "volpop/style.css"));
    g_assert_true(c->anchor_top && c->anchor_right);
    g_assert_cmpint(c->max_volume, ==, 100);
    g_assert_true(c->peak_meters && c->hide_on_focus_loss);
    char *p = vp_config_default_path();
    g_assert_true(g_str_has_suffix(p, "volpop/volpop.conf"));
    g_free(p);
}

static void test_load(void)
{
    const char *data = "[appearance]\ncolors = ~/c.css\nwidth = 420\n"
                       "[popup]\nanchor = bottom\nmargin-bottom = 10\nhide-on-focus-loss = false\n"
                       "[audio]\nmax-volume = 150\npeak-meters = false\n";
    g_autoptr(VpConfig) c = vp_config_new();
    GError *e = NULL;
    g_assert_true(vp_config_load_data(c, data, -1, &e));
    g_assert_no_error(e);
    char *expect = g_build_filename(g_get_home_dir(), "c.css", NULL);
    g_assert_cmpstr(c->colors_css, ==, expect);
    g_free(expect);
    g_assert_cmpint(c->width, ==, 420);
    g_assert_true(c->anchor_bottom && !c->anchor_top && !c->anchor_left && !c->anchor_right);
    g_assert_cmpint(c->margin_bottom, ==, 10);
    g_assert_false(c->hide_on_focus_loss);
    g_assert_cmpint(c->max_volume, ==, 150);
    g_assert_false(c->peak_meters);
}

static void test_errors(void)
{
    const char *bad[] = {
        "[audio]\nmax-volume = 99\n", "[audio]\nmax-volume = 151\n", "[audio]\npeak-meters = sometimes\n",
        "[popup]\nanchor = nowhere\n", "[popup]\nmargin-top = x\n", "garbage",
    };
    for (guint i = 0; i < G_N_ELEMENTS(bad); i++) {
        g_autoptr(VpConfig) c = vp_config_new();
        GError *e = NULL;
        g_assert_false(vp_config_load_data(c, bad[i], -1, &e));
        g_assert_nonnull(e);
        g_clear_error(&e);
    }
}

static void test_file(void)
{
    g_autoptr(VpConfig) c = vp_config_new();
    GError *e = NULL;
    g_assert_true(vp_config_load_file(c, "/nonexistent/volpop.conf", &e));
    g_assert_no_error(e);

    char *dir = g_dir_make_tmp("volpop-test-XXXXXX", NULL);
    char *path = g_build_filename(dir, "volpop.conf", NULL);
    g_assert_true(g_file_set_contents(path, "[audio]\nmax-volume = 120\n", -1, NULL));
    g_assert_true(vp_config_load_file(c, path, &e));
    g_assert_cmpint(c->max_volume, ==, 120);
    g_assert_true(g_file_set_contents(path, "[audio]\nmax-volume = 1\n", -1, NULL));
    g_assert_false(vp_config_load_file(c, path, &e));
    g_assert_nonnull(strstr(e->message, path));
    g_clear_error(&e);
    g_unlink(path);
    g_rmdir(dir);
    g_free(path);
    g_free(dir);
}

static void test_expand_home(void)
{
    char *p = vp_expand_home("~/x");
    char *expect = g_build_filename(g_get_home_dir(), "x", NULL);
    g_assert_cmpstr(p, ==, expect);
    g_free(p);
    g_free(expect);
    p = vp_expand_home("/abs");
    g_assert_cmpstr(p, ==, "/abs");
    g_free(p);
    g_assert_null(vp_expand_home(NULL));
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/config/defaults", test_defaults);
    g_test_add_func("/config/load", test_load);
    g_test_add_func("/config/errors", test_errors);
    g_test_add_func("/config/file", test_file);
    g_test_add_func("/config/expand-home", test_expand_home);
    return g_test_run();
}
