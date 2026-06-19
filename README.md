# PCL-CGRE

Plain Craft Launcher 的 GTK4 + libadwaita 跨平台社区移植版。

PCL 是最受欢迎的 Minecraft Java Edition 启动器之一，PCL-CE 使用 C# .NET 编写，仅支持 Windows。

本项目尝试用 C++17 和 GTK 生态（GTK4 / libadwaita / libsoup）重新实现核心功能，目标是在 Linux、Windows（MSYS2）、macOS 上获得原生体验。

> **状态：原型阶段** — 基础 UI 框架、资源浏览和 CLI 搜索功能已可用。启动 Minecraft、Mod 管理、账号登录等功能尚未实现。

## 功能

| 功能 | 状态 |
|---|---|
| 标题栏页签导航（启动 / 下载 / 设置 / 更多） | ✅ |
| 启动页（侧边栏 + 资讯面板 + 搜索框 + 新闻卡片） | ✅ |
| Minecraft 版本列表（BMCLAPI / Mojang API） | ✅ |
| Minecraft 版本详情与安装页 | ✅ |
| Mod / 整合包 / 数据包 / 资源包 / 光影 / 世界浏览 | ✅ |
| CurseForge + Modrinth 双源搜索 | ✅ |
| 分类、版本、加载器、排序筛选 | ✅ |
| Mod 加载器版本列表（Forge / Fabric / Quilt / OptiFine 等） | ✅ |
| 资源详情页（作者、协议、文件列表） | ✅ |
| 下载页（左侧导航 + 右侧内容） | ✅ |
| 异步图标加载 + 磁盘缓存 | ✅ |
| 收藏夹 | ✅ |
| 愚人节版本列表（本地 JSON） | ✅ |
| 自定义图标主题（Lucide） | ✅ |
| HarmonyOS Sans 字体集成（GB 2312 子集化） | ✅ |
| 设置页面（10 个子页面：启动 / Java / 游戏管理 / 界面 / 语言 / 杂项 / 关于 / 更新 / 反馈 / 日志） | ✅ |
| 更多页面（帮助 / 工具箱） | ✅ |
| 通知系统（右下角 Toast + 左侧通知抽屉） | ✅ |
| 命令行接口（CLI 搜索、安装、实例管理等） | ✅ |
| 动态窗口背景（纯色 / 本地图片 / 网络图片） | ✅ |
| 帧率限制（30–360 FPS，≥ 360 无限制） | ✅ |
| 启动 Minecraft | ❌ |
| 账号登录（Microsoft / AuthLib） | ❌ |
| Mod 下载与管理 | ❌ |

## 依赖

### 构建依赖

| 库 | 版本要求 |
|---|---|
| GTK 4 | ≥ 4.0 |
| libadwaita | ≥ 1.0 |
| fontconfig | 任意 |
| libsoup 3 | ≥ 3.0 |
| libcurl | 任意 |
| json-glib | ≥ 1.0 |
| CMake | ≥ 3.16 |
| C++ 编译器 | GCC ≥ 13 或 Clang ≥ 17（需 C++17） |
| blueprint-compiler | ≥ 0.12 |

### 运行时依赖（非 AppImage）

如果使用系统包管理器安装而非 AppImage，运行时需要：

```
gtk4 libadwaita fontconfig libsoup3 libcurl json-glib
adwaita-icon-theme (或等价的图标主题)
```

## 从源码构建

### 快速编译

```bash
./scripts/build.sh          # Release
./scripts/build.sh Debug    # Debug
```

产物在 `./build/pcl-cgre`，通过 `./build/pcl-cgre` 启动。

### AppImage 打包

生成可在任意 glibc ≥ 2.35 的 Linux 系统上运行的独立包：

```bash
./scripts/build_appimage.sh
```

产物在 `build/dist/PCL-CGRE-<version>-x86_64.AppImage`（~52 MB）。

### 手动编译

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build --parallel $(nproc)
./build/pcl-cgre
```

### 各发行版依赖安装

**Arch Linux**
```bash
sudo pacman -S gtk4 libadwaita fontconfig libsoup3 json-glib curl cmake gcc \
    blueprint-compiler
```

**Fedora**
```bash
sudo dnf install gtk4-devel libadwaita-devel fontconfig-devel \
    libsoup3-devel libcurl-devel json-glib-devel cmake gcc-c++ blueprint-compiler
```

**Ubuntu / Debian**
```bash
sudo apt install libgtk-4-dev libadwaita-1-dev libfontconfig-dev \
    libsoup-3.0-dev libcurl4-openssl-dev libjson-glib-dev cmake g++ \
    blueprint-compiler libglib2.0-dev-bin
