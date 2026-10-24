# 实施阶段记录

## 规划与 API/model

基线 `62c990a1b494532c358ac487b2bb5931c08a7231`。OpenSpec 1.14.0 context/list 确认本工作树根及 081 未使用。离线 Ant Design 源 HEAD 为 `4a39f54842eade4e565ab336ef6097cd7e723cdd`，读取 descriptions 文档、style、useRow、Row 和 Cell；Context7 文档用于交叉核对，锁定源决定实现规则。

`windows-msvc-headless` / Ninja Multi-Config / MSVC 19.51，最多 4 jobs。API/model 的 Debug 定向 CTest 2/2 通过，clang-format 22.1.3 check 609 owned files 零失败；doctor healthy、strict 81/81、diff check 通过。

## Retained host 与 Theme（合并新 main 前）

实际执行：

```powershell
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 4 --target rynui_descriptions_component_tests rynui_descriptions_theme_tests rynui_descriptions_public_api_tests rynui_descriptions_model_tests rynui_portable_theme_algorithm
ctest --preset windows-msvc-headless-debug -R '^(rynui.descriptions_(component|theme|model|public_api)|rynui.portable.theme_algorithm)$'
& ./out/build/windows-msvc-headless/tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' openspec/changes/081-20261025-add-native-descriptions/evidence/verify_preservation.py
```

结果：Debug 5/5。覆盖实际横/纵布局、title/extra、span/隐藏重排、Input 草稿与 Button 事件、身份和资源清理；Theme 默认/继承/独立算法、inactive seed、colors/metrics dirty、最终字体验证、后代 preflight 原子拒绝及恢复。五组 fresh goldens：default/dark/compact 各 2806、dark-compact/compact-dark 各 2807，共 14032 个旧 typed leaf values 与 `62c990a` 相同（仅新增 Descriptions 和 identity 改变）；28 个旧 IIFE 文本及顺序完全相同。format check 614 owned files 零失败。

初次测试发现 slot 容器使用外层 build context，隐藏分支未包含实际子控件；已改为读取当前 slot 的 build context，专项测试确认隐藏输入焦点/会话/引用均失效。没有将失败测试记为通过。

后续按用户追加指令，在干净本地阶段提交后显式 merge `c083fed5a72f644b491118c6440908e0354d241e`，再做共享 Theme/配置和最终专项验收，不 rebase。独立 Astra 复核由父代理执行；此文件不是独立 review 结论。无 GPU/Linux/系统 IME/AT/Gallery 验收声明。
