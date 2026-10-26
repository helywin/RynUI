# Astra R2 动态增长焦点顺序 P2

原生产基线 `0df5c99be1ede796d8941a45d969a6fad29d59df`，分支 `t3/add-native-rate`，隔离树 `C:/Users/jiang/.t3/worktrees/RynUI/t3-c74e4a3d`。父转交的 R2 有一个新的 P2；R1 四项与 font/clip 没有新增 P1/P2。本记录为本 worker 的实际复现、修复与定向证据，不代表 fresh Astra R3 已通过。

## Before

先只新增专项测试 harness/CMake，保持生产实现为原干净 0df5c99b。在本树新编译/链接：

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_rate_focus_order_tests
out/build/windows-msvc-headless/tests/Debug/rynui_rate_focus_order_tests.exe --before
```

最小场景为 count=0 的 Rate（第一项有原生富 Tooltip Input），后接外部 Button。count 改为 1，真实 FocusManager Tab 到 Rate 确实打开 popup，并先确认 Input branch active；下一 Tab 跳过 Input，实际到达外部 Button：

```text
BEFORE count0->1: Tab skipped Input to external Button=1; declaration Root=0 Button=1 Input=5
```

与父推演一致，没有静默略过或用初始 count=1 场景替代动态创建。

## After：最小生产补正

生产代码仅在 `RateComponentHost::ensure_items` 追加完成后增加 22 行：按完整 ComponentId 重新查询 owner，从完整 registry `declaration_order()` 沿组件祖先归属收集每个 retained 星项的全部后代，按 `items` 逻辑序拼接，通过既有 `InteractionRegistry::reorder_after(root, owned)` 放到 Rate 根之后。包含未激活项、关闭 popup 和 Tooltip 窗口层逻辑后代，不使用 active/paint 列表。

没有重建 Component、editor 或 ref；没有改 FocusManager/InteractionRegistry 通用机制。foreign interaction IDs 的相对序列保持完全一致，祖先与其他 Rate 不纳入本 owner 的移动集合。根单一 Tab 与隐藏 eligibility 仍由既有状态控制。

## 专项覆盖与实测

`rate_focus_order` 使用真实 Tab 和声明序断言，覆盖：

- count 0→1：Rate→新 Input→原外部 Button。
- 已有项 1→3：通过 End 选择新末项后，Tab 进入新 Input，再进入中间 Button。
- 3→1 后增长到 4：追加期间旧项 1/2 已隐藏；仍要求全部 retained 后代连续且按逻辑序排列，不能漏掉未来恢复的后代。
- 4→1→3：隐藏项不进入 Tab；恢复的 Input 保留 ComponentId/editor/ref；原 Input 的 Saved A 草稿保持。
- 外部 Before/Between/After 三个 Button、显式祖先 interaction、两个 Rate owner 分别增长：每次比较完整 foreign ID 序列不变，并检验两个 owner 的完整后代连续性。第二 owner 的新末项也可由真实 Tab 进入。
- 所有原断言保留，最后销毁无 owned input blocks 泄漏。

官方 `windows-msvc-headless`、MSVC 19.51、Ninja Multi-Config、最多 2 jobs；只使用隔离树中新编译/链接的测试产物。修复后每配置仅执行一次下列定向 CTest，未跑无关 full matrix 或金样生成：

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_rate_focus_order_tests rynui_rate_review_tests rynui_rate_lifecycle_tests rynui_rate_font_limit_tests
ctest --preset windows-msvc-headless-debug -R '^rynui\.(rate_focus_order$|rate_r1_|rate_lifecycle$|rate_font_limit$)' --output-on-failure
cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_rate_focus_order_tests rynui_rate_review_tests rynui_rate_lifecycle_tests rynui_rate_font_limit_tests
ctest --preset windows-msvc-headless-release -R '^rynui\.(rate_focus_order$|rate_r1_|rate_lifecycle$|rate_font_limit$)' --output-on-failure
```

Debug 7/7，2.32 秒；Release 7/7，1.45 秒。包括 R1 四场景、100ms 可取消 deadline/idle、边界清理、实际 outline coverage/keyboard emphasis、完整 generation 重绑、字号 float/round 原子性及直接 lifecycle 代表。

## 保持的不变量

- 对 0df5c99b 的 diff 确认 `rate_theme.cpp`、`theme_snapshot.cpp`、`theme_runtime_types.hpp`、共享 `window_component_services.cpp` 及五份金样未变。
- 既有 `verify-theme-compat.py` 确认 36 个旧 IIFE 原文/顺序、196 个旧 identities、16587 个旧 typed 叶字段不变；未重新生成金样。
- 读取本次四个目标的 Debug/Release 共 8 个实际 PE32+ 头，`SizeOfStackReserve` 全为 1048576 字节，未扩大默认栈。
- clang-format 22.1.3 检查 740 个自有源码文件、0 failures；父给定 explicit OpenSpec 1.14 doctor 返回本隔离树 `healthy=true`，strict 88/88、0 failed；working/index diff check 均通过。

收口命令（PowerShell，本隔离树）：

```powershell
$python = 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe'
$node = 'C:/Program Files/nodejs/node.exe'
$openspec = 'C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js'
& $python scripts/format-code.py --check --clang-format 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe'
& $node $openspec doctor --json
& $node $openspec validate --all --strict --no-interactive
git diff --check
git diff --cached --check
git diff --exit-code 0df5c99be1ede796d8941a45d969a6fad29d59df -- src/theme/rate_theme.cpp src/theme/theme_snapshot.cpp src/theme/theme_runtime_types.hpp src/component/window_component_services.cpp design-tokens/ant-design/6.6.5/golden
& $python openspec/changes/090-20261026-add-native-rate/evidence/verify-theme-compat.py
@'
from pathlib import Path
import struct
count = 0
for config in ['Debug', 'Release']:
    for name in ['rate_focus_order', 'rate_review', 'rate_lifecycle', 'rate_font_limit']:
        path = Path('out/build/windows-msvc-headless/tests') / config / f'rynui_{name}_tests.exe'
        data = path.read_bytes()
        assert data[:2] == b'MZ', path
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        assert data[pe:pe + 4] == b'PE\0\0', path
        assert struct.unpack_from('<H', data, pe + 24)[0] == 0x20b, path
        assert struct.unpack_from('<Q', data, pe + 24 + 72)[0] == 1048576, path
        count += 1
print(f'{count} focused Debug/Release PE images retain 1048576-byte stack reserve')
'@ | & $python -
```

不写 ROOT，不运行 ROOT/out 测试程序，不导入其他 WIP，不访问 remotes/push/PR/archive/children。

完成本地英文 commit、clean HEAD 后停待父 fresh Astra R3；本机 HEADLESS 结果不扩展为真实窗口/GPU/Linux/系统 IME/AT 验收。
