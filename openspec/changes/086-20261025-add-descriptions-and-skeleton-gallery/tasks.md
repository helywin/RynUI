# Tasks

## 1. 规划与基线（平台通用）

- [x] 1.1 核实 bfbb492 的 ID/旧样例函数、组件合同和锁定上游，生成规划；doctor、strict validate、diff check 通过后本地提交。

## 2. 详情与加载流程（平台通用，Windows MSVC 验证一次）

- [x] 2.1 追加 Descriptions/Skeleton 真实交互示例、共同生产 anchor、目标文档和两条 overlay/生成目录；实现 focused fixture，验证三主题双有限视口中的真实目录、按钮、键盘、Input/Select/Switch、草稿/preedit 清理恢复、refs/身份以及实际 glyph/surface/hit clip 和四边焦点 coverage。
- [x] 2.2 用 windows-msvc-debug、Ninja Multi-Config、最多 4 jobs 构建本树 frame/catalog target；新 focused flag、必要旧代表 flag、精确 inventory、八 contracts 与 generator checks 实际通过，基线脚本确认 334 旧 IDs/旧函数体和非目标 overlay 未变后本地提交。

## 3. 集成边界（平台通用）

- [x] 3.1 运行 clang-format 22 全仓 check、doctor、strict validate、diff check；记录本树实际证据、检查与限制，本地提交并确认 dirty clean，交父新一轮 Astra 审查。

完整约 1001 秒 frame/native smoke 由父统一执行；不在本任务重复。无新增 OS/GPU 功能，故不建立 Linux/native/system IME/AT 验收项，也不据 headless 结果宣称这些通过。

父最终已在 `fbce29a0` 完成冻结批次 9/9（完整 frame 1800.51 秒）及独立 Windows D3D12/DXIL 七帧启动，见 [最终记录](evidence/03-frozen-batch-final.md)。上方约 1001 秒为规划时旧基线，不是本轮耗时。

## 4. 父分支平台通用集成

- [x] 4.1 独立 Astra 复核通过后合入 main，使用 ROOT 正式 MSVC Debug 新构建 Gallery/frame/catalog，运行新 focused、inventory 和八项 contracts；保留既有源码与数量，记录本轮边界后提交并推送 main。统一完整 frame 与原生启动仍由父后续分别记录，不冒充本阶段已执行。
