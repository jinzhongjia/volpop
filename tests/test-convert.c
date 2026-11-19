// SPDX-License-Identifier: GPL-3.0-or-later
#include "vp-convert.h"

#include <string.h>

static void test_sink(void)
{
    pa_sink_port_info speaker = { .name = "analog-output-speaker", .description = "Speakers", .priority = 10000,
                                  .available = PA_PORT_AVAILABLE_UNKNOWN };
    pa_sink_port_info hp = { .name = "analog-output-headphones", .description = "Headphones", .priority = 9900,
                             .available = PA_PORT_AVAILABLE_NO };
    pa_sink_port_info *ports[] = { &speaker, &hp, NULL };
    pa_sink_info i = { 0 };
    i.index = 61;
    i.name = "alsa_output.pci.analog-stereo";
    i.description = "Built-in Audio Analog Stereo";
    i.monitor_source_name = "alsa_output.pci.analog-stereo.monitor";
    i.mute = 1;
    i.card = 52;
    i.flags = PA_SINK_DECIBEL_VOLUME;
    pa_channel_map_init_stereo(&i.channel_map);
    pa_cvolume_set(&i.volume, 2, PA_VOLUME_NORM / 2);
    i.n_ports = 2;
    i.ports = ports;
    i.active_port = &speaker;
    i.proplist = pa_proplist_new();
    pa_proplist_sets(i.proplist, PA_PROP_DEVICE_BUS, "pci");
    pa_proplist_sets(i.proplist, PA_PROP_DEVICE_FORM_FACTOR, "internal");

    VpDevice *d = vp_device_from_sink(&i);
    g_assert_cmpint(d->dir, ==, VP_OUTPUT);
    g_assert_cmpuint(d->index, ==, 61);
    g_assert_cmpstr(d->description, ==, "Built-in Audio Analog Stereo");
    g_assert_cmpstr(d->monitor_name, ==, "alsa_output.pci.analog-stereo.monitor");
    g_assert_true(d->mute);
    g_assert_true(d->decibel);
    g_assert_cmpuint(d->card, ==, 52);
    g_assert_cmpstr(d->bus, ==, "pci");
    g_assert_cmpuint(d->volume.channels, ==, 2);
    g_assert_cmpuint(d->ports->len, ==, 2);
    g_assert_true(((VpPort *)d->ports->pdata[0])->available);  /* UNKNOWN 视为可用 */
    g_assert_false(((VpPort *)d->ports->pdata[1])->available); /* NO 为未插入 */
    g_assert_cmpstr(d->active_port, ==, "analog-output-speaker");
    vp_device_free(d);
    pa_proplist_free(i.proplist);
}

static void test_source(void)
{
    pa_source_info i = { 0 };
    i.index = 5;
    i.name = "x.monitor";
    i.description = NULL; /* 没有描述时退回名称 */
    i.monitor_of_sink = 61;
    pa_channel_map_init_stereo(&i.channel_map);
    pa_cvolume_set(&i.volume, 2, PA_VOLUME_NORM);
    VpDevice *d = vp_device_from_source(&i);
    g_assert_cmpint(d->dir, ==, VP_INPUT);
    g_assert_true(d->is_monitor);
    g_assert_cmpstr(d->description, ==, "x.monitor");
    g_assert_null(d->active_port);
    g_assert_null(d->bus);
    vp_device_free(d);

    i.monitor_of_sink = PA_INVALID_INDEX;
    d = vp_device_from_source(&i);
    g_assert_false(d->is_monitor);
    vp_device_free(d);
}

