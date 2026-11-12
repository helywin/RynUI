# Proposal

## Why

Gallery 现有工具能自动运行有限动作、检查部分合同和捕获原生 GPU 图像，但一般视觉结果仍是 `pending image inspection`，没有固定官方 Web 参考与独立机器 oracle 的端到端判定。用户需要能自动发现差异、定位、修复后回归的测试系统，不能让用户或 Agent 逐页采图、读图成为通过条件。

## What Changes

- 建立稳定 case identity 与覆盖账本，连接固定 Ant Design `6.6.5` / `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的官方 demo/源文件、当前 Gallery canonical 页、Input 家族、真实 sample/target 和合同维度；漏映射、漏执行不得当作通过。
- 提供单命令差分 runner：选择 case、校验/准备 Web 参考、在独立 headless browser 与隐藏原生窗口执行同语义动作、采集图像/几何/状态/事件、自动判定并返回正常非零错误码。Linux 复用私有 Wayland；所有输出与可重建参考缓存只在 ignored `out/`。
- 分层比较布局、文本、图标、Token、像素与动画取样；用单独的语义 oracle 比较受控状态、焦点/弹层、滚动/编辑、异步、动态拓扑、离页清理与 idle。容差通过校准和故障注入检验，不能靠全图宽阈值或大片 mask 消除失败。
- 输出可定位的分类差异与复现命令；修复后自动运行原失败 case 和依赖影响集。Agent 只消费失败并修复，不承担必需采集或裁决步骤；参考基线迁移不随 native 失败自动发生。
- 先以 Button、Input、Select 的真实注册样例跑通最小端到端 pilot，证明真实外观/逻辑故障注入红、恢复后同命令绿，再扩展全目录账本与现有原生范围。建立局部回归、CI 全覆盖和机制 meta tests。

## Capabilities

### New Capabilities

- `web-native-demo-differential-testing`：固定官方参考、覆盖映射、隔离执行、分层机器判定、故障定位与回归的自动化合同。

### Modified Capabilities

无。`openspec list --specs --json` 在规划 checkout 返回空列表；105 的页面/捕获合同和 108 的国际化合同继续保留，本 change 增加测试系统，不改写其任务向量。

## Impact

- 后续实现涉及 `tools/` 中独立 Web 参考工具与 Python 调度/比较器、`tests/fixtures/` 中确定性输入/schema、`examples/token_gallery/` 中现有 finite acceptance 的 scenario/观测接口、相关 CMake 测试登记及开发文档。生产 Core 不依赖 React、DOM、Playwright 或 Node；继续使用既有输入、Theme、AnimationRuntime、SceneResources 与 renderer。
- Web 测试依赖独立锁定版本、integrity/source SHA256、license、browser revision 和安装方式，不能变成产品 npm 运行依赖。原始源码副本、截图、运行 JSON/日志和 baseline captures 不入库。
- 规划检查基线为 `48033d58061a540514ed7f60941e9880fc9b18d5`。只读源码盘点为 73 目录项、75 组件/家族路由、110 组件 sample，另 4 Token sample；数量是该 SHA 的注册观察值，运行时枚举与覆盖集合才是后续验收依据。List 和 Util 的显式边界不允许扩展成其他已有 native 能力的豁免。
- 非目标：复刻官网导航壳、将全部浏览器 API 移植到 C++、新增渲染/输入框架、替代 OS IME/AT 或实际桌面呈现验收、接管 107 Progress 数值研究与 Listy/Table/Other 实施。旧 2026-11-06 审查是历史采集记录，不能当作当前仍失败清单。
- 主要风险是样例数据不等价、字体/栅格和色彩域差异、错误归一化、参考漂移及未执行被误报成功；具体 fail-closed、校准和负例合同见 design/spec。Linux 与 Windows/MSVC/D3D12 的实际自动集成分别验收，不互相替代。
- 本次只交付 OpenSpec 规划；所有实施任务保持未完成。当前协调状态按用户交接为 107 `124/135`、108 `21/21`，不是本 change 的测试通过证据。
