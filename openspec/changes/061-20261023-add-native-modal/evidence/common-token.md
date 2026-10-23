# Modal Token 与task2收口

2026-10-23，实际 Windows/MSVC x64，`windows-msvc-headless` Debug/Release。结合此前API/model增量，完成task2；没有据此实现或验收真实对话框。

新增10颜色、11布局尺寸、正文/标题字体、focus/阴影和z-index Token。支持继承/reset、component seed/algorithm/覆盖，颜色/尺寸/字体/效果/层级五个失效域、identity和诊断JSON。默认配方对照固定6.6.5 Modal：elevated背景、透明header/footer、mask黑色0.45、padding20/24、gap8/12、标题16/24和600、close32、圆角8与boxShadow。既有全局alias没有mask字段，本次不扩大alias范围，通过Modal Token覆盖。

测试包含默认值、Dark/Compact、parent继承/reset、独立算法及关闭算法、组件局部性、五域通知和unchanged update、非法零/负数/非有限/溢出/字重/层级原子拒绝、零可选效果、序列化/hash/名称。颜色只通知材料，层级只通知材料/命中，不测量。实际对话框测量、focus/shield/scroll在后续task验收。

```text
windows-msvc-headless Debug/Release complete builds: passed
affected API/model/theme/public API/boundary CTest: 23/23 each (1.54s / 1.53s)
five goldens regenerated; every previous Token value equal to d1365a5 (excluding identity and added modal group)
clang-format 22.1.3: 492 owned source files, 0 failures
openspec doctor: healthy; strict: 61/61; git diff --check: passed
```

旧值核对：`python openspec/changes/061-20261023-add-native-modal/evidence/verify_existing_tokens.py`。API/model与Token均已验证，输入边界、实际Modal、Gallery、Windows/Linux原生窗口任务仍未完成。
