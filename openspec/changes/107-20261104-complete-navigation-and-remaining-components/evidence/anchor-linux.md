# Anchor：Linux 定向结果

- 基线：`f20a77415d8f37c185f0de74dfe5f570cbbed06a`；设计对照 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 API、Anchor/AnchorLink 类型和 basic/horizontal/static/targetOffset demos。
- 实现显式 BackTop 兼容滚动状态/回调与嵌套视口定位、动态唯一 key 层级、保留富标题、横纵投影、活动追踪、全局/每项偏移、边界和尾部钳制、受控高亮、点击取消/变化事件、占位固定/静态模式、溢出 wheel/焦点显露及单调有限运动。使用独立 Anchor Theme；未增加 renderer ABI 或平台链接。
- Linux GCC、`Ninja Multi-Config`、`linux-gcc-headless` Debug/Release：`rynui.anchor_component` 各 1/1（5 组，含回调销毁/异常、抛异常 append 回滚、用户首帧取消、隐藏/禁用、reduced motion、草稿保留、溢出/固定几何）；`rynui.anchor_gallery` 各 1/1；`rynui.theme_algorithm` 各 1/1。均来自本轮 21/21 定向组合运行，目录/source 共享检查另有 5/5 通过。组件子集可用 `ctest --preset linux-gcc-headless-debug -R '^rynui\.(anchor_component|anchor_gallery|theme_algorithm)$' --output-on-failure` 和对应 Release preset 复现。
- 构建入口：`cmake --preset linux-gcc-headless -DFETCHCONTENT_FULLY_DISCONNECTED=ON`；`cmake --build --preset linux-gcc-headless-debug --target rynui_anchor_component_tests rynui_navigation_completion_gallery_tests rynui_theme_algorithm_tests -j2` 及对应 `linux-gcc-headless-release`。相关 target 已由组合构建覆盖。
- Gallery 独立页使用实际 Anchor、裁剪滚动正文、用户滑块、富标题、方向、固定/静态和键控增删；API/source/token 提取、目录生成和冻结 ID 检查通过。五份完整 Theme golden 增加 Anchor 并更新整体 identity，原有 57 个字段保留。

原生 `linux-gcc-release` Gallery 构建通过（`cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2`）。2026-11-05 私有后台 Wayland 两页序列 **2/2 case、128 项检查、24 帧通过**：Vulkan/SPIR-V、系统字体、SDL input area/cursor 服务；Anchor 实际正文位移、方向/拓扑及离页清理通过，未使用桌面 fallback。

```sh
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --configuration Release --page anchor --page breadcrumb --timeout 90 --log-dir out/change107/native-final
```

早期有界运行发现验收辅助函数将“已聚焦、无需变化”误判失败；修复判定并保留真实 wheel 显露后重跑通过。分类级 `63.1`、Windows `64.1` 与最终集成 `65.1` 仍由父任务处理。

边界：不实现 URL/hash/history/DOM/CSS/React。HEADLESS/Recording 不代表 GPU 或系统输入法候选窗口、跨实际显示器、系统辅助技术实测。原始输出仅在忽略的 `out/change107/`。
