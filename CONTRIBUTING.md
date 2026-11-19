# 参与贡献

欢迎提交 issue 和 pull request。

## 开发环境

```sh
# Arch Linux
sudo pacman -S meson ninja gtk4 libpulse gtk4-layer-shell
# Debian / Ubuntu 24.04+
sudo apt install meson ninja-build libgtk-4-dev libpulse-dev libgtk4-layer-shell-dev

meson setup build
meson test -C build --print-errorlogs
```

只构建和测试核心库（不需要 GTK）：`meson setup build -Dui=disabled`。

试运行开发版本前，先退出已安装的常驻进程：`volpop --quit; ./build/src/volpop --show`。

## 代码组织

```
src/
├── core/            不依赖 GTK，也不连接音频服务，可以完整单元测试
│   ├── vp-volume    百分比 ⇄ pa_cvolume、步进、平衡、显示文字
│   ├── vp-model     设备 / 应用流 / 声卡的数据模型：排序、过滤、图标分类
│   ├── vp-convert   pa_*_info → 模型
│   └── vp-config    配置文件
├── backend/
│   └── vp-pulse     libpulse 后端：订阅、重连、音量写入节流、电平表
├── ui/              GTK4 界面
│   ├── vp-app       窗口、layer-shell、点外面收起、刷新调度
│   ├── vp-widgets   小部件和可复用的音量控件 VpVolumeCtl
│   └── vp-page-*    首页、设置页
└── main.c
tests/               GLib 测试（meson test）
```

设计约定：

- 过滤、排序、显示规则写在 `src/core/`，并补单元测试；界面只负责显示。
- 界面中的行按对象序号原地更新，不要整列表重建，否则用户正在拖动的滑块会被打断。
- 用户操作后的短时间内忽略服务端推送的旧值（`VpVolumeCtl.hold_until`），避免滑块回跳。
- 音量写入经过后端节流：同一对象同时最多一个请求在途。
- 电平表只在首页可见时开启。

## 提交前检查

- [ ] `meson test -C build` 全部通过，使用 `-Dwerror=true` 编译没有警告
- [ ] 新的逻辑有对应的测试
- [ ] 界面改动附了截图
- [ ] 用户可见的变化已写入 `CHANGELOG.md`

新建的源文件请在开头加上 `SPDX-License-Identifier: GPL-3.0-or-later`。
