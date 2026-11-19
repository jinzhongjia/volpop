# volpop

[![CI](https://github.com/jinzhongjia/volpop/actions/workflows/ci.yml/badge.svg)](https://github.com/jinzhongjia/volpop/actions/workflows/ci.yml)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)

基于 GTK4 和 libpulse 的音量面板，用来替代 pavucontrol。支持 PulseAudio 和 PipeWire（pipewire-pulse）。
外观与 [netpop](https://github.com/jinzhongjia/netpop) 一致。

- 状态栏弹出面板：切换输出 / 输入设备、音量、静音、电平表
- 每个应用单独调音量，并可以切换到其他输出设备
- 设置页：设备端口、左右平衡、默认设备、录音应用、声卡配置（如 HDMI / 模拟输出、蓝牙 A2DP / HFP）
- 常驻进程，弹出几乎没有延迟；电平表只在面板可见时运行

## 安装

依赖：gtk4 ≥ 4.12、libpulse ≥ 15、meson。可选：gtk4-layer-shell（固定在状态栏下方的弹层）。

```sh
meson setup build --prefix ~/.local
meson install -C build
```

Arch Linux：`cd packaging/arch && makepkg -si`

## 使用

```
volpop              切换显示 / 隐藏
volpop --settings   声音设置
volpop --daemon     后台常驻（用于自启动）
volpop --quit       退出
```

waybar 示例：

```jsonc
"wireplumber": { "on-click": "volpop", "on-click-right": "wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle" }
```

## 配置

`~/.config/volpop/volpop.conf`，所有选项见 [volpop.conf.example](data/volpop.conf.example)：

```ini
[appearance]
colors = ~/.config/theme/current/waybar.css   ; 用 @define-color 定义调色板的 CSS 文件

[audio]
max-volume = 150   ; 允许放大到 150%
```

## 开发

见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

[GPL-3.0-or-later](LICENSE)