```

### Windows（MSYS2）

PCL-CGRE 深度依赖 GTK 生态，Windows 上**只能在 MSYS2 UCRT64 环境内构建**，不支持从 Linux 交叉编译。

```bash
# 在 MSYS2 UCRT64 shell 中
pacman -S mingw-w64-ucrt-x86_64-gtk4 mingw-w64-ucrt-x86_64-libadwaita \
    mingw-w64-ucrt-x86_64-fontconfig mingw-w64-ucrt-x86_64-libsoup3 \
    mingw-w64-ucrt-x86_64-libcurl mingw-w64-ucrt-x86_64-json-glib \
    mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-make

cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release .
cmake --build build --parallel $(nproc)
```

### macOS

尚无测试设备，状态未知。理论上安装 GTK4 + libadwaita 等依赖后可通过 Homebrew 构建：

```bash
brew install gtk4 libadwaita fontconfig libsoup json-glib curl cmake pkg-config
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build --parallel $(sysctl -n hw.ncpu)
```

如有测试结果，欢迎反馈。

## CLI 命令行接口

`pcl-cgre` 支持纯命令行模式，无需启动图形界面（零 GTK 依赖）。

### 基本用法

```bash
pcl-cgre --help                    # 显示帮助
pcl-cgre --gui                     # 启动图形界面（默认）
pcl-cgre version                   # 显示版本号
pcl-cgre version --json            # JSON 格式输出版本
```

### 搜索

```bash
# Minecraft 资源搜索
pcl-cgre search mod <关键词> [选项]
pcl-cgre search modpack <关键词>
pcl-cgre search respack <关键词>
pcl-cgre search datapack <关键词>
pcl-cgre search shader <关键词>

# Minecraft 版本搜索
pcl-cgre search minecraft [rel|devel|alpha|april]
pcl-cgre search minecraft latest          # 最新正式版 + 快照版

# 搜索选项
#   --type t          按类型筛选
#   --source modrinth|curseforge  按来源筛选
#   --loader forge|fabric|quilt|neoforge|liteloader
#   --limit N          返回条数（默认 20）
#   --json             JSON 格式输出

# `list` 是 `search` 的别名
pcl-cgre list mod sodium --source modrinth --loader fabric
```

### 安装（交互式）

```bash
pcl-cgre install mod <关键词> --instance <实例名>
pcl-cgre install respack <关键词> --version <MC版本>
pcl-cgre install shader <关键词> --loader fabric
# 选项: --instance, --version, --loader, --source, --prequences, --noconfirm
```

> **注意：** 下载后端尚未实现。

### 实例管理

```bash
pcl-cgre instance list              # 列出本地实例
pcl-cgre instance list --json       # JSON 格式输出
pcl-cgre instance <名称>            # 查看实例详情及可用操作
```

### 账号管理

```bash
pcl-cgre account list               # 列出所有账号
pcl-cgre account list --json        # JSON 格式输出
```

### 插件管理

```bash
pcl-cgre plugin list                # 列出已安装插件
```

## 项目结构

```
CMakeLists.txt                       # 顶层构建脚本
libpclcore/                          # 纯 C++17 核心库（零 GTK/GLib 依赖）
├── CMakeLists.txt
├── include/pclcore/
│   ├── pclcore.hpp                  # umbrella header
│   ├── data/
│   │   ├── McVersion.hpp            # Minecraft 版本数据结构
│   │   └── LoaderVersion.hpp        # 加载器版本数据结构
│   ├── core/
│   │   ├── Colors.hpp               # PCL-CE 配色常量
│   │   └── Log.hpp                  # 日志宏
│   ├── local/
│   │   ├── Instance.hpp             # 本地实例 + 可插拔 Provider
│   │   ├── Account.hpp              # 账号信息 + 可插拔 Provider
│   │   ├── CrashReport.hpp          # 崩溃报告模型 + Provider
│   │   ├── DownloadTask.hpp         # 下载任务模型 + Provider
│   │   ├── LaunchContent.hpp        # 启动页资讯内容
│   │   ├── HelpContent.hpp          # 帮助 / FAQ 内容
│   │   └── ToolRegistry.hpp         # 工具箱 / 快捷按钮注册
│   └── network/
│       └── Dispatcher.hpp           # 异步调度抽象（GUI / CLI 双实现）
└── src/
    ├── core/Log.cpp
    ├── local/Instance.cpp
    ├── local/DownloadTask.cpp
    └── network/Dispatcher.cpp
