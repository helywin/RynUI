# 其他分类 Linux 定向验收

2026-11-12，基于 main `48033d58061a540514ed7f60941e9880fc9b18d5`。实际使用 Linux/GCC、正式 `linux-gcc-headless` Debug/Release 与 `linux-gcc` Release、Ninja Multi-Config，单次构建最多 `-j2`。所有依赖、生成文件与构建输出均在 main 自有 `out/`。

ConfigProvider 的旧 partial 目录未反映 107/108 已实现的窗口隔离及 locale/formatLocale/messages/direction/size/disabled/theme。现同步原生完整状态，保留 DOM/CSP/Web wave、完整 ICU/CLDR 和平台桥接边界；不新增 Provider 公共属性或扩大消费者范围。Affix、App、BorderBeam 保留既有实现；Util 继续是无运行时控件的独立说明页。

## 自动验证

- HEADLESS 相关 10 个 target、46 个不同 CTest：Debug 首轮 **44/46**；修正 public i18n 夹具的第二窗口销毁顺序和旧 Modal 目录断言后，两个失败入口定向复验 **2/2**。补充 evidence 后目录入口再验 **1/1**，不累加为新的覆盖数。Release 同选择单轮 **46/46**。既有内部 require 未统一计数，不虚报其断言总数。
- `gallery_sample_source_contract/generated`、`gallery_i18n_source/contract`：**4/4**。源码实际为 **114 个唯一样例**；旧 112 计数来自 `1cc667d`，后续仅增加 Card rounded-mask 与 Modal backdrop，原 112 身份顺序完全保留。现精确检查 114，并验证两者真实 clip/blur/App 消费入口；静态目标 inventory 仍为 **490 个唯一目标**，原 frozen 458 身份及顺序的既有守卫保留。
- 最终 Release Gallery 正常窗口：ConfigProvider、Affix、BorderBeam、App、Util、Button，共 **6 cases / 381 checks / 80 submitted frames / 80 accepted frame-callback markers**。宽度为 1440/960/728/480 循环，使用实际系统字体、Vulkan/SPIR-V。验证默认尺寸/禁用与显式、嵌套覆盖；语言/方向更新保留草稿；Affix 目标、偏移和释放；Beam phase、参数、减弱动画与隐藏恢复；App 通知更新/语言、Modal 服务及离页退役。四个有效组件页均先建立真实 pointer capture 和 Core preedit，再断言 editor/session/capture 及本页服务清理；窄屏壳自身的导航 Drawer 必须保留。
- 独立 hidden/offscreen GPU readback：上述四个有效组件页 **4 cases / 57 checks / 171 submitted frames**，scale=1，**72 个诊断捕获**。该路径不执行真实 surface frame callback，也不激活 host input-area/cursor 桥接，不能与前一组混计。

主要命令：

```text
cmake --preset linux-gcc-headless
cmake --build --preset linux-gcc-headless-{debug,release} --target <受影响 target> -j2
ctest --preset linux-gcc-headless-{debug,release} -R <ConfigProvider/i18n/App/decoration/catalog/host 定向选择>
cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2
python tools/run_gallery_page_acceptance.py <Release Gallery> --preset linux-gcc --page config-provider --page affix --page border-beam --page app --page util --page button
```

Linux runner 使用 `tools/run_background_wayland.py`、私有 D-Bus 与 headless Mutter；PATH 显式选择已修正 frame-clock 的 main 自有私有 Mutter 51.1。每轮重新核验实际 compositor executable、Clutter 加载路径/哈希及子进程退出，`desktop_fallback=false`；没有修改系统 compositor、正常事件泵或五秒 callback deadline。未打开用户桌面窗口或控制物理鼠标。

本阶段同时修正 GCC 测试夹具中同目录头文件的错误 `support/` 前缀，保留全部原断言。Modal 目录守卫改为要求当前真实服务/背景模糊及实施证据，不回退生产功能。原生验收夹具按编辑器 viewport 点击，并区分 window-layer 编辑器与文档滚动区；退出比较精确的非本页 owner 集合，避免将窄屏壳 Drawer 误报为泄漏。

clang-format 22.1.3、目录生成/自检、实际 source generator、OpenSpec doctor/strict 与 diff 检查通过。这里只关闭 63.5；63.4 等待 Progress，Windows 分类项与 65.1 保持独立。既有 Windows 证据保留，不以本次 Linux 结果替代。物理系统 IME、OS 辅助技术、其他实际 DPI、跨物理显示器、stock Mutter 51.1 及官方 Web/native 自动差分均不据此宣称通过；捕获是诊断材料，不是逐图黄金比较或人工审核通过数。原始日志、图像、运行指纹和二进制只保留于 ignored `out/`。
