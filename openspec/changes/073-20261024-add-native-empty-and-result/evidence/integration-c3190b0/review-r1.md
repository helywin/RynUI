# Astra R1 聚焦修复

基线为原始 073 `a6017b1bcb10f38a551491302fc639652071afde`，本地合入 main `c3190b00210112a3a1ccf3a350cf0290082a4b14` 后的源代码为 `c3b84839029d1d292dc2c549c6c72efb1c2cf33f`。Astra 对原始 073 提出两项 P2；先在此集成基线增加回归并实际运行，再修改实现。未合入后续 Gallery 或 popup wheel 变更。

## 修复前证据

`before-fix.log`：同一 `480×900` host 下，Empty image 与 Result icon 都使用 `width=100`、`height=80`、左右 margin 各 50、上下 margin 10/20。区域测得 `200×110`，子节点实际被放成 `0×50`，两项断言失败。

初始及响应式 `0/0.01/0.49 dp` 图示尺寸均失败。本 headless fixture 的真实 font resolver 在请求 0 逻辑像素时先报 `Input validation fonts failed to load`，因此没有走到 Astra 推导的下游 `Icon pixel size must be positive`。响应式日志实际显示回调抛出前 ThemeSnapshot 已发布新尺寸，证实发布后失败。没有通过替换 resolver 或放大线程栈绕过失败。

此时完整 Debug CTest 已在 `c3b8483` 通过 **148/148，280.22 秒**，保存为 `pre-review-full-debug.log`；它不包含新增回归通过的结论，也不替代修复后的完整验证。

## 修复与回归

- 区域按子节点 `layout_size` 外框居中和放置，让 layout engine 只扣除一次 margin。回归同时检查两种 slot 的实际节点 `100×80`、外框 `200×110`、中心位置和实际提交的可见 glyph。
- 内置图示尺寸零时休眠原有 image 分支，不占空间、不提交 glyph draw command。保留至少 1 逻辑像素的有效字体；正的极小尺寸使用请求的宽高并裁剪实际 glyph，不把布局放大到 1 dp。自定义 slot 保留自己的尺寸和身份。
- Empty/Result 初始与响应式 `0/0.01/0.49 dp`、Compact Empty 初始/响应式零均覆盖；检查合法 Theme 发布、非法负尺寸更新不改 snapshot、零状态下 Result 状态变化、2× DPI 刷新、恢复 72 dp 后实际字体 DPI 与可见 glyph、Component/region 身份保持。
- glyph 检查遍历 composer 本帧提交的 draw commands，保留 store 中的休眠实例不作为当前可见内容计数。

## 本阶段验证

Windows MSVC 19.51.36256.0；官方 `windows-msvc-headless` preset，`Ninja Multi-Config`，`HEADLESS/RECORDING`，独立 `out/build/empty-result-headless`，`--parallel 4`。

```text
cmake --build out/build/empty-result-headless --config Debug --parallel 4 --target rynui_empty_result_component_tests
ctest --test-dir out/build/empty-result-headless -C Debug -R '^rynui\.empty_result_' --output-on-failure -V -j 4
cmake --build out/build/empty-result-headless --config Release --parallel 4 --target rynui_empty_result_component_tests
ctest --test-dir out/build/empty-result-headless -C Release -R '^rynui\.empty_result_' --output-on-failure -V -j 4
```

Debug **3/3，4.50 秒**；Release **3/3，3.17 秒**，完整专项日志为 `review-debug.log` / `review-release.log`。clang-format 22.1.3 检查 583 个自有源文件、doctor、全部 strict validate、diff check 通过。4.1 在完整修复后 Debug/Release 及最终 evidence 收口前保持未勾选。

改动限于 Empty/Result 实现、其组件回归、独立组件说明与本证据。无公开 API、Theme 验证规则、wheel routing、host order、Gallery 或全局文档改动。真实 GPU、Linux、系统 IME 与上游插画像素等价没有在本阶段验证。
