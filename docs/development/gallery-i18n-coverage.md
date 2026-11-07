# Gallery 国际化源码覆盖

108 的 Gallery 迁移覆盖公开应用、壳、导航和实际示例，并与 ConfigProvider 消费者共同集成。源码检查只证明下述有界覆盖；实际平台结果单列在 [Windows 验收](../../openspec/changes/108-20261107-add-native-i18n/evidence/gallery-windows.md)，不以源码交接代替运行。

## 来源与操作

| 来源 | 源码覆盖 |
| --- | --- |
| `gallery_pages_host.cpp` | 顶栏语言按钮、独立 formatLocale/direction 设置、导航、搜索、状态筛选、目录、页面标题、TOC、来源与 Token/API 说明；切语言不请求 route |
| `gallery_page_controller.cpp` / `gallery_source_panel.cpp` | 失败、重试、返回、展开源码、复制及说明；源码正文与路径保持原文 |
| `gallery_i18n.cpp` | 2169 条显式双语声明和参数 schema；当前 73 个 catalog 身份的 name/summary/supported/missing 逐项核对，技术标识与旧验收边界保留 |
| `token_gallery_definition.cpp` | 112 个真实 sample 标题、操作、说明、属性与占位、事件反馈、历史 frame 导航与 reference cells；sample/action/target/source_function/public_header 身份保持 |
| 应用模型 | Tab/Menu/Select/AutoComplete/Mentions/Checkbox/Radio/Tree/TreeSelect/Transfer/Cascader/Tag/Steps/Timeline/Breadcrumb/Anchor、Table filter、Picker preset 的实际标签投影 |
| 新依赖工厂 | Table 原生固定列/虚拟行/筛选和 Tour 原生目标两工厂的标题、操作、说明、步骤、反馈；353 App 的 Modal/Drawer/Popconfirm 服务样例 |

原始模型来源通过 Memo 冻结，消息语言只投影标签，不重跑业务 factory/validator。菜单键、选项值、异步请求、排序与筛选来源、图片像素、上传内容和二维码 payload 保持业务身份。Table filter 与 Picker preset 的可选 `label_message`/`label_args` 在原标签位置投影，旧 literal 初始化保持有效。

`GalleryFeedback` 保存公开 MessageKey 与 typed MessageArgs，或明确的业务 String；contact/project/destination 保存结果、提交结果、媒体、上传和二维码状态均保存来源，由 mount 捕获的 I18n handle 显示。草稿编解码保存来源与参数类型，未将译文缓存为状态。整数和 Decimal 不经 double 中转；消息 schema、locale alias、fallback 与独立格式语言沿用公开合同。事件使用挂载时捕获的 handle。

Form 必填使用 builtin 来源；数量、日期、时间、FormList 与异步业务 validator 保留声明来源并用 `FormItemProps::errorText` 投影当前错误。`FormField::errorSource()` / `FormListRef::errorSource()` 显式区分 validator、rule、async、builtin 与 setFields literal；即便文本相同，也不猜来源。投影不改变 metadata、已交付 FormResult、required mark、Form 布局、校验次数、deadline 或受控决定。

ConfigProvider 页挂载只使用公开头的 `examples/i18n_app/i18n_app.cpp`，展示 UInt 最大值、精确 Decimal、命名参数、复数、独立内层 Provider，以及带真实 Input/Select 的 retained Modal。顶栏、旧 CLI、旧 frame 和全部旧回归入口保留。语言更新不重跑 build_page、不 remount；长说明采用真实布局尺寸与换行，没有用裁字隐藏遗漏。

## 精确 literal 与检查器

`tests/fixtures/gallery-i18n-literals.json` 当前登记 220 个精确 source/function/literal 来源，分类为业务 payload、确定性文本样本或代码/API。范围包括 editable 初始正文、人员名与业务值、合成 IO 错误、Typography/CJK/bidi/复制样本、二维码/水印 payload、代码与物理单位。未登记整页、整个组件或“所有中文”；操作、提示、菜单和选项标题不作为 fixture 放行。模型中保留的旧标签初值由显式键映射投影，不以原文猜语言。

`tools/check_gallery_i18n.py` 同时检查声明/schema、调用身份、112 个实际 sample、73 个 catalog、fixture 精确来源，以及直接文本/属性/反馈和具名模型 label 的显示 sink，输出文件、行号、文字。`tests/gallery_i18n_source_checker_tests.py` 的 5 个负例组验证中英文遗漏、拼接反馈、模型标签、显示 automation ID 和 fixture 来源不匹配。该工具是有界源码检查，不是 C++ 语义分析器，也不替代实际绘制和交互。

## 自动入口与证据边界

`rynui.i18n_gallery` 的 6 组真实 Host 场景保留原 3 组，并增加保存结果重投影、当前业务错误重投影、新 Table filter/Tour step 保留身份。原场景覆盖反向选区、preedit、undo、scroll、打开 Select、resize、导航退役、等值 catalog 和 idle。Form 新增同文本 setFields 反例，验证错误 provenance 与原 Form 区域/editor 保留。这些真实 Host 场景已进入正式 MSVC 自动验证，平台及运行边界见独立验收记录。

公开应用、Gallery hidden native 序列仍通过原 CMake 注册；`--page-i18n` 与 `--page-acceptance --page-visual` 同用，复用 hidden/offscreen、错误框抑制和有界 timeout。序列包含两语言、Modal/Select/Picker/Form/Table/Empty/公开应用、局部 RGBA readback、输入/弹层保留、导航退役/resize/idle。父任务执行命令与 target 清单随 ignored 源码交付包提供。

```text
python tools/run_gallery_page_acceptance.py out/build/windows-msvc/examples/Release/rynui_token_gallery.exe --preset windows-msvc --i18n --timeout 90 --log-dir out/108-gallery-completion/windows
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --i18n --timeout 90 --log-dir out/108-gallery-completion/linux
```

Linux runner 使用私有 headless Wayland compositor。Windows hidden D3D12/DXIL、系统字体、1.5 倍 DPI 的七条路径已实际运行；Linux、系统 IME、人工桌面及跨物理显示器仍按各自证据记录。不新增人工 checkbox。
