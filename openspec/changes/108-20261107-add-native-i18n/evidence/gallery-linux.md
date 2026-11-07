# Linux 后台双语与最终集成

2026-11-08，源码 `1cc667def28765a967d8e914d31e24d08179a068`。实际 WSL2 Ubuntu 24.04，GNU/GCC，正式 `linux-gcc-release`、Ninja Multi-Config；经 `tools/run_background_wayland.py` 启动独立 headless Mutter/Wayland，不连接用户桌面。

```text
cmake --build --preset linux-gcc-release --parallel 1 --target rynui_token_gallery
python3 tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --i18n --timeout 120 --log-dir out/108-gallery-linux
```

最终构建和 runner/native 均通过。实际 Vulkan/SPIR-V、系统 Fontconfig 字体、scale 1：7 条路径、263 项检查、87 次提交、24 张局部捕获。覆盖语言按钮、Latin/CJK/Arabic 实际绘制、编辑/反向选区/preedit/undo、打开弹层、配置来源、拒绝/恢复/重入、resize、导航退役和 idle。软件 Vulkan 的唯一设备为 Mesa llvmpipe 25.2.8 / LLVM 20.1.2；没有据此声明独立显卡性能或其他驱动通过。

初次探针因系统缺少 CJK 字体在零帧时失败；登记仓库锁定的 Noto Sans CJK SC 字体到该 WSL 用户的 Fontconfig 后重新构建和执行。实际由系统字体服务解析，没有开启应用内 bundled fallback。字体原始文件和运行输出不入库。

offscreen 的 host input-area/cursor 标志实际为 false。Linux 分数 DPI、系统 IME、人工桌面、跨物理显示器和 OS accessibility 未测；Windows 的 1.5 倍 DPI 结果保持独立。

## 最终平台通用集成

`windows-msvc-headless-debug` / `windows-msvc-headless-release` 各 30/30：全部 14 个 i18n/真实 Gallery 测试入口、12 个 ConfigProvider 入口，以及四个原有 Theme/golden 入口。沿用原五套 checked-in golden，未重新生成以适配结果。

```text
^rynui\.(i18n_.*|config_provider_.*|portable\.(default_theme|theme_public_api|theme_algorithm|theme_runtime))$
```

平台通用逻辑没有在 Linux 重复整套 CTest。107 的真实 App/服务与 ConfigProvider 实现直接复用，没有增加第二份 Provider 或服务 Holder。格式、source/catalog、doctor、strict 和 diff 检查通过；原始日志、截图、运行指纹只保存在 ignored `out/`。