data/ui/                             # Blueprint 界面定义 (.blp)
├── launch_right.blp                 # 启动页右侧（资讯面板）
├── launch_sidebar.blp               # 启动页左侧（侧边栏）
├── skin.blp                         # 皮肤管理
├── tools.blp                        # 工具箱
├── help_faq.blp                     # 帮助 FAQ
├── help_support.blp                 # 帮助支持
├── page_setup_*.blp                 # 设置子页面（启动/Java/游戏管理/界面/语言/杂项/关于/更新）
└── settings_placeholder.blp         # 设置占位页
resources/
├── fonts/                           # HarmonyOS Sans（GB 2312 子集化，4 字重）
├── icons/pcl-cgre/                  # Lucide SVG 图标主题
├── blocks/                          # Mod 加载器 / 版本类型 PNG 图标
├── Heads/                           # 社区贡献者头像
├── Skins/                           # Minecraft 默认皮肤
├── lirpa_loof.json                 # 愚人节版本列表
└── style.css                        # 全局 CSS 样式
scripts/
├── build.sh                         # CMake 快速编译
├── build_appimage.sh                # AppImage 打包
├── gen_gresource_xml.py             # GResource XML 生成
├── setup_msys2_sysroot.py           # MSYS2 交叉编译环境准备
└── setup_vps.sh                     # VPS 开发环境初始化
src/
├── pcl-cgre/                        # GUI 前端（GTK4 + libadwaita）
│   ├── main.cpp                     # 入口：解析 --gui / CLI 分流
│   ├── CMakeLists.txt
│   ├── app/
│   │   ├── App.{hpp,cpp}            # AdwApplication 包装类
│   │   └── MainWindow.{hpp,cpp}     # 主窗口：标签页导航、通知系统挂载
│   ├── core/
│   │   ├── GtkDispatcher.hpp        # GTK 主线程调度器（实现 Dispatcher 接口）
│   │   ├── Styles.{hpp,cpp}         # CSS 样式定义
│   │   ├── Colors.hpp               # PCL-CE 配色常量（重新导出 libpclcore）
│   │   ├── BackgroundManager.{hpp,cpp}  # 动态窗口背景（纯色/本地图片/网络图片）
│   │   ├── FrameLimiter.{hpp,cpp}   # 帧率限制（30–360 FPS）
│   │   └── Log.hpp                  # 日志宏（重新导出 libpclcore）
│   ├── pages/
│   │   ├── LaunchPage.{hpp,cpp}     # 启动页：侧边栏 + 资讯面板 + 新闻卡片
│   │   ├── DownloadPage.{hpp,cpp}   # 下载页：左侧导航 + GtkStack 右侧内容
│   │   ├── McVersionDetailPage.{hpp,cpp}  # Minecraft 版本详情与安装页
│   │   ├── ResourceDetailPage.{hpp,cpp}   # 资源详情页
│   │   ├── SettingsPage.{hpp,cpp}   # 设置页面（10 个子页面）
│   │   ├── MorePage.{hpp,cpp}       # 更多页面（帮助 + 工具箱）
│   │   └── PageSetupStubs.cpp       # 设置子页面占位实现
│   ├── widgets/
│   │   ├── HeaderTabs.{hpp,cpp}     # 标题栏页签组
│   │   ├── NotificationDrawer.{hpp,cpp}  # 通知抽屉（左侧滑出面板）
│   │   ├── NotificationToast.{hpp,cpp}   # 右下角弹窗通知
│   │   └── ResourceItem.{hpp,cpp}   # 资源搜索结果列表项
│   ├── network/
│   │   ├── HttpUtil.hpp             # 同步 HTTP GET 工具（libsoup）
│   │   ├── McVersionFetcher.{hpp,cpp}    # Minecraft 版本清单
│   │   ├── LoaderFetcher.{hpp,cpp}       # Mod 加载器版本列表
│   │   └── ResourceFetcher.{hpp,cpp}     # CurseForge / Modrinth API
│   ├── data/
│   │   ├── McVersion.hpp            # 版本数据（重新导出 libpclcore）
│   │   └── LoaderVersion.hpp        # 加载器版本数据（重新导出 libpclcore）
│   └── util/
│       ├── IconHelper.{hpp,cpp}     # Lucide 图标 + PCL-CE block 图标加载
│       └── FontHelper.{hpp,cpp}     # 自定义字体注册（HarmonyOS Sans）
└── cli/                              # CLI 前端（零 GTK 依赖）
    ├── CMakeLists.txt
    ├── CliCommands.hpp              # CLI 入口声明
    └── CliCommands.cpp              # 所有 CLI 子命令实现
```

## 许可

本项目重度参考了 [PCL2](https://github.com/Hex-Dragon/PCL2) 的相关代码，因此采用自定义许可证。详见 [LICENSE](./LICENSE)。

[NOTICE](./NOTICE) 包含了第三方二次创作声明及第三方资源（Lucide 图标、HarmonyOS Sans 字体）的版权归属。
