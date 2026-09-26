# 验收记录

规划阶段：用户确认参照 Ant Design 6 官方组件页，并要求可见滚动条。官方组件总览和 Layout 文档已核对；正文目录内容仍以仓库锁定的 Ant Design 6.6.5 数据为准。本 change strict validate 通过；全仓 strict 为 26/32，既有 change 013、015、016、017、018、021 失败。当前 OpenSpec CLI 不支持 `doctor --json`，报 `unknown command 'doctor'`；`git diff --check` 通过。此记录只代表规划与静态校验，不代表代码或真实窗口已完成。

滚动区基础：`GalleryScrollRange` 统一校验 extent、夹紧 offset，`GalleryScrollTranslation` 只平移指定 generation 有效的子树；文档 viewport 改用这两个对象但保留 section/category 锚点、resize 和诊断。平台通用测试在 Windows MSVC Debug 运行 `rynui.gallery_document_viewport` 与 `rynui.gallery_document_viewport_contract` 2/2 通过，新增双区域 offset 与 Node 子树隔离、同值不脏、失效根拒绝断言；源码合同已跟随迁移到独立 helper。本阶段尚未把独立滚动接入 Gallery 窗口，也未报告滚动条可用。

滚动条与装饰：`ReferenceSurface` 增加顶栏、轨道和滑块的 Gallery 内部角色，颜色取自 Theme，装饰角色不生成状态文案或交互；`GalleryScrollbarController` 支持指针拖动与轨道翻页，几何按可视比例及最小滑块长度计算。Windows MSVC Debug 定向构建成功，`rynui.reference_surface`、`rynui.gallery_document_viewport`、`rynui.gallery_document_viewport_contract` 3/3 通过；测试覆盖短内容隐藏滑块、比例/最小尺寸、拖动映射、轨道点击及装饰层语义。本阶段只完成组件，尚未接入 Gallery 布局和真实窗口。

布局阶段：导航由横向换行按钮改为纵向文档入口、七个类别及锁定目录的 73 个静态组件条目；站点栏作为最后绘制且优先布局的 56dp Theme 装饰面，位于两列之上。Windows MSVC Debug `rynui.token_gallery_frame` 1/1 通过，覆盖顶栏高度与纵向位置、导航 97 个直接子节点、宽窄列布局及组件 identity 保持。本阶段尚未把滚动和顶栏拦截接入运行时。

运行时滚动：宽窗口滚轮按逻辑坐标路由到左栏或正文；各自保持 offset、轨道和滑块。左栏/正文只平移各自 Node 子树，站点栏不平移；窄窗口改为整段内容共用正文 offset，隐藏左栏条、保留一条可见滚动条。轨道点击翻页、滑块指针捕获拖动、顶部输入拦截、resize 尺寸与锚点恢复已接入；导航 Button 在局部 Theme 下使用透明背景与边框。Windows MSVC Debug 定向 `rynui.token_gallery_frame`、`rynui.gallery_document_viewport`、`rynui.gallery_document_viewport_contract` 3/3 通过；新增单元测试覆盖列路由、装饰节点定位、导航/正文隔离与窄窗口共滚。Debug 真实 D3D12 `--scroll-acceptance` 和 `--scrollbar-acceptance` 均退出 0；后者日志确认左右滚动、拖动、轨道点击及滑块绘制/几何均通过。正式 Release 验收仍在下一阶段。

Windows 正式验收：用 `windows-msvc-release` 构建并在真实 Windows 窗口运行 D3D12/DXIL。第一次增量 Release 运行发生访问冲突；重新配置并完整重建相关 Release 单元后恢复原始 `/O2 /Ob2 /DNDEBUG` 与 `/INCREMENTAL:NO` 设置，随后 `--scroll-acceptance`、`--scrollbar-acceptance`、`--smoke`、`--input-acceptance`、`--selection-acceptance`、`--password-acceptance`、`--input-clear-acceptance` 7/7 退出 0。初次崩溃最可能与旧构建产物不一致有关，这是基于完整重建后消失的推断，尚未独立定位到源代码缺陷。滚动条验收确认左栏滚轮、左栏滑块、正文轨道、正文滑块及最终两列 offset 全部通过，四个滚动条图层几何和可见 draw 命令匹配。五次正式 240 帧滚动数据见 `gallery-scroll-release.csv`：首帧 CPU 54.7–58.6ms，滚动平均 4.157–4.159ms，p95 4.557–4.582ms，每次 785 个 buffer upload region，draw 命令累计 quad 4905–4935、glyph 5179–5211、effect 2800–2820。相较 change 031 的旧布局五次记录，滚动平均约 4.16ms 基本持平；可见固定目录与滑块带来更多 draw 与每帧约一个额外上传区域，内容及可视范围也发生变化，不能把两组数据当作同内容的纯性能回归比较。

