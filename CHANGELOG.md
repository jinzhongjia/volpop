# Changelog

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，版本号遵循语义化版本。

## [0.1.0] — 2026-11-20

### 新增
- 弹出面板：默认输出 / 输入设备切换、音量、静音、电平表；每个应用单独调音量和切换输出设备。
- 声音设置：设备端口、左右平衡、设为默认；录音应用的音量与输入设备；声卡配置切换。
- 支持超过 100% 的软件放大（`[audio] max-volume`，最高 150%）。
- libpulse 后端：自动重连、音量写入节流；电平表只在面板可见时运行。
- gtk4-layer-shell 弹层，点击面板外收起；外观与 netpop 一致，配色可跟随外部主题文件。
- 不依赖 GTK 的核心库及单元测试；CI、CodeQL、按 tag 自动发布、Arch PKGBUILD。
