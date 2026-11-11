# 终端功能调研与设计取舍

日期：2026-11
目标：在 Linux 上做一个 macOS 风格的现代终端，支持分屏、主题/设置/应用打包，VT 解析内核使用 `libghostty-vt`。

## 1. 调研来源

- iTerm2 Features：分屏、搜索、全局搜索、触发器、通知、tmux 集成  
  https://iterm2.com/features.html
- Ghostty Features：原生窗口/标签/分屏、GPU 渲染、主题、ligatures、Kitty graphics、Quick Terminal  
  https://ghostty.org/docs/features
- Warp / Ghostty / iTerm2 对比（2025）：块状输出、命令面板、AI、搜索、分屏、通知  
  https://blog.apps.deals/macos-terminal-comparison-warp-ghostty-default
- libghostty-vt 官方文档：Terminal、Render State、Selection、Search、Input、Kitty Graphics  
  https://lib.ghostty.org/vt/
- libghostty-vt 1.0 发布说明（2026-11）  
  https://mitchellh.com/writing/libghostty-1-0

## 2. 现代终端应具备的能力

### 2.1 终端正确性（内核层）

| 能力 | 说明 | 本项目 |
| --- | --- | --- |
| PTY | 真正交互式 shell，尺寸同步 | ✅ forkpty + TIOCSWINSZ |
| UTF-8 / grapheme | 中文、emoji、组合字符正确宽度 | ✅ libghostty-vt |
| 颜色 | 16 / 256 / truecolor，SGR 样式 | ✅ |
| 备用屏幕 | vim、htop、less 等全屏程序 | ✅ |
| 光标 | 形状、闪烁、可见性 | ✅ |
| 滚动回看 | 历史缓冲与重排 | ✅ 8000 行 |
| Bracketed paste | 安全粘贴 | ✅ libghostty-vt |
| 鼠标上报 | X10/SGR/URxvt 等 | ✅ 鼠标编码器 |
| 焦点上报 | mode 1004 | ✅ |
| 标题 / 工作目录 | OSC 0/2、OSC 7 | ✅ + `/proc` 回退 |
| 铃声 | BEL | ✅ 窗格闪烁 |
| IME | 中文输入提交、候选窗定位 | ✅ inputMethodEvent + ImCursorRectangle |

### 2.2 日常使用体验

| 能力 | 说明 | 本项目 |
| --- | --- | --- |
| 标签页 | 新建/关闭/切换，竖向侧栏 | ✅ |
| 分屏 | 任意层级横/纵向，拖动分隔线 | ✅ |
| 查找 | 高亮全部匹配、上一个/下一个 | ✅ |
| 选择 | 拖选、双击选词、三击选行、全选 | ✅ |
| 复制/粘贴 | 系统剪贴板、快捷键、右键菜单 | ✅ |
| 字体缩放 | 快捷键调整并重排网格 | ✅ |
| 命令面板 | 键盘驱动的高频操作入口 | ✅ |
| 快捷键帮助 | 键帽样式说明 | ✅ |
| 主题 | 深色 / 浅色，持久化 | ✅ |
| 设置面板 | 主题、字号、Shell、回看行数 | ✅ |
| 标签管理 | 重命名、上移/下移、关闭 | ✅（右键菜单） |
| 标签拖拽排序 | 调整标签顺序 | ⏳ |
| 配置/Profile | 多套 shell、字体、配色 | ⏳（Shell/回看可配置） |
| 会话恢复 | 重启后恢复窗口/标签 | ⏳ |

### 2.3 进阶能力

| 能力 | 本项目 |
| --- | --- |
| OSC 52 / OSC 5522 剪贴板读写 | ✅ 已接入 libghostty-vt 剪贴板回调 |
| OSC 8 超链接 | ✅ `Ctrl + 单击` |
| 桌面通知（OSC 9 / 777） | ✅ 无焦点时通过 `notify-send`，同时应用内 Toast |
| Kitty graphics / 内联图片 | ⏳ libghostty-vt 可解析，但当前自绘层未做图片合成 |
| ligatures / 字体特性 | ⏳ 网格渲染暂未启用 |
| tmux control mode / AI / 块状输出 | ⏳ |
| 多窗口 / Quick Terminal / 布局恢复 | ⏳ |

## 3. 为什么选择 libghostty-vt

- 从 Ghostty 生产内核提取，VT 语义、Unicode 宽度、备用屏幕、鼠标/键盘协议覆盖完整。
- 原生 C、无重量级依赖；性能好。
- 自带 **Render State**、**Selection**、**Search**、**Key/Mouse Encoder**、**Paste**，避免自己重新实现这些易错逻辑。
- 相比 libvterm：终端状态/选择/搜索/输入编码是完整的一体化 API；本项目最终只保留 `libghostty-vt`，没有使用 libvterm。

## 4. 为什么用 Qt Quick（而不是 QuickShell）

- QuickShell 是 Wayland 桌面外壳框架，目标是把 shell 组件（bar、launcher、锁屏等）挂到 layer-shell 上；独立终端应用需要普通 xdg-toplevel 窗口、焦点、IME、窗口缩放，Qt Quick 更合适。
- Qt Quick 提供原生窗口、输入法、DPI、动画、QPainter 自绘能力；终端网格用 `QQuickPaintedItem` 绘制，UI 用 QML 描述。
- 不依赖 Chromium/Electron，冷启动和内存占用更符合系统终端预期。

## 5. 视觉方向

- macOS Big Sur / Sonoma 的层级：深色半透明侧栏、圆角窗口、交通灯、细描边、低饱和蓝紫背景光。
- 终端窗格做成玻璃卡片，透明默认背景让 QML 渐变透出；活动窗格蓝色描边，非活动窗格压暗。
- 使用 JetBrains Mono 终端字体与 Noto Sans 界面字体；中文走系统 CJK fallback。
- 侧栏竖向标签、底部新建按钮与快捷键卡片，工具栏保留分屏/搜索/命令/主题/设置入口。
- 深色海洋渐变与浅色雾面两套主题，终端调色板、选择色、光标色一并切换。
- 动效克制：悬停淡入、面板淡入、窗格激活色过渡、Toast。

## 6. 验收要点

- 真实 shell、vim/htop、彩色 `ls`、中文输出、IME。
- 双标签 + 三层分屏下布局正确，切换标签后会话持续运行。
- 搜索高亮与跳转、全选复制、粘贴、字号缩放。
- 关闭窗格/标签、退出码提示、重新启动。
- 窗口拖动、边缘缩放、全屏、交通灯。
- 在 Wayland（niri）和 Qt 6.10 下无 QML 运行时错误。
