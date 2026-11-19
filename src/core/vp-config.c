// SPDX-License-Identifier: GPL-3.0-or-later
#include "vp-config.h"
#include "vp-volume.h"

G_DEFINE_QUARK(vp-config-error-quark, vp_config_error)

VpConfig *vp_config_new(void)
{
    VpConfig *c = g_new0(VpConfig, 1);
    c->style_css = g_build_filename(g_get_user_config_dir(), "volpop", "style.css", NULL);
    c->width = 368;
    c->anchor_top = c->anchor_right = TRUE;
    c->margin_top = 6;
    c->margin_right = 8;
    c->hide_on_focus_loss = TRUE;
    c->max_volume = 100;
    c->peak_meters = TRUE;
    return c;
}

void vp_config_free(VpConfig *c)
{
    if (!c) return;
    g_free(c->colors_css);
    g_free(c->style_css);
    g_free(c);
}

char *vp_config_default_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "volpop", "volpop.conf", NULL);
}

char *vp_expand_home(const char *path)
{
    if (!path) return NULL;
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0'))
        return g_build_filename(g_get_home_dir(), path[1] ? path + 2 : "", NULL);
    return g_strdup(path);
}

gboolean vp_config_set_anchor(VpConfig *c, const char *a, GError **error)
{
    static const struct { const char *name; gboolean t, b, l, r; } map[] = {
        { "top-left", 1, 0, 1, 0 },    { "top", 1, 0, 0, 0 },    { "top-right", 1, 0, 0, 1 },
        { "bottom-left", 0, 1, 1, 0 }, { "bottom", 0, 1, 0, 0 }, { "bottom-right", 0, 1, 0, 1 },
        { "center", 0, 0, 0, 0 },      { "left", 0, 0, 1, 0 },   { "right", 0, 0, 0, 1 },
    };
    for (guint i = 0; i < G_N_ELEMENTS(map); i++)
        if (!g_strcmp0(map[i].name, a)) {
            c->anchor_top = map[i].t;
            c->anchor_bottom = map[i].b;
            c->anchor_left = map[i].l;
            c->anchor_right = map[i].r;
            return TRUE;
        }
    g_set_error(error, VP_CONFIG_ERROR, 0, "未知的 anchor：%s", a ? a : "(null)");
    return FALSE;
}

static gboolean get_int(GKeyFile *kf, const char *group, const char *key, int min, int max, int *out, GError **error)
{
    if (!g_key_file_has_key(kf, group, key, NULL)) return TRUE;
    GError *e = NULL;
    int v = g_key_file_get_integer(kf, group, key, &e);
    if (e) {
        g_propagate_prefixed_error(error, e, "[%s] %s：", group, key);
        return FALSE;
    }
    if (v < min || v > max) {
        g_set_error(error, VP_CONFIG_ERROR, 0, "[%s] %s 超出范围 %d–%d", group, key, min, max);
        return FALSE;
    }
    *out = v;
    return TRUE;
}

static gboolean get_bool(GKeyFile *kf, const char *group, const char *key, gboolean *out, GError **error)
{
    if (!g_key_file_has_key(kf, group, key, NULL)) return TRUE;
    GError *e = NULL;
    gboolean v = g_key_file_get_boolean(kf, group, key, &e);
    if (e) {
        g_propagate_prefixed_error(error, e, "[%s] %s：", group, key);
        return FALSE;
    }
    *out = v;
    return TRUE;
}

static void get_path(GKeyFile *kf, const char *group, const char *key, char **out)
{
    char *v = g_key_file_get_string(kf, group, key, NULL);
    if (!v) return;
    g_strstrip(v);
    g_free(*out);
    *out = *v ? vp_expand_home(v) : NULL;
    g_free(v);
}

gboolean vp_config_load_data(VpConfig *c, const char *data, gsize len, GError **error)
{
    GKeyFile *kf = g_key_file_new();
    gboolean ok = FALSE;
    if (!g_key_file_load_from_data(kf, data, len, G_KEY_FILE_NONE, error)) goto out;

    get_path(kf, "appearance", "colors", &c->colors_css);
    get_path(kf, "appearance", "style", &c->style_css);
    if (!get_int(kf, "appearance", "width", 240, 1200, &c->width, error)) goto out;

    char *anchor = g_key_file_get_string(kf, "popup", "anchor", NULL);
    if (anchor) {
        gboolean a_ok = vp_config_set_anchor(c, g_strstrip(anchor), error);
        g_free(anchor);
        if (!a_ok) goto out;
    }
    if (!get_int(kf, "popup", "margin-top", 0, 10000, &c->margin_top, error) ||
        !get_int(kf, "popup", "margin-right", 0, 10000, &c->margin_right, error) ||
        !get_int(kf, "popup", "margin-bottom", 0, 10000, &c->margin_bottom, error) ||
        !get_int(kf, "popup", "margin-left", 0, 10000, &c->margin_left, error) ||
        !get_bool(kf, "popup", "hide-on-focus-loss", &c->hide_on_focus_loss, error) ||
        !get_int(kf, "audio", "max-volume", 100, VP_VOLUME_MAX_LIMIT, &c->max_volume, error) ||
        !get_bool(kf, "audio", "peak-meters", &c->peak_meters, error))
        goto out;
    ok = TRUE;
out:
    g_key_file_unref(kf);
    return ok;
}

gboolean vp_config_load_file(VpConfig *c, const char *path, GError **error)
{
    char *data = NULL;
    gsize len = 0;
    GError *e = NULL;
    if (!g_file_get_contents(path, &data, &len, &e)) {
        if (g_error_matches(e, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
            g_error_free(e);
            return TRUE;
        }
        g_propagate_error(error, e);
        return FALSE;
    }
    gboolean ok = vp_config_load_data(c, data, len, error);
    if (!ok && error && *error) g_prefix_error(error, "%s：", path);
    g_free(data);
    return ok;
}
