// SPDX-License-Identifier: GPL-3.0-or-later
/* vp-convert.h — libpulse 的 pa_*_info → VpModel 结构（纯数据转换，不连接音频服务） */
#pragma once

#include <pulse/introspect.h>

#include "vp-model.h"

G_BEGIN_DECLS

VpDevice *vp_device_from_sink(const pa_sink_info *i);
VpDevice *vp_device_from_source(const pa_source_info *i);
VpStream *vp_stream_from_sink_input(const pa_sink_input_info *i);
VpStream *vp_stream_from_source_output(const pa_source_output_info *i);
VpCard   *vp_card_from_info(const pa_card_info *i);

G_END_DECLS
