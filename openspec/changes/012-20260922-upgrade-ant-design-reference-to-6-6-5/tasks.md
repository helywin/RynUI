# Tasks

## 1. 官方来源与差异账本

- [x] 1.1 核实 Ant Design 6.6.5 正式 tag、完整 commit、MIT license 和所需文件 SHA256；保存离线来源清单，并以 tag/commit/hash/duplicate/missing-source contract 验证，不以浮动 `latest` 作为构建输入
- [x] 1.2 对照 6.5.0 与 6.6.5 的目录、分类、Token identity/default/derivation、Button/Input API 与状态视觉，记录新增/移除/改变/等价及来源；以差异账本覆盖率和人工 source review 验证，不预设旧 72/1194 计数仍成立
- [x] 1.3 在一个受支持平台记录实际 OS/compiler/preset，运行来源与差异 contract、`git diff --check`；以英文 `test: audit Ant Design 6.6.5 sources` 提交本阶段，不主动 push

## 2. 离线参考数据与生成器

- [x] 2.1 并列建立 `gallery/ant-design/6.6.5/` manifest 与 support overlay，按新来源重算分类、identity、source path、条目数和状态；以 schema/order/duplicate/source-version/hash 测试验证，保留旧目录不改写
- [x] 2.2 并列建立 `design-tokens/ant-design/6.6.5/` catalog、source lock 与 golden，按差异账本更新生成器和 checked-in metadata；以 schema、count、Token identity、source SHA、`python -B ... --check` 及无 cache contract 验证
- [x] 2.3 增加当前生效 Gallery/Token/生成物跨版本一致性 gate，混用 6.5.0/6.6.5 必须失败；在一个受支持正式 preset 运行生成器与相关 CTest、`git diff --check`，以英文 `feat: update Ant Design offline reference data` 提交，不主动 push

## 3. Theme 与既有组件迁移

- [x] 3.1 按已确认差异更新 Default/Dark/Compact Theme、typed Token 映射与必要的 Button/Input 视觉或交互实现；未变化项保留现有逻辑，以新旧 source identity、数值/状态矩阵、Theme/renderer 测试验证
- [x] 3.2 迁移 Button/Input source contract 与 public compatibility tests，明确任何上游 React Prop 差异是否影响 `ryn` API；以 pointer/keyboard/IME、DPI geometry、scene dirty/upload、idle 与 benchmark 回归验证无静默破坏
- [x] 3.3 在一个受支持正式 preset 运行本阶段受影响 CTest 与 `git diff --check`，记录 OS/compiler/preset/result；以英文 `feat: align controls with Ant Design 6.6.5` 提交，不主动 push

## 4. Gallery 与当前文档

- [x] 4.1 迁移 Gallery 生成 metadata、目录内容、Foundation/Token 与支持状态，按新来源重算 hash/count/reachability；以 Introduction、七类或新版实际分类、filter、anchor/wheel、72 项变更检测和无 fake sample 的 headless contract 验证
- [x] 4.2 更新 README、README.en、`docs/architecture.md` 和生成 Token 文档的当前基线说明，保留旧 change proposal/spec/tasks/evidence 的历史版本与验收状态；以当前文档/version gate、旧证据未改写检查和 `git diff --check` 验证
- [x] 4.3 在一个受支持正式 preset 运行 Gallery/Theme/Token/文档合同、`python -B` 生成器 `--check`；以英文 `feat: update Ant Design reference gallery` 提交，不主动 push

## 5. 后续组件规划对齐

- [x] 5.1 使用 OpenSpec update workflow 将在途 011 的 proposal/spec/design/tasks 对齐已核实的 6.6.5 Switch/Checkbox 来源，纠正尺寸和 indeterminate 视觉描述，不实现组件；以跨 change version/source 一致性、OpenSpec strict validate 与 `git diff --check` 验证，单独英文提交且不主动 push

## 6. 平台通用集成验收

- [x] 6.1 增加 6.6.5 passed evidence schema，要求完整 release/source identity、目录/Token counts/hash、差异账本、组件/Gallery 回归、preset/compiler/platform/driver/shader/font 与截图路径，并拒绝旧版或 planning-only 结果；以正反 fixture 测试验证
- [x] 6.2 在一个受支持正式 preset 运行全部相关 unit/headless/contract/benchmark、public dependency、lock/license、无网络 runtime、生成器和 Python cache 检查，记录实际平台与结果；不要求另一平台重复平台通用合同
- [x] 6.3 运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive`、`git diff --check`；以英文 `test: validate Ant Design 6.6.5 baseline` 提交平台通用 evidence，不主动 push

## 7. Windows 专属验收

- [x] 7.1 用 `windows-msvc` 正式 preset 完成受影响 Debug/Release build 与平台 CTest，核对 Ninja Multi-Config、MSVC x64、Win32/D3D12/DXIL 与系统字体，保存独立 Windows 构建证据
- [ ] 7.2 在 Windows 真实窗口按差异账本核对 Button/Input/Gallery 的受影响状态，以及系统 display scale 和 1.0/1.25/1.5/2.0 acceptance scale 的视觉与操作；保存截图、driver、font、scale、exit code 和 diagnostics，旧版截图不得替代新版
- [ ] 7.3 运行 Windows 6.6.5 passed evidence contract、受影响平台测试、shader/lock/license/cache、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Windows Ant Design 6.6.5` 提交，不修改 Linux 清单且不主动 push

## 8. Linux 专属验收

- [ ] 8.1 用 `linux-gcc` 与 `linux-clang` 正式 preset 完成受影响构建和平台 CTest，核对 Ninja Multi-Config、原生 Wayland、Vulkan/SPIR-V 与 Fontconfig 系统字体，保存独立 Linux 构建证据
- [ ] 8.2 在原生 Linux Wayland 真实窗口按差异账本核对 Button/Input/Gallery 的受影响状态，以至少两档实际 display scale 保存截图、window system、driver、font、scale、exit code 和 diagnostics，不以 XWayland 或 Windows 代替
- [ ] 8.3 运行 Linux 6.6.5 passed evidence contract、受影响平台测试、shader/lock/license/cache、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Linux Ant Design 6.6.5` 提交，不修改 Windows 清单且不主动 push

## 9. Change 收口

- [ ] 9.1 仅在准备 archive 时核对当前基线唯一、平台通用/Windows/Linux 独立证据与历史 6.5.0 记录边界，运行最终 OpenSpec doctor/strict validate、受影响 CTest、`git diff --check` 和 clean worktree 检查；本项不能替代缺失的真实平台验收，也不自动 archive 或 push
