# 104 验收结果

## 平台通用回归

- Windows / MSVC / Ninja Multi-Config；正式 `windows-msvc-headless-debug`、`windows-msvc-headless-release`，最多 2 jobs，构建 38 个受影响 target。
- 对应注册 CTest：Debug **61/61**，Release **62/62**。涵盖 Quad/Effect 打包与 coverage、Text/Icon/Input/OTP/InputNumber、Selection、Tree/TreeSelect/Table、Tooltip/Button/Modal/Alert/Progress/Avatar/Timeline 与布局。
- 收尾 Switch 内容按压连续性修改后，重新构建 Switch、Selection、Radio component/features，Debug、Release **各 4/4**；不是把前面的所有集成项冒称最后一次统一重跑。
- 零分配性能合同使用 Release：Input/Scene 两阶段各 20,000 帧，Core/Quad/Glyph/Effect 分配均为 0。Debug 的零分配探针另见已有空 vector 的 MSVC 调试代理分配，不纳入上述 61 项逻辑通过数；未修改零分配断言。
- backend 分层及 Icon 资产合同 **2/2**；clang-format 22.1.3 检查 **814 文件、0 失败**；OpenSpec doctor healthy、strict **104/104**、diff check 通过。

## Windows 原生

- 正式 `windows-msvc-release` 重建 Gallery、shader 和最终原生 RC。字体、platform lifecycle、generated/embedded shaders、三种 shader 合同及 Gallery 构建合同 CTest **8/8**。
- 系统 Symbol/Emoji 新回归对原字体链实际失败，修复后真实 shape 和灰度 bitmap 通过；样例 Unicode/字素计数未替换，普通启动仍使用系统字体。
- D3D12 / DXIL，系统及渲染缩放 **150%**：Input features（含真实系统 I-beam）、Button、Checkbox、Radio、Switch、OTP、Tooltip content、Slider、Modal **9 个原生入口通过**；最终 Switch 再跑通过。程序内输入注入不冒充真实系统 IME 验收。
- 真实鼠标移入编辑区选择 I-beam；禁用输入及清除动作恢复默认箭头。平台测试覆盖创建失败重试、释放、离开、detach 和另一窗口已选择 cursor 的情况。
- 普通系统字体 Gallery 的 Input、Switch、Tree、InputNumber、Alert、Progress、Avatar、Timeline、Badge 和 Table 共 **10 个目标画面**已读回检查；快照入口按真实滚动根定位，断言目标确已进入 viewport。专门验收的锁定字体由显式非 ALL 准备目标提供。
- 实际 GPU 检查覆盖本机 150%；1/1.25/1.5/2 倍及多像素 phase 的 draw-domain/source-over 属于平台通用回归，不冒充四种原生 DPI 实测。
- 最终 Gallery `--scroll-acceptance` **240/240 步、exit 0**；本机 150% 的首次显露滚动样本平均 **11.792 ms/帧**、P95 **13.882 ms**，包含首次显露字体光栅/上传，不能作为稳态或整页 240Hz 达标证据。

## 25 条报告对应

| 报告 | 处理与验证 |
| --- | --- |
| 1 问号 | 补 Windows Symbol/Emoji 候选；系统字体实际画面已显示原 emoji。 |
| 2、24 文字/控件对齐 | 紧行高采用对称 signed leading；Alert 空 description 退出布局，保留节点；中英及真实字体度量回归。 |
| 3 输入鼠标形状 | 共用编辑命中查询接入 SDL 系统 cursor；Input/TextArea/Password/Search/OTP/InputNumber 及门禁/动作区域回归，实际 I-beam 检查。 |
| 4、25 Switch | 小 RTL 样例改用无圆底 CloseOutlined；开关内容与手柄共用连续位移，按压使用已呈现 inset；覆盖中间态、反向、按压/释放、受控更新、RTL/Small、门禁、身份和停稳。 |
| 5、17 Tooltip 接缝 | 主体和四向箭头用单次联合 coverage，避免 glyph 相位差及透明重叠；Slider/Tooltip 原生入口与 alpha 0.85/1 回归。 |
| 6 Table 填充穿表头 | 按实际祖先 content clip 转换 surface/content 填充；真实 Checkbox/InputNumber/Tag 的 fill、glyph、hit 及窗口层转换退出回归。 |
| 7、14 InputNumber | 满足分配宽度后布置 editor/尾端动作；动作覆盖输入框内侧高度、贴右沿并保留圆角；真实 glyph 中心及门禁/retained 回归。 |
| 8、9 Tree | 内部 wrapper 按已分配 slot stretch，恢复 16dp checkbox 与 24dp switcher；普通/富行以及复用 Tree 的 TreeSelect 回归。 |
| 10 虚线 | 直线和圆角共用连续周长 phase；实际虚线 shader 画面检查。 |
| 11、13、15、18、19 圆形/矩形边缘 | Quad 单独扩展一物理像素 draw guard，保留 shape/半径；统一一物理像素 AA，覆盖方/圆角/圆形、描边及 source-over。 |
| 12 Modal | Gallery loading 正文显式纵向间距和按钮 focus 留白；独立 Modal 组合检查布局、正文和焦点四边，原生 Modal 流程通过。 |
| 16 Timeline | 锁定 LoadingOutlined 本为约 1dp 的四分之一圆弧，线宽保留；补可见 loading 的旋转，离屏/隐藏/完成/减少动画停止，身份及 clip 回归。 |
| 20 RadioButton 共享边 | 全部背景先于边框，共享边单一所有者、选中边最后发布；竖排 false/true/数字及横排/RTL 多 phase source-over 回归。 |
| 21 OTP | 空格 caret 先以自身矩形中心定位再做设备像素取整；尺寸、DPI、phase、mask 和身份回归。 |
| 22 Dashboard | rail/进度使用相同有限圆弧和圆端帽，保留 75° 开口；0%/部分进度逻辑回归，原生检查 35% 画面，移除水平平切。 |
| 23 Avatar | 修紧行高文字居中及 group border-box；正常 AvatarGroup 负间距重叠是设计意图，未取消；文字缩放、身份与实际画面检查。 |

## 边界与交付

未宣称整页 240Hz 达标，未代验 Linux、其他 GPU、彩色 emoji、真实系统 IME 或 AT。字体阶段已独立提交；其余实现与结果按本阶段提交。原始 log、样本和图片仅留本机已忽略的 `out/`，仓库没有原始产物附件。
