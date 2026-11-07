# Table / Tour：Windows 集成验证

2026-11-07，基于 `72bbb769` 后的本阶段源码。使用正式 CMake presets、Ninja Multi-Config 和最多两个 compiler jobs；原始产物仅保留在 ignored `out/`。

## 实现与回归

Table 补齐确定行高的 keyed 虚拟行、树/详情展开、筛选、固定列、RTL 和实际滚动条捕获。保留已访问富子树、编辑草稿与受控拒绝；缓存随访问增长，删除 key 才退役。单元格保留完整列宽，独立视口裁剪固定列之间的内容和命中；列宽变化在子节点放置前夹紧滚动。

Tour 使用带窗口/代际的目标引用、保留步骤内容、导航、遮罩高亮和既有输入约束事务；目标、富编辑器、子 Modal、语言更新、回调销毁和离页清理均有真实 Host 回归。窗口层和滚动条显式遵守 LayoutEngine 的 measure/place 合同。

共同修复覆盖初次挂载的目标 Theme/Config scope 恢复及局部失败回滚；原动态 append 防重入限制保持。初次订阅先登记 observer 再运行应用回调，允许 getter 销毁 Scope，抛错时撤销订阅。Select 弹层在祖先滚动位移更新后重新定位，修复同一同步中落后 16 logical px 的锚点。

## 实际自动结果

| 平台 / preset | 范围 | 结果 |
| --- | --- | --- |
| Windows / MSVC / `windows-msvc-headless-debug` | 20 个相关 target；Table、Tour、集合 i18n、投影缓存/重入/退役/重试、输入约束、浮层栈、主题、挂载、Select 和 Reactive 生命周期 | 55/55 CTest 通过，267.71 秒 |
| Windows / MSVC / `windows-msvc-headless-release` | 同一组 target 和测试集合 | 55/55 CTest 通过，47.71 秒 |
| Windows / MSVC / HEADLESS Debug、Release | 新增 `rynui.gallery_visual_table`：480→1440→728→480，真实文本输入、固定列可见范围与保留身份 | 各 1/1 通过；缩放不重跑 sample，草稿保留 |
| Windows / MSVC / `windows-msvc-release` / D3D12 / DXIL / system fonts | Gallery 原生自动交互：Table→Tour→Table→Tour，1440/960/728/480 与 Default/Dark/Compact；离页重入 | 4 场景、120 项检查通过；实际 GPU 提交与原生宿主输入区域/光标同步通过 |
| 同一原生环境，hidden readback | Table/Tour 四配置局部、滚动往返和 Tour 打开状态 | 24+12=36 张捕获，自动扫描零告警；选定宽/窄 contact sheets 与 Tour 遮罩面板已复核 |
| Windows / MSVC / HEADLESS Debug、Release | 同步 Table/Tour 目录后的 reference catalog、generator、contract | 各 3/3 CTest 通过；重新构建原生 Gallery 后，4 场景/120 检查再次通过 |
| Python / 目录与主题基线 | Gallery source 生成、目录 check/self-test、五套实际 resolver golden 的历史前缀比较 | 111 个 sample 生成成功；目录检查通过；全部旧字段、顺序和值不变，仅追加 Tour |

定向构建使用 `cmake --build --preset windows-msvc-headless-{debug,release} --parallel 2 --target ...`，随后按相关 CTest 名称筛选执行 `ctest --preset ... --output-on-failure`。五套 golden 由真实 Release `rynui_portable_theme_algorithm --write-goldens` 生成，未手工修改 JSON。上表分别记录对应源码阶段修复后的结果，不累计重跑次数。

原生使用 `tools/run_gallery_page_acceptance.py --page table --page tour --page table --page tour` 及 `tools/run_gallery_visual_acceptance.py --page table --page tour --cases 4`。首次脚本错误地将没有示例的 overview 放进 visual 序列；修正后分别运行交互与截图。自动交互选择有实际可见交集的虚拟行，避免把 overscan 行当作可聚焦编辑器；焦点断言检查最终归属，而非把重复聚焦的“未变化”当作拒绝。

截图发现窄屏两侧固定列挤占编辑区，示例现将固定列分别限制为可用宽度四分之一并保留宽屏上限。新增真实 Gallery Host 回归经 Debug/Release 通过后复拍；没有更改 Table 公开列宽语义或丢弃富内容。目录保留 partial，明确已覆盖的原生范围及 Web/平台边界。

捕获数量不等于逐图黄金比对数。以上不代表 Linux/Wayland/Vulkan、系统 IME、辅助技术、全部实际显示器 DPI 或人工桌面体验通过。
