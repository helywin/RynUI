# Tasks

平台通用合同只记录实际使用的一种受支持平台；本机使用 Windows/MSVC HEADLESS Debug/Release、Ninja Multi-Config，最多 2 compiler。各小阶段通过后独立提交并推送。实现与本阶段自动检查都完成才勾选，不把 source-only 或规划计为功能完成。

## 1. i18n 数据与唯一配置基础

- [ ] 1.1 实现 immutable LocaleId/Pack/Catalog、完整 zh-CN/en-US、fallback/alias/诊断与 typed 应用消息/参数/整数复数；新增并运行 `rynui.i18n_catalog`，覆盖缺 key、schema、转义、非法重复输入与语义相等，并更新数据/API 文档。
- [ ] 1.2 实现窗口隔离 ConfigScope 与唯一 ConfigProvider 的 locale/formatLocale/messages/direction/size/disabled/theme 来源和嵌套继承，覆盖所有 slot/append/透明路径；运行 `rynui.i18n_context` 验证窗口隔离、作用域生命周期、重入、24 层与等值零额外帧，文档说明覆盖优先级。
- [ ] 1.3 实现公开 `useI18n`、纯 lookup/format 入口与响应式文本绑定，支持过期 handle 的明确失败；运行 `rynui.i18n_public_api` 的独立公开头/应用编译及文本身份回归，完成公开 API 使用说明。
- [ ] 1.4 在正式 HEADLESS Debug/Release 构建运行阶段 1 新入口及受影响 Prop/reactive/Theme/scope/10k 等值分配回归，保留原五套 Theme golden；运行格式、doctor、strict、diff，记录实际数量与边界后提交。

## 2. 组件文案、格式与编辑保留

- [ ] 2.1 接入 Modal/Popconfirm、Empty/Select/TreeSelect/Mentions/Cascader 等实际内置文案，保留 slot/显式 Locale/英文与空串优先；运行 `rynui.i18n_components_defaults`、`overrides` 及对应旧组件测试，更新组件文档。
- [ ] 2.2 接入 Picker/Range/TimePicker/Calendar 的 placeholder、面板、月份/年、确认取消与格式语言，保留编辑冻结及同 key 单元格；运行 `rynui.i18n_components_pickers`、`editors` 和受影响既有 Picker/Calendar 测试，记录解析/格式边界。
- [ ] 2.3 接入 Pagination/Table/Transfer/Descriptions 与媒体 Upload/Image/QRCode，修复绕过 Locale 的计数/错误显示，业务字符串保持原文；运行 `rynui.i18n_components_collections`、`media` 和对应 retained/controlled/cleanup 旧回归，更新各组件说明。
- [ ] 2.4 在整合 StatisticLocale 后抽取精确 number helper，接入 Statistic/InputNumber/ColorPicker 等实际数字与默认文案，保留显式 formatter/parser/separator、整数/Decimal/负零语义；运行 `rynui.i18n_components_formatting` 与既有数字合同，文档明确有限能力。
- [ ] 2.5 实现 Form builtin/literal 来源、required 重载、optional rule message 迁移与当前 issue 重投影，保持已返回结果快照/validator/async/dirty 语义；运行 `rynui.i18n_components_form`、公开编译及受影响 Form 旧回归，提供迁移说明。
- [ ] 2.6 正式 HEADLESS Debug/Release 完成各组件族新测试与受影响旧测试，验证实际输入/反向选区/composition/undo、弹层 open/clip/hit 和回调退役；运行格式/doctor/strict/diff，记录分族数量并分阶段提交，不重复无关全量矩阵。

## 3. 真实 App 与服务上下文集成

- [ ] 3.1 依赖 107 的实际 App/Notice/Modal/Drawer 服务与输入域合同，在 append/update/late completion 传递 captured theme/config/window owner generation；运行 `rynui.i18n_service` 的嵌套/双窗口/显式 String 与 Binding 场景，更新服务上下文文档。
- [ ] 3.2 验证 Provider/App 销毁、跨窗口 ref、重入/抛错、future 只完成一次、晚到回调及输入租约释放，运行新服务失败/清理 cases 与 Notice/Modal/Drawer 既有边界回归；依赖未就绪不得勾选或用 Holder 替代。
- [ ] 3.3 正式 HEADLESS Debug/Release 完成服务组合检查，运行格式/doctor/strict/diff，精简记录实际结果并提交；107/108 引用同一份实现证据，不重复造 Provider/App。

## 4. 公开应用与 Gallery 双语

- [ ] 4.1 交付仅公开 API 的真实应用例子，覆盖参数/复数/格式语言、嵌套 Provider、独立 direction 和 retained 富内容；运行 public API/例子编译及对应交互合同，完成 docs/i18n.md。
- [ ] 4.2 将 Gallery 壳/导航/说明/示例操作文案接公共消息词典，保持稳定 ID/源引用及登记的 literal fixture；运行 `rynui.i18n_gallery` 校验中英 key/参数覆盖，且切换不 build_page/remount、不丢业务状态，并维护覆盖清单。
- [ ] 4.3 完成上述 Gallery 实现的 source/catalog 生成检查及受影响 page/controller/host/frame 合同，保留原有参数入口；运行格式/doctor/strict/diff，记录精简结果并提交。

## 5. Windows 自动平台集成

- [ ] 5.1 正式 Windows/MSVC native preset 构建公开例子与 Gallery，后台 hidden D3D12 运行 zh/en 切换和局部 GPU readback，覆盖 Modal/Select/Picker/Form/Table/Empty 等实际路径及混合脚本/分数 DPI；记录有效捕获数量与实际 driver，不以 CPU 测试替代。
- [ ] 5.2 同一后台序列验证打开弹层、输入/选区保留、resize/回滚重入、导航退役和 idle 零新增 deadline；错误框抑制、SDL hidden 和超时均生效，原始输出只在 out，精简记录 Windows 边界并提交。

## 6. Linux 自动平台集成

- [ ] 6.1 在实际 Linux 机器正式构建，经 tools/run_background_wayland.py 使用独立 headless Wayland/Vulkan 运行两语言局部 readback、输入/DPI/弹层和退役序列；只记录真实 Linux 结果，不连接用户桌面、不重复平台通用逻辑矩阵。
- [ ] 6.2 核对 Linux 实际 font/shaping、GPU 和窗口集成结果，原始产物只在 out；独立记录平台数量与未测边界并提交，不以 Windows 结果勾选。

## 7. 最终集成

- [ ] 7.1 在所有通用阶段就绪后运行一次实际 HEADLESS Debug/Release 集成检查和目录/文档校验，核对 107 共享实现引用、Theme golden 与公开迁移说明；只汇总本 change 实跑的平台证据，不重复已通过合同或新增人工门槛。
