# Astra R1 六项 P2 修复验收

基线：`f3a900ec9c4ef21ed4e2a3268ea033221beacd99`。仅在原 worktree、原 `t3/native-popover` 分支修复，未合入 main、Message 或086。

## 失败复现与修复后结果

先只新增 `popover_review_tests.cpp` 与 CMake 注册，未修改生产代码，MSVC headless Debug 新构建执行6项测试，实际 **0/6通过**；原始输出见 [r1-before.txt](r1-before.txt)。最终同一6项测试及其扩展场景在 Debug/Release 均 **6/6通过**。

1. `popover_review_fonts`：修复前失败 `R1.1: unrenderable final Popover font was published`。修复后最终字号 `.25` 的 token override、独立 seed，以及 long/uint32 上界均在发布前拒绝；检查 root/child identity、generation、通知保持。合法 `.5` 恢复并继承；祖先 Compact 更新导致独立后代字体非法时，三层 identity/generation/通知均原子，合法 seed 恢复后可发布；真实富 slot Icon/Text 的合法边界更新可用。
2. `popover_review_positioning`：修复前失败 `R1.2: moved picker footer left child Popover on previous anchor`。修复后 Date→Popover→Time、Time→Popover→Date、Popover→Date/Time→Popover 都在一次 sync 内完成初次打开、移动和视口变化定位。每次核对 Popover anchor 与最终实际节点坐标，内层 picker 横坐标与最终 selector 一致，不以额外 sync 掩盖旧位置。
3. `popover_review_escape`：修复前失败 `R1.3: Escape closed parent picker before its open child Popover`。修复后 Date/Time footer 内真实 Button Pointer 打开子层，Button 回调保留且焦点留入口；Escape 先关闭子 Popover、父仍 open。受控拒绝维持子层与父层，原请求去重不变；新的 authority 事务后可接受关闭。IME Escape 与更深 Select 优先均通过。
4. `popover_review_focus`：修复前失败 `R1.4: initial focus selected proxy wrapper instead of native entry`。修复后键盘打开父 Popover 首焦点为子层真实 Button，Enter/Space 各执行原生回调并打开子层；仅将代理 wrapper 标为不可聚焦，没有全局过滤 tab_stop 或改变合法 roving 控件。
5. `popover_review_delays`：修复前失败 `R1.5: reactive leave delay discarded pending close forever`。修复后 pending enter/leave 分别热更新为2000ms、50ms、0，保留方向并以更新时间重算，提前一微秒不执行，到期完成并清 timer。待入/待出的取消与销毁均通过。
6. `popover_review_immediate`：修复前失败 `R1.6: pending hover delayed immediate focus`。修复后10秒 hover 待入期间，Tab 和 ref focus 均立即打开，无遗留 deadline，失焦并移出后按100ms正常关闭。

## 生产变更范围

- `popover_theme.cpp`：只增加最终可舍入字号边界校验，未在 Text 端 clamp，未改 Theme 派生默认值。
- `popover_model.hpp/.cpp`：保留 pending 方向的 retime；immediate focus 提前已有 hover deadline。
- `popover_component.hpp/.cpp`：原生入口代理 wrapper 不可聚焦、热更新延时走 retime、子层 Escape 归属查询、逐实例定位入口与最近逻辑祖先定位。
- `date_picker_component.hpp/.cpp`、`time_picker_component.hpp/.cpp`：复用原定位算法，抽出逐实例入口，先提交最近 Popover/Date/Time 祖先；完整 ComponentId 重入栈防止重复进入，删除旧 Time→全局 Date 刷新。保留内容 Table 的必要同帧刷新，父 picker 对已打开子 Popover 的 trigger 让出 Escape。

未更换固定 host 注册顺序，未创建通用 overlay 框架，没有 renderer/ABI/新依赖变更。所有祖先调用后回取 live state；原覆层、子 Table、wheel、glyph/hit 和 focus 裁剪回归保留。

## 最终实际检查

Windows MSVC Ninja Multi-Config，官方 `windows-msvc-headless` Debug/Release 构建，最多4 jobs。去重影响集合各 **44/44**（Debug30.95秒，Release16.24秒）：原31项 + 本轮6项 + 受定位/键盘变更影响的 TimePicker component及6项 review。

```text
ctest --test-dir out/build/windows-msvc-headless -C Debug -R "^rynui\.(popover_.*|date_picker_component|date_picker_review_.*|time_picker_integration|time_picker_component|time_picker_review_.*|time_navigation_merge|portable\.(theme_algorithm|theme_runtime|theme_scope_component|default_theme|switch_features|tooltip_component|popconfirm_component|dropdown_component|backend_boundaries))$" --output-on-failure
ctest --test-dir out/build/windows-msvc-headless -C Release -R "^rynui\.(popover_.*|date_picker_component|date_picker_review_.*|time_picker_integration|time_picker_component|time_picker_review_.*|time_navigation_merge|portable\.(theme_algorithm|theme_runtime|theme_scope_component|default_theme|switch_features|tooltip_component|popconfirm_component|dropdown_component|backend_boundaries))$" --output-on-failure
```

新 Debug Theme 程序重新生成五份 goldens，与 f3 **逐字节无差异**；[compare-theme.py](compare-theme.py) 继续确认15,372个旧叶子、33个 IIFE 原文及顺序、旧 TokenIdentity 不变。ThemeSnapshot/runtime、Time 静态配置、Switch publish_theme helper、Binding bridge 没有改动。[check-stack.py](check-stack.py) 读取最终 Debug/Release 各7个 Popover测试程序，14个 PE32+ 的 stack reserve 均为1,048,576 bytes。

clang-format22.1.3 检查690个自有文件，0 failures；doctor健康、OpenSpec1.14 strict全部84/84、diff check通过。重开的任务全部验收后再勾选。

证据仅为 Windows headless 平台通用合同，不代表 native/GPU/Linux/system IME/AT 验收。没有远端操作/push、ROOT修改、children/PR/archive或新建 R2；父任务负责独立 R2 与集成。
