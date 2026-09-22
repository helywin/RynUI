# Ant Design 6.5.0 → 6.6.5 官方来源审查

本账本由 2026-09-22 的两个官方 tag 离线 checkout 生成。机器可核对的逐文件 SHA256、Token identity 和目录候选见 [source-diff.json](source-diff.json)；`python -B tools/audit_ant_design_665.py --old-source <6.5.0 checkout> --new-source <6.6.5 checkout> --check --self-test` 可复算。运行时与构建均不得读取浮动 `latest`。

| 项目 | 6.5.0 | 6.6.5 |
| --- | --- | --- |
| tag / commit | `6.5.0` / `740ad964dc2397f33e40944367b0536a7314cc32` | `6.6.5` / `4a39f54842eade4e565ab336ef6097cd7e723cdd` |
| license | MIT `LICENSE` | MIT `LICENSE` |
| Gallery 原有组件 | 72 | 72 项仍有官方文档；新增 Listy 候选 1 项 |
| Token identity | 1194 | 1198 |
| 审计来源文件 | - | 249 路径：新增 2、改变 150、等价 97、移除 0 |

目录七类的原有 72 项未发现分类改动。`components/listy/index.en-US.md` 是新增来源，属于 `Data Display`，因此新版目录预计 73 项、Data Display 预计 21 项；仍须在阶段 2 由新版 manifest/schema/generator 验证，不能凭此把 Gallery 标为已升级。List 的英文文档注明已 deprecated、推荐 Listy；阶段 2 应保留 List 项但更新 support 状态，不应静默删除。

Token 新增 `ant.seed.focusOutline`（`components/theme/interface/seeds.ts`，上游 `@default true` 和 `components/theme/themes/seed.ts` 实值）、`ant.component.Alert.borderRadius`（默认取 `borderRadiusLG`）以及 `ant.component.Listy.itemPaddingBlock` / `itemPaddingInline`（默认分别取 `paddingSM` / `padding`）。其余 1194 项在当前 importer 的 identity、type、seed default、support 分类上等价；来源行号变化不视作视觉值变化。`focusOutline` 尚未进入旧 importer 的 seed-default 表，阶段 2 必须补上，不能把新 catalog 的默认值留空。

Button 的公开 Props 在核对的 `Button.tsx` 中未见新增或删除；延迟 loading 从自管 timer 改为 `useDelayState`，图标垂直对齐与 loading icon 入场 opacity 改变，variant border style 改为消费 `lineType`。桌面端不照搬 React hook，但须检查延迟 loading、图标对齐、边框线型及相关视觉测试。Input 的 `GroupProps` 导出被标记 deprecated，`lineWidthFocus` 影响 borderless focus outline，`initComponentToken` 对其作 0/非 0 映射；现有 `ryn` Input API 不需机械重命名，但 focus 视觉/Token 映射须审查。Checkbox/Switch 的 no-motion 实现变更，Switch 内文案改用 flex 居中；这些为 011 后续规划的来源，不代表本 change 已实现组件。

本阶段源审计使用 Windows 10.0.26200、Python 3.10.11。`windows-msvc` preset 声明 Ninja Multi-Config / MSVC，机器已安装 MSVC x64 19.51.36256；当前普通 PowerShell 的 PATH 未包含 `cl.exe`，直接 `cmake --preset windows-msvc` 配置失败（C++ compiler `cl` 未找到）。这不是 Windows 构建验收通过；阶段 3/7 必须在正确的 VS 开发环境重新配置、构建和测试。源码审计自身不依赖编译器。