集成收口：Windows MSVC Debug 全量构建通过。完整 CTest 首次为 229/235；六项失败均源于旧 Windows 工作区检出的 14 个被 SHA/golden 校验的文件仍为 CRLF。把这 14 个文件恢复 LF 后，Git 对象哈希与 `HEAD` 相同且无内容差异；六项定向复测通过，完整 CTest 重跑 235/235 通过（166.19 秒）。本 change strict validate 通过；全仓 strict 仍为 26/32，既有 change 013、015、016、017、018、021 失败，本 change 通过。当前 CLI 对 `openspec doctor --json` 报 `unknown command 'doctor'`。`git diff --check` 通过，工作树在提交前无其他内容差异。
# 用户反馈修正（2026-09-26）

## Windows 增量构建修复

- Release 启动复现 `0xc0000005`，模块偏移 `0x105b`；反汇编位于结构体内 `std::function` 的析构路径。修改 `TokenGalleryDefinition` 后的构建日志没有重编译 `main.cpp`，Ninja 对该对象记录 `#deps 0`。
- 本机只有 `2052/clui.dll`，`VSLANG=1033` 回退到中文；console output code page 为 65001、ANSI code page 为 936。CMake 自动探测把 UTF-8 `/showIncludes` 输出按 ANSI 解码，规则前缀成为乱码。新增配置探测使用真实输出代码页及 UTF-8 解码，规则恢复为 `注意: 包含文件:  `。
- 对 Debug 和 Release **分别** clean build。两者的 `main.cpp` 均记录 `#deps 15`，包含 `token_gallery_definition.hpp`。随后触碰该头文件，真实 Release 增量构建重新编译 `main.cpp`、`token_gallery_runtime.cpp` 与 `token_gallery_definition.cpp`。
- 清理重建后的 Windows MSVC Release D3D12 连续拖动在 100%、125%、150%、200% 四种 render scale 均 exit 0；每种 495 个阶段，包含 480 次跨帧移动及固定顶栏目录按钮。实际系统 DPI 为 125%，其余通过 Gallery 的 acceptance-scale 映射验证，未更改系统 DPI 设置。

## 裁剪边缘修复

- 扩展 `--scrollbar-acceptance` 后，修复前 Windows MSVC Debug 真实窗口稳定退出：`frame_error=Cannot pack a fully clipped rounded effect`（exit 5）。阴影 store 使用 1 logical px 的保守 AA 范围，GPU 使用 1 physical px；125% DPI 下只有保守边缘与 clip 相交的合法阴影被 GPU pack 拒绝。
- 新增 GPU resource 回归在修复前同样失败；修复后合法的完全裁剪实例上传透明零面积 quad，保留 store 索引与已组合 draw 顺序，重新进入视口时正常恢复。无效 geometry/metrics 仍按原合同拒绝。
- Windows `windows-msvc` / Debug 定向 CTest 10/10 通过，包含 rounded effect math/store/scene/GPU/resources、shader contract 和 Gallery frame/viewport。回归覆盖 100%、125%、150%、200% 以及返回 100%，窗口与 ancestor clip 两种边缘，往返进入/离开物理可见区。
- Windows `windows-msvc` / Release D3D12、实际 DPI 125%：492 阶段的滚动条验收 exit 0，左右列各 240 次跨帧移动，每步 3 次输入（含横向移出轨道），`continuous_drag=passed`，几何与 offset 逐帧一致。

## 排版与导航修正

- 共用 `GalleryLayoutMetrics` 计算两列、正文最大宽度、卡片列数与滚动条沟槽；左栏宽 216dp，正文最大宽 960dp，列间距 40dp，卡片间距 16dp。正文与滑块保持至少 16dp 空隙。
- 导航行高 36dp、组件条目 32dp，文字左对齐且不换行；主标题 28/40dp、分节标题 22/32dp，来源与设计理念采用文档排版，移除无关的状态外框。长支持范围占整行，规划条目随宽度以 1/2/3 列呈现。
- 左右列保持独立滚动，当前文档高亮随正文 section 更新；窄窗口初始定位正文，固定顶栏的目录按钮返回导航，再从导航跳回正文。
- Windows MSVC Debug 定向 CTest 6/6 通过（6.97 秒），包括 frame、reference surface 与 contract、document contract、viewport 与 contract。新断言覆盖文字不换行、导航/正文与滚动条不重叠、六个语义标题、响应式几何，以及高亮实际颜色从旧行移到当前行。
- 完整 Debug CTest 在整合布局与构建依赖修复后 235/235 通过（213.67 秒）；最后的设计理念样式及导出等待调整后复测上述受影响的六项，未重复完整平台通用测试。
