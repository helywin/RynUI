# Tasks

以下为本 change 的 apply 跟踪；只有对应实现和自动验证完成才勾选，未完成阶段不代表已实现。按依赖顺序推进，每阶段仅构建相关 target、运行受影响自动检查，结果记录实际平台/preset/configuration 与数量，原始产物只在 `out/`。C++ 修改阶段使用 clang-format 22.x，并运行 `python scripts/format-code.py --check` 与 `git diff --check`。

平台通用测试只选一个平台完成。默认采用 `windows-msvc-headless` Debug/Release（Ninja Multi-Config）；若通用逻辑已在 `linux-gcc` 完成，Windows 只补受影响 HEADLESS/MSVC 工具链集成，不重复整套通用 suite。Windows/Linux 原生结果独立完成，缺环境或失败不代打勾。

## 1. 页面模型与真实状态（平台通用）

- [x] 1.1 实现 Gallery 内部 page/route/section/family 模型及 catalog 到 descriptor 的完整生成，保持 6.6.5 锁与 73 个 canonical identity；完成标准为 1.3 的唯一性、全目录可达与缺省说明页断言通过。
- [x] 1.2 实现中文/英文/家族别名搜索、六状态筛选、目录统计与当前页不匹配提示；显示原始支持/缺失清单，不产生能力完成率；完成标准为 1.3 的筛选、别名、零结果和无百分比断言通过。
- [x] 1.3 新增/更新 model/catalog 定向自动测试，覆盖 7 类计数、73 页、Input 五别名、未知 identity、全部六状态（包括零数量夹具）、筛选不改当前页；构建 model/catalog targets，运行 `rynui.ant_design_reference_catalog`、catalog generator/contract 和 `rynui.gallery_document_model` 及新增页面 model 测试，记录结果。
- [x] 1.4 增加页面/状态语义与历史长文档迁移的简短开发说明，自动校验来源路径、版本和文档链接，并运行 OpenSpec doctor/strict 与 diff 检查。

## 2. 页面宿主、草稿与退役（平台通用，依赖 1）

- [x] 2.1 实现独立 shell/page root、ReferenceSurface 的内部 append/prune adapter 与安全事件点的导航控制器，复用 services append_slot/destroy；完成标准为 2.4 的仅当前页挂载、shell 身份稳定及异常清理断言通过。
- [x] 2.2 实现按 page/family 保存的 typed 草稿、页面历史与锚点恢复，普通编辑保留已提交值、Password/OTP 离页清空、不保存旧 ref/IME/撤销栈；完成标准为 2.4 的往返与敏感状态用例通过。
- [x] 2.3 接入切页时 popup/boundary、pointer capture/scrollbar drag、focus/keyboard press、IME/preedit、caret/动画/deferred ticket 的 owner/generation 退役；完成标准为 2.4 的迟到事件、同批事件、拒绝关闭、重入和挂载失败用例通过。
- [x] 2.4 新增页面 lifecycle/Recording 自动夹具，覆盖 Button→Input→TreeSelect→Upload 往返、旧页活资源归零、草稿恢复、主题/resize 不重挂、failure fallback 与 idle；构建相关 frame/lifecycle targets 并运行定向 CTest，保留真实失败信息。
- [x] 2.5 编写内部页面状态/销毁顺序说明及稳定 ID 合同，自动校验示例中的内部/公开接口边界和链接，运行格式与 diff 检查。

## 3. 逐页迁移真实样例（平台通用，依赖 2）

- [x] 3.1 提取 Button、Input 五分组和 TreeSelect 样例及 C++/头文件映射，保留现有 typed Props、controlled/uncontrolled 与流程语义；完成标准为 3.2 的原有行为断言及 factory 隔离通过。
- [x] 3.2 将上述组件的 Gallery frame 用例改为 page/sample/test ID 定位，自动验证 Button 状态、Input/Password/Search/TextArea/OTP 编辑与草稿、TreeSelect 目标/备注/Form/受控拒绝；运行对应页面定向用例及受影响组件测试。
- [x] 3.3 迁移通用/布局/导航真实样例，包括 Typography/Divider 从目录卡移出、Flex/Space/Icon、Breadcrumb/Steps/Menu/Dropdown/Pagination/Tabs；完成标准为 3.4 的归属和原有交互断言通过。
- [x] 3.4 更新这一批样例的页面自动用例，验证唯一主归属、关联入口、无关页面 factory 零调用和原有状态行为，构建相应 targets 并运行定向 CTest。
- [x] 3.5 迁移其余数据录入/展示/反馈样例与 Theme/Token 内容，拆分混合 add_* 函数，指定跨组件流程的主归属；对未实现/已弃用条目生成说明页；完成标准为 3.6 的全部旧 ID 映射与行为测试通过。
- [x] 3.6 自动核对旧 458 个 stable test ID 的 page/sample 或 shell/文档归属，校验 replacement 清单，移除全页固定 307 live samples 和全局 mounted 序号假设；执行迁移页面的定向 Gallery 测试，保留原功能断言而非仅更新数量。
- [ ] 3.7 为已登记样例提供可展开的原生 C++ 片段、真实 public header/API/token 入口，自动验证片段与样例出处、公开符号和有效路径；本阶段只检查登记片段，不生成未实现 API 或完整 Web API 表。

