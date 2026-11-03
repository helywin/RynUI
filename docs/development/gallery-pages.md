# Gallery 组件页面

Gallery 的独立页面改造按 [change 105](../../openspec/changes/105-20261103-redesign-gallery-component-pages/design.md) 分阶段实施。页面模型、内部页面宿主和真实样例工厂已经实现；正式界面迁移仍按该 change 的未完成任务推进，内部夹具通过不代表原生界面已替换。

## 页面身份与目录状态

内部 [GalleryPageModel](../../examples/token_gallery/gallery_page_model.hpp) 为总览、设计介绍、Design Token 和 catalog 中每个 canonical identity 提供页面路由。Input、Search、Password、TextArea、OTP 对应同一个 Input identity 的五个分组；不增加目录条目。未知 identity 或不属于该组件的分组被拒绝，不静默切到另一页。

英文名称不区分 ASCII 大小写，中文名称和显式家族别名可以搜索。搜索与状态筛选只改变目录结果，不修改当前路由；当前页不匹配时由 `current_matches()` 给界面提供提示。目录顺序与七类计数来自实际数组。

唯一数据源仍是锁定的 [source manifest](../../gallery/ant-design/6.6.5/source-manifest.json) 和 [support overlay](../../gallery/ant-design/6.6.5/support-overlay.json)，由 [现有生成器](../../tools/update_ant_design_gallery_catalog.py) 生成离线 catalog。页面只引用原始 summary、supported_scope、missing_scope 和 evidence 标识符；native scope 与是否登记样例是不同概念。六种状态均有定义，即使当前某种状态数量为零。

目录状态计数不代表功能完成率或平台通过率；模型没有百分比字段，也不读取 tasks.md 或历史人工关闭记录。组件真正的样例归属及其工厂由后续页面登记层维护。

## 页面宿主与状态

[GalleryPageController](../../examples/token_gallery/gallery_page_controller.hpp) 是 example 内部适配器，依赖 `WindowComponentServices`，不是公开 Router API。shell 保留一个稳定的 page holder；页面内容由 `ReferenceSurfaceHost::append_slot` 建立正确的组件与 ReferenceSurface 构建上下文，每次只追加一个页面 root。卸载通过宿主 `destroy` 通知所有参与者；追加失败时由宿主回滚半挂载子树，再清理 ReferenceSurface 索引。

导航回调只调用 `request`；外围单个事件分发返回后再调用 `process`。不得在旧控件的回调栈内直接销毁页面。相同 page/family 的 section 或 sample 跳转保留 root，不压入页面历史。后退/前进保存 page/family；草稿以该身份分组，页内位置保存稳定 identity 和局部距离，实际重排后的夹紧由 viewport 完成。

`GalleryPageDraft` 只允许字符串、布尔、整数、数值和字符串列表，不接受组件/ref/Signal、编辑器或场景句柄。factory 以这些值重建新状态；save hook 只读取已提交业务值。Password/OTP 不调用 save hook，离页清除敏感值，但可保留与输入值无关的页面滚动锚点。不能把整个样例状态对象存入草稿，也不能恢复旧 preedit、选区、撤销栈或 popup open 状态。

后续组件的业务值使用内部 [typed codec](../../examples/token_gallery/gallery_draft_codec.hpp) 转成上述基础值：保留 optional 的空值、variant 的类型、选择顺序和完整无符号整数；日期/时间重新经过公开值类型的合法性检查。它不支持 Prop/ref/callback 等运行状态。页面字段按 canonical page/family 显式登记；普通非受控控件通过内部构造上下文恢复 default 值并组合原回调，空 Select 值不会被初始默认值覆盖。受控 Signal/FormField 保持原有接受与拒绝规则。

退役先使旧 generation ticket 失效，再保存草稿、取消属于旧 root 的 pointer 状态与 focus，调用 `before_retire` 取消 Gallery 自有 scrollbar drag，然后通过宿主销毁页面。既有输入参与者负责取消 session/preedit、弹层、keyboard press、caret 和动画。异步/延迟操作捕获弱 `GalleryPageTicket`，检查 `valid()` 后才能访问页面。挂载或保存失败仍清理旧资源，并显示可手动重试的说明页；不会自动重建忙循环。

页面代码只使用 `ryn::*Props`、typed slots 和现有公开组件；内部 host、controller、ticket 与稳定测试身份不进入 `include/ryn/`。稳定身份按 page → family → sample → test 的层次登记；与文案、挂载序号、Node/Interaction/Scene identity 无关。后续样例登记层负责旧 test ID 的唯一归属映射，切页后所有运行时句柄必须重新解析。

## 样例登记与迁移

[GallerySampleDescriptor](../../examples/token_gallery/gallery_sample_registry.hpp) 登记稳定 sample identity、canonical page/family、标题、实际 C++ 构造函数和公开头文件。当前登记 48 个组件页的 63 个真实样例组，以及 Design Token 页的 4 组主题、色板、尺度和阴影。其余有原生范围声明的 Notification、ConfigProvider 提供关联入口，分别指向 Message 联合流程与 Token 页；没有制造独立实现。每次只运行对应 page/family 的构造函数；其他条目的目录状态不因暂未迁移样例而改变。[旧身份清单](../../examples/token_gallery/gallery_test_inventory.cpp) 为全部 458 个历史 ID 指定唯一主 sample，并显式列出 4 项 replacement；caption/Token 同样有 sample 归属，不伪称都对应交互控件。四项 replacement 在实际活动页中解析并检查所属 sample。