static void test_streams(void)
{
    pa_sink_input_info i = { 0 };
    i.index = 9;
    i.sink = 61;
    i.name = "Playback";
    i.has_volume = 1;
    i.volume_writable = 1;
    i.corked = 1;
    pa_channel_map_init_stereo(&i.channel_map);
    pa_cvolume_set(&i.volume, 2, PA_VOLUME_NORM);
    i.proplist = pa_proplist_new();
    pa_proplist_sets(i.proplist, PA_PROP_APPLICATION_NAME, "Firefox");
    pa_proplist_sets(i.proplist, PA_PROP_APPLICATION_ICON_NAME, "firefox");
    pa_proplist_sets(i.proplist, PA_PROP_APPLICATION_ID, "org.mozilla.firefox");
    pa_proplist_sets(i.proplist, PA_PROP_MEDIA_ROLE, "video");

    VpStream *s = vp_stream_from_sink_input(&i);
    g_assert_cmpint(s->dir, ==, VP_OUTPUT);
    g_assert_cmpuint(s->device, ==, 61);
    g_assert_cmpstr(s->app, ==, "Firefox");
    g_assert_cmpstr(s->media, ==, "Playback"); /* 没有 media.name 时用流名 */
    g_assert_cmpstr(s->icon, ==, "firefox");
    g_assert_cmpstr(s->role, ==, "video");
    g_assert_true(s->corked);
    g_assert_true(s->has_volume);
    vp_stream_free(s);

    i.volume_writable = 0; /* 不可写也算没有音量 */
    s = vp_stream_from_sink_input(&i);
    g_assert_false(s->has_volume);
    vp_stream_free(s);
    pa_proplist_free(i.proplist);

    pa_source_output_info o = { 0 };
    o.index = 11;
    o.source = 62;
    o.has_volume = 1;
    o.volume_writable = 1;
    o.mute = 1;
    pa_channel_map_init_mono(&o.channel_map);
    pa_cvolume_set(&o.volume, 1, PA_VOLUME_NORM);
    o.proplist = pa_proplist_new();
    pa_proplist_sets(o.proplist, PA_PROP_MEDIA_NAME, "Mic capture");
    pa_proplist_sets(o.proplist, PA_PROP_MEDIA_ICON_NAME, "audio-input-microphone");
    s = vp_stream_from_source_output(&o);
    g_assert_cmpint(s->dir, ==, VP_INPUT);
    g_assert_cmpuint(s->device, ==, 62);
    g_assert_null(s->app);
    g_assert_cmpstr(s->media, ==, "Mic capture");
    g_assert_cmpstr(s->icon, ==, "audio-input-microphone"); /* 退回 media.icon_name */
    g_assert_true(s->mute);
    vp_stream_free(s);
    pa_proplist_free(o.proplist);
}

static void test_card(void)
{
    pa_card_profile_info2 analog = { .name = "output:analog-stereo+input:analog-stereo",
                                     .description = "模拟立体声双工", .n_sinks = 1, .n_sources = 1,
                                     .priority = 6565, .available = 1 };
    pa_card_profile_info2 off = { .name = "off", .description = "关", .priority = 0, .available = 1 };
    pa_card_profile_info2 hdmi = { .name = "output:hdmi-stereo", .description = "HDMI", .n_sinks = 1,
                                   .priority = 5900, .available = 0 };
    pa_card_profile_info2 *profiles[] = { &analog, &off, &hdmi, NULL };
    pa_card_info i = { 0 };
    i.index = 52;
    i.name = "alsa_card.pci";
    i.n_profiles = 3;
    i.profiles2 = profiles;
    i.active_profile2 = &analog;
    i.proplist = pa_proplist_new();
    pa_proplist_sets(i.proplist, PA_PROP_DEVICE_DESCRIPTION, "内置音频");

    VpCard *c = vp_card_from_info(&i);
    g_assert_cmpuint(c->index, ==, 52);
    g_assert_cmpstr(c->description, ==, "内置音频");
    g_assert_cmpuint(c->profiles->len, ==, 3);
    g_assert_cmpstr(c->active_profile, ==, analog.name);
    VpProfile *p = c->profiles->pdata[2];
    g_assert_false(p->available);
    g_assert_cmpuint(((VpProfile *)c->profiles->pdata[0])->n_sources, ==, 1);
    vp_card_free(c);

    /* 没有 device.description 时用名称 */
    pa_proplist_clear(i.proplist);
    i.active_profile2 = NULL;
    c = vp_card_from_info(&i);
    g_assert_cmpstr(c->description, ==, "alsa_card.pci");
    g_assert_null(c->active_profile);
    vp_card_free(c);
    pa_proplist_free(i.proplist);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/convert/sink", test_sink);
    g_test_add_func("/convert/source", test_source);
    g_test_add_func("/convert/streams", test_streams);
    g_test_add_func("/convert/card", test_card);
    return g_test_run();
}
