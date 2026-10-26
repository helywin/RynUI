# 094 原生 Tree 验收记录

## 源码与环境

- 隔离路径：C:/Users/jiang/.t3/worktrees/RynUI/t3-9bdf1a7f；分支 t3/9bdf1a7f。
- 基线：34eefb82c57f3d3bdf687db477aef9de9247b4b3。最终功能源码：a5d0b041e62755316ea13edf74744adf43967c78。
- 本地阶段提交：8d016487 规划；7f2dc613 模型/Theme；a0deab46 原生组件；a5d0b041 响应式重排后的焦点显露。
- Ant Design 6.6.5 锁定参考：D:/code/RynUI/out/upstream-ant-design-6.6.5，HEAD 4a39f54842eade4e565ab336ef6097cd7e723cdd。查阅 components/tree/index.en-US.md、Tree.tsx、style/index.ts、basic/basic-controlled/customized-icon demo 及 demo.test.ts.snap。
- 正式 preset：windows-msvc-headless；Ninja Multi-Config，MSVC 19.51.36256.0，Debug/Release，构建始终 --parallel 2。工具详见 versions.log。
- configure、缓存、编译、测试及程序均位于自身 worktree。BUNDLED 依赖只读复用 ROOT/out 的锁定 FreeType/HarfBuzz/SheenBidi/utf8proc source；四份锁定字体复制至自身缓存并由 CMake 校验。

## 实际结果

| 验证 | Debug | Release |
| --- | --- | --- |
| Tree 组件 11 项 + 模型/Theme 2 项 | 13/13 | 13/13 |
| 直接共享回归 | 23/23 | 23/23 |
| 定向 CTest 合计 | 36/36，21.88 秒 | 36/36，9.91 秒 |
| 三个 Tree 测试程序 x64 PE32+ stack reserve | 均 1048576 字节 | 均 1048576 字节 |

共享回归包含 Avatar 的 9 项，以及 Timeline integration、Collapse、Steps、Menu、Input、Button、Select、Theme algorithm/runtime、Focus order/state/lifecycle、InteractionRegistry、LayoutStyle。共 36 个独立 CTest，在两个配置实际执行；不是完整 CTest matrix。

- basic/controlled：真实 pointer、Home/End/方向键、Enter/Space，展开、单选、级联/半选及受控候选不提前生效。
- rich/boundaries：Input/Button/Select 真正组合；输入草稿、ref、组件/editor 身份；disabled 行保留有效富 child，刷新不夺走其焦点；checkbox 动态显隐、折叠恢复及邻居 Tab 顺序；无焦点 controlled popup 关闭后不因旧 open 恢复。
- scroll/clips/theme_scene：有限 viewport、行间隙滚轮、焦点显露及前缀节点恢复后重排；真实 R8 atlas ink、glyph clip、marked fill quad、rounded effect coverage、hit clip、向外焦点装饰；颜色更新不触发重测。
- mount_lifecycle/lifecycle/reactivity：首次富 slot、初始 Prop/LayoutStyle getter、挂载中 Signal 更新、Theme/焦点回调和最终 mount 输入边界销毁 owner 或私有区域；同步 blur 替换新代际不伤新 ref；capture/session/preedit、旧 session 拒绝和 owned/foreign blocks 清理。
- 模型接受 1024 节点、64 层；组件挂载拒绝重复/空 key、受控/default 冲突、1025 节点或 65 层。未声称对 1024 个实际富控件做性能验收。
- Tree Theme 的最终字体 round/INT_MAX/64 边界、组件 algorithm、继承、无效根更新及仅后代组合超限的原子失败/恢复均通过。
- Release 实际重新生成五份 golden；37 个旧 IIFE 原文/顺序、198 个旧 TokenIdentity ordinal、五份 golden 共 240 个旧顶层字段深度相等。仅尾追加 Tree 片段与两个 identity；原身份 hash 随完整快照改变。
- docs/tree.md 中的 C++ 示例与 tree_basic 编译执行的函数相同；verify-artifacts.py 同时验证该对应关系、缓存、六个 PE 及 fresh golden 哈希。
- clang-format 22.1.3 检查 742 个自有源码，0 failures；OpenSpec 1.14.0 doctor/strict validate 和 git diff --check 的最终结果见 logs。

## 日志与复现

run-headless.ps1 保存实际工具路径、只读 source 缓存和目标集合，且检查当前目录必须是绑定的 Tree worktree。它将输出写入自身 out/tree-logs。logs/ 保留实际 configure/build/test stdout/stderr 文本，统一转为 UTF-8；无内容删改。Release initial 日志保存首次完整编译，随后 build-Release.log 保存最终修正的增量编译。已有 FreeType code-page 与旧组件编译告警保留在日志中。

日志含工具原生的行尾空格，本目录 .gitattributes 仅对 logs/*.log 保留换行和原始空格；自有源码、文档和其他文件继续执行正常 diff --check。

五份原始 fresh JSON 在 goldens/；对应已更新的正式 golden 只增加 Tree。可运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File openspec/changes/094-20261026-add-native-tree/evidence/run-headless.ps1 -Action build -Configuration Debug
powershell -NoProfile -ExecutionPolicy Bypass -File openspec/changes/094-20261026-add-native-tree/evidence/run-headless.ps1 -Action test -Configuration Debug
# Release 同样使用 -Configuration Release
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' tests/verify_tree_theme_baseline.py out/tree-goldens
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' openspec/changes/094-20261026-add-native-tree/evidence/verify-artifacts.py
```

## 边界与交接

本次仅证明 Windows MSVC HEADLESS 的 logical scene 和原生输入运行时合同。未进行原生窗口/GPU、Linux、系统 IME 或辅助技术验收。首版排除的 API 见 docs/tree.md；没有多选、strict checking、动态拓扑、异步加载、拖放、虚拟化、搜索、RTL、动画、DirectoryTree 或 Web 占位实现。

全部提交为本地英文 Conventional Commits；没有远端操作，也未修改 ROOT、Gallery、README、全局进度、support overlay 或 generated catalog。当前代码包含基线 Avatar，未导入未审 Rate/Statistic/AutoComplete 工作。最终证据提交后保持 clean，交给父线程 fresh Astra 审查与 main 集成；没有创建 children、PR 或 archive。