活动页的 `target(test_id)` 直接解析构造时登记的组件身份。登记只发生在 mount，并由所属组件的 resource cleanup 删除；测试通过 sample owner 验证归属。滚动和普通属性更新不扫描正文文案，不使用全局 mounted 数组偏移定位目标。这些运行时 ID 只属于当前页面状态，不进入 `GalleryPageDraft`。

普通非受控 Input/Search/TextArea 用保存的已提交值设置新实例的 `defaultValue`，并组合原有 change/submit 回调；受控值仍使用原有 Signal/FormField。TreeSelect 保存选择、交付备注、表单值和展开键，open、focus 与 session 总是重新开始。代码片段的展开 UI 和公开 API 内容由后续页面 shell 任务接入，目前登记的函数/头文件映射不等同于已展示代码片段。

Typography/Divider 的真实实例已提取为各自页面工厂；总览不需要调用它们。Bidi 的 Text/Paragraph 归 Typography，编辑器归对应 Input 家族；方向控制有各页自己的目标 ID。Pagination 独立页使用 `gallery.pagination.controlled`，Table 的联动分页保留同一构造能力并使用 `gallery.table.pagination`，防止把同一历史 ID 重复归属两页。含 Space/Radio/Checkbox 的页面宿主仍须建立既有 `SelectionComponentHost`，和旧原生 Gallery 的宿主装配一致。

Table 页是原有编辑、排序、选择、分页、Menu、Dropdown、InputNumber 流程的主归属；其依赖控件使用 Table 前缀的目标身份，独立组件页保留自己的基础示例并提供关联入口。Modal/Drawer、Card、Tabs、Collapse 等内容中的普通已提交编辑值可恢复；弹层 open、loading 请求、focus、IME 和挂载身份不存入草稿。Token 页面通过主题请求接口交给后续 shell 应用全局设置。样例容器使用 example role，不把默认 planned 状态徽标误标到真实演示上。

## 定向自动检查

```sh
cmake --build --preset linux-gcc-release --target rynui_gallery_page_model_tests rynui_gallery_document_model_tests rynui_ant_design_reference_catalog_tests
ctest --preset linux-gcc-release -R '^rynui\.(gallery_page_model|gallery_document_model|ant_design_reference_catalog|ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract)$' --output-on-failure
```

这里均为平台通用逻辑，不打开窗口，不代表 Linux/Windows 的原生 GPU 或系统输入服务验收。测试代码见 [页面模型测试](../../tests/gallery_page_model_tests.cpp)。

页面生命周期的定向入口如下：

```sh
cmake --build --preset linux-gcc-release --target rynui_gallery_page_model_tests rynui_gallery_page_controller_tests
ctest --preset linux-gcc-release -R '^rynui\.gallery_page_(model|controller)$' --output-on-failure
```

[生命周期夹具](../../tests/gallery_page_controller_tests.cpp) 使用真实组件宿主、模拟文本平台和记录上传的 GPU adapter，覆盖草稿、敏感清空、受控弹层退役、迟到文本/组合输入、capture/键盘按压、scrollbar drag、回调重入、失败回滚、主题/resize 身份以及稳定后无新增上传/帧请求。它不运行 SDL、真实 IME 或 GPU 驱动。

[样例页面夹具](../../tests/gallery_sample_page_tests.cpp) 的定向入口为 `rynui.gallery_sample_page`（target `rynui_gallery_sample_page_tests`）。它遍历全部 73 个 canonical 路由和 Token 页，验证真实工厂隔离、关联路由、稳定 ID、编辑与跨页草稿、Button 变体与延迟加载/ref、Search 键盘提交、OTP 粘贴分发/敏感清空、TreeSelect 选择/Form/受控拒绝，以及同页 Theme/resize 的身份保留。

已有 frame binary 的 `--input-only` / `--tree-select-only` 现已直接挂载相应页面工厂，并用登记目标定位；对应 CTest 为 `rynui.gallery_page_input_frame` / `rynui.gallery_page_tree_select_frame`。Input 家族按组依次追加/退役，保持编辑、剪贴板、提交、场景身份与 idle 检查；TreeSelect 保留三种主题与 1200/560 宽度的 glyph/fill/hit/focus/popup 检查。其余组件渲染用例已改为从登记工厂构造所需页面，Card/Tabs/Modal 编辑器与 Select 直接使用 test ID，阴影、目录卡、章节与滚动条按具名身份定位；不再断言全页 307 live samples 或其前后固定数组偏移。涉及多组件的历史渲染断言使用显式路由列表的内部兼容夹具，单活动页与退役仍由页面夹具验证。兼容夹具保留旧目录外壳，只用于有限自动回归，不是第二种交互产品模式。新目录 shell 的交互/几何由后续 viewport 阶段验证。

迁移后的组件 frame 定向入口为 `rynui.gallery_page_<group>_frame`，例如 `ctest --preset linux-gcc-release -R '^rynui.gallery_page_time-picker_frame$' --output-on-failure`；修改局部页面时只运行受影响组。TimePicker 的选中填充同时检查 Quad 与共同裁剪填充 RoundedEffect，颜色、实际覆盖和列/弹层边界仍受断言约束；window layer 按实际窗口边界检查，正文保留自己的 content clip。
