# Windows 原生 Card 验收

2026-10-23，实际 Windows、MSVC x64/Ninja Multi-Config，`windows-msvc` Debug/Release；实际 `direct3d12` / `DXIL` 窗口。系统 display scale=1.5，正常字重400使用 Segoe UI Variable Text，600使用 Segoe UI Semibold；来源文件、face index、weight在每次日志记录。

最终20次运行全部 exit=0：Debug/Release × Light/Dark × system/1/1.25/1.5/2。每次18张真实 GPU readback，合计360张PNG。两个JSON清单记录截图/UTF-8 LF规范化日志/EXE SHA256；最终核对与本地两个EXE匹配。完整校验命令如下：

```powershell
python openspec/changes/059-20261023-add-native-card/evidence/run_native_matrix.py Debug
python openspec/changes/059-20261023-add-native-card/evidence/run_native_matrix.py Release
python openspec/changes/059-20261023-add-native-card/evidence/verify_native_evidence.py --check-executables
```

每次实际验证7个Card：标题/extra/Input/Select/actions、无标题正文、小号、borderless/hover、静态loading、富标题/cover/actions、局部Component Token。验证子Button的pointer/keyboard激活、自动composition/text提交、loading清理焦点/IME/嵌套弹层、编辑值恢复、width=160的有限宽度、真实窗口resize、3次idle polls、identity/content runs与dispose清理。

原生截图发现并修正了三个展示问题：actions内容默认居中；浅色局部示例同步覆盖深色文字，避免继承Dark文字造成对比度不足；普通title沿用Ant Design单行省略并保留原始完整文本，typed富标题仍由应用排版。最终矩阵全部在这些修正后重新执行。视觉抽查了加载、暗色局部主题、窄宽度省略与居中actions；所有PNG另经解码、非空像素范围和SHA256校验。

最终相关验证：

```text
windows-msvc Debug/Release complete builds: passed
native affected Card/Select/Input/SDL/platform CTest: 14/14 each (3.02s / 1.88s)
final Gallery Recording contracts: 8/8 each (78.79s / 15.88s)
affected windows-msvc-headless Card CTest after presentation fixes: 3/3 each (0.53s / 0.26s)
native matrix: 20/20 runs, 360 PNGs, both EXE hashes passed
clang-format 22.1.3: 476 owned files, 0 failures
doctor healthy; strict 59/59; git diff --check passed
```

完整平台通用106/106两配置在容器阶段完成，见`common-container.md`；本阶段展示修正补跑上述受影响合同。这里的输入为向真实窗口关联组件服务注入的逻辑事件，不是人工系统IME或完整OS事件来源验收。

Linux native、Vulkan/SPIR-V、对应系统字体/输入仍无实际证据，5.2保持未勾选。已完成common与Windows不回退，继续后续Tabs等常用组件，不自动archive。
