# 075 平台通用验证

## 环境与范围

- 日期：2026-10-24 创建规划，2026-10-25 恢复并完成首版验证。
- 工作树：`C:/Users/jiang/.t3/worktrees/RynUI/t3-e37b407e`，分支 `t3/native-form-components`；原始基线 `0de353b07f7ed6313172aa38e9776a8f931158af`。
- 官方 configure preset `windows-msvc-headless`，Ninja Multi-Config，MSVC x64 19.51.36256，Debug/Release。构建目录为该工作树自己的 `out/build/windows-msvc-headless`；每次 `--parallel 4`。
- 只复用 `D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_{freetype,harfbuzz,utf8proc,sheenbidi}-src` 依赖源码；库、objects、生成文件、字体和测试程序均由当前构建树独立生成，未引用其他构建树二进制。
- clang-format 22.1.3；Python 3.10.11。无原生 GPU、Linux、系统字体、系统 IME 或人工视觉验收声明；不运行全套 CTest。

## 规划和模型阶段

`776a22e` 保存完整 proposal/spec/design/tasks；doctor healthy、strict all 74/74、diff check 通过。`06baf765` 模型/API 阶段使用相同官方 preset，Debug/Release 各 2/2（form_model、form_public_api）通过；该提交不代表 host 已验收。

锁定的 Ant Design 6.6.5 commit 为 `4a39f54842eade4e565ab336ef6097cd7e723cdd`。实际取得 `components/form/style/index.ts`，SHA256 `38453f00c6aa3408ef3e623666c8bb6a9c57552c4039959419168f6f4d03f39d` 与仓库 sources.lock 一致。Context7 提供行为参考，锁定源及本仓库现有原生合同决定实现。

## 定向构建与测试

本机通过忽略的 `out/form-build.ps1` 进入 VS x64 Developer Environment，执行下列真实 preset 命令；portable 旧用例使用已有的 `rynui_portable_*` targets。最初使用未注册的普通 theme_algorithm target 被 Ninja 拒绝，改用现有 portable target 后构建成功，未修改平台注册范围。

```text
cmake --preset windows-msvc-headless
  -DPython3_EXECUTABLE=C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_FREETYPE=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_freetype-src
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_HARFBUZZ=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_harfbuzz-src
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_UTF8PROC=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_utf8proc-src
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_SHEENBIDI=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_sheenbidi-src
cmake --build --preset windows-msvc-headless-debug --parallel 4 --target
  rynui_form_model_tests rynui_form_public_api_tests rynui_form_component_tests rynui_form_theme_tests
  rynui_portable_theme_algorithm rynui_portable_theme_runtime rynui_portable_selection_component
  rynui_portable_modal_component rynui_portable_focus_lifecycle rynui_portable_text_area
  rynui_portable_input_component rynui_input_number_component_tests rynui_feedback_component_tests
ctest --preset windows-msvc-headless-debug --output-on-failure -R "^rynui.(form_.*|input_number_component|feedback_component|portable.(theme_algorithm|theme_runtime|selection_component|modal_component|focus_lifecycle|text_area|input_component))$"
```

Release 将上述 build/test preset 的 `debug` 换为 `release`，targets 和过滤表达式一致。Debug 13/13、Release 13/13 通过。既有源码有未使用参数/变量、nodiscard 等 warning，本次自有源格式检查通过；未将 warning 当成原生验收证据。

从 `docs/form.md` 的 cpp fenced block 提取生成 `out/form-doc-example.cpp`，在相同 MSVC Developer Environment 执行 `cl /nologo /std:c++20 /permissive- /Zc:__cplusplus /utf-8 /EHsc /Zs /Iinclude out/form-doc-example.cpp`，退出码 0。`scripts/format-code.py --check` 使用 clang-format 22.1.3 检查 587 个自有源，0 failures；doctor healthy、strict all 74/74、`git diff --check` 通过。

## 覆盖的真实行为

- Input、Select、Checkbox、InputNumber 原生事件 → typed 字段 → required/custom → 失败焦点/成功结果 → 原生 Button reset；保留组件/editor 身份与 slot 执行次数。
- String 与 optional<double> 外部 controlled authority、拒绝回写、reset 初值请求；外部变化清错但不再次发用户 callback。
- `-` 数字草稿、相同空数值 reset、preedit 验证保留与 reset 取消、TextArea Enter 换行及 IME Enter 不触发表单提交。
- Modal 关闭/打开的 branch 与背景边界、Spin 输入阻止/会话清理、祖先 hidden、控件区域 clip 后的失败焦点资格。
- 移除字段结果/错误清理、原 handle 重挂保值、旧 onChange 代际、InputRef/CheckboxRef 重绑后旧 FormItem 不操作新控件。
- validator、错误观察器、focus、原生 Button finish 回调同步销毁；递归命令被拒绝，held FormRef/InputRef/CheckboxRef 清理。
- 实际节点的 bounds/layout_size/out_of_flow、标签/反馈显示后的 sibling 重排、实际 ordered glyph scene 的 ancestor/document clip、实时 Theme 颜色到 glyph material、metrics 到 layout 的局部更新。
- Theme Default/Dark/Compact、继承与独立 component algorithm、override、equality/hash/serialization、两类 subscription/dirty domain 与非法配置原子拒绝。

## 五份金样和解析栈

运行当前树的 `tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens` 更新五份 JSON。`python out/verify-form-goldens.py 06baf7658175fb1d0f80073a3ee0f2ddc380896c` 将 Git 基线 JSON 与当前 JSON 解析为值，去掉 identity 与新增 form 后逐项相等；5/5 通过。脚本同时逐块比较既有引用捕获 IIFE 的完整源文本，24/24 保留，新 Form 使用独立 `[&]` IIFE，未改变 Theme bridge/commit_snapshot 的既有栈复制修复。

## 验收边界

首版边界见 `docs/form.md`。本 change 不更新 Gallery、support overlay、global README 或完成度计数；不创建 PR、archive、子线程或远端 T3 分支，不 push。父线程负责后续 Astra 审核、合并和推送。后续父基线合并结果与最终检查将在独立收口段记录。