## 4. 文档 shell、滚动与交互（平台通用，依赖 3）

- [ ] 4.1 实现顶栏、目录搜索/筛选/分类、总览、组件标题/状态/能力区、示例卡与 TOC，参考 design 尺寸和主题映射；完成标准为 4.5 的内容/几何/导航断言通过。
- [ ] 4.2 实现宽/中/窄布局、窄目录输入边界、代码区域横向滚动、Default/Dark/Compact 及主题设置；完成标准为 4.5 的 1440/960/728/480/360 布局、theme identity 和无不可达内容断言通过。
- [ ] 4.3 将固定六 section/category 全文锚点改为当前 page generation 的具名锚点，替换 scrollbar 尾部索引与全局文案扫描；保留嵌套输入优先 wheel、目录/正文独立 scroll 和最小 translation；完成标准为 4.5 的 clamp/恢复/消费路由断言通过。
- [ ] 4.4 实现 Tab/Shift+Tab、Enter/Space、Ctrl+K、Escape、进入示例/TOC 控件与可见焦点恢复；composition 和模态边界内不抢键，编辑器拥有其 Page/Home/End；完成标准为 4.5 的键盘/边界用例通过。
- [ ] 4.5 扩展 gallery viewport/frame 自动回归与源合同，覆盖 resize 往返、导航与正文独立滚动、过滤不卸页、TOC 不切页、popup wheel、焦点显露和窄目录关闭；构建相关 targets，运行 `rynui.gallery_document_viewport`、相关 contract 及新页面/键盘用例。
- [ ] 4.6 更新 Gallery 使用说明和测试入口文档，自动检查现行页面入口、snippet/TOC identity 与命令参数一致，运行格式、OpenSpec doctor/strict 和 diff 检查。

## 5. 工作量边界与自动平台入口（平台通用，依赖 4）

- [ ] 5.1 为现有 Gallery benchmark 增加 page mount/switch 和真实 `GalleryEvents::inject_scroll_at` 分发计数，区分输入、布局、同步、上传，记录所用配置；完成标准为 5.2 的工作量和资源断言通过，不以直接 scroll_by 代替输入路径。
- [ ] 5.2 使用同一 preset/configuration 运行定向 benchmark 与 Recording 回归，验证未访问页 factory 零调用、加未访问样例不增加当前页样例工作、往返无活资源增长、正常稳定 idle；只记录分项实际结果，不添加固定 FPS 或改善百分比断言。
- [ ] 5.3 实现 `--page-acceptance` 有限自动入口及相应 runner/schema：指定页/分组导航、resize、主题、popup/输入退役、绘制/host geometry 断言，输出路径限制在 out；完成标准为参数/退出码/超时/证据 schema 自动测试通过。
- [ ] 5.4 在选定通用平台用 CMakePresets 构建相关最终 targets，执行受影响页面、catalog、viewport、lifecycle、frame、benchmark/contract 集成集；记录实际 preset、Debug/Release 与通过数，确认 Core/renderer 依赖守卫保持，无全量 CTest 或原生矩阵要求。

## 6. Windows 自动平台集成（依赖 5）

- [ ] 6.1 在实际 Windows/MSVC 机器以 `windows-msvc` Release 构建最终 Gallery，自动执行 `--page-acceptance`，检查页面切换后的 D3D12/shader/clip、系统字体、host input area/cursor 与 resize；另完成本次受影响的 `windows-msvc-headless` Debug/Release 构建/定向集成，若已由通用阶段完成则复用该结果。
- [ ] 6.2 生成 Windows 精简结果摘要，自动验证 preset/backend/命令/通过失败数及未验证边界字段；原始日志与图像只写 out，不把系统 IME 候选窗或其他 DPI/平台写成通过。

## 7. Linux 自动平台集成（依赖 5，与 6 独立）

- [ ] 7.1 在实际 Linux 机器以 `linux-gcc` Release 构建相关最终 Gallery，通过 `tools/run_background_wayland.py --log-dir out/... --timeout <有限秒数> -- <binary> --page-acceptance` 自动检查 Wayland/Vulkan、系统字体、host input area/cursor、popup 和 resize；环境失败/超时记录原始原因，禁止回退用户桌面。
- [ ] 7.2 生成 Linux 精简结果摘要，自动验证实际 backend/preset/数量和环境边界；注明 Mutter headless、dbus-run-session、Wayland/Vulkan 依赖和未覆盖的真实显示器/系统 IME 范围，原始产物只写 out。

## 8. 文档与结果收口（平台通用，依赖已完成的要求项）

- [ ] 8.1 自动汇总本 change 已要求的通用、Windows、Linux 结果，缺失/失败项保持未完成；验证没有从任务勾选、历史人工管理关闭或 HTML 原型推导组件完成度/平台通过。
- [ ] 8.2 运行 `python scripts/format-code.py --check`、`openspec doctor --json`、`openspec validate --all --strict --no-interactive`、`git diff --check`，自动核对变更未包含 out 原始产物与非本阶段文件，记录精简结论。
