# Typography 提示与多行编辑：Windows 原生验收

2026-10-23；实现基线 `07a44b0`。Windows 11 Pro for Workstations 10.0.26300，MSVC 19.51.36256 x64，Ninja Multi-Config，`windows-msvc`，BUNDLED。Debug/Release 完整构建通过。

## 实际运行

- 两种 configuration × Light/Dark × system/1/1.25/1.5/2 = **20次实际原生窗口运行**，全部 exit_code=0。系统 display scale 为1.5；其他档位为 acceptance render scale，并非改动系统显示缩放。
- renderer 报告 Direct3D12/DXIL；系统枚举 NVIDIA GeForce RTX 5080，驱动32.0.16.1656（同时存在远程显示适配器，renderer 未单独报告所选 adapter 名称）。
- 每次保存15张真实 GPU readback PNG，共 **300张**；两个 EXE hash、每张 PNG hash 和20份日志的 canonical UTF-8 LF hash 已核对，避免 Git 换行归一化造成伪漂移。清单见 `windows/windows-debug.json`、`windows/windows-release.json`。
- 自动操作涵盖文本 hover 提示、操作 hover/focus、复制成功、展开/收起、编辑入口提示、多行编辑/Shift+Enter/Enter、composition 与两次 Escape、受控回写、Link、实际窗口 resize 至1100×900、失活、3次 idle poll 和dispose。
- 特别保留其他组件的 hover 提示，验证其不会先于当前编辑器消费 Escape。重新进入编辑后检查 shell effect；GPU PNG 边框区域的蓝色像素数每次均大于20（实际2618–6612），直接捕捉隐藏缓存回归。
- `windows-msvc` Debug/Release 各运行10项受影响 native CTest，均 **10/10**：系统字体、历史 Typography evidence、平台 input/event pump/lifecycle、shader生成/部署和三类shader合同。

## 字体与图像审核

实际 face：Segoe UI Variable Text（400）、Segoe UI（600）、Segoe UI italic（400）、Cascadia Mono；weight500 无精确匹配时日志如实记录使用 regular face。CJK/Latin 图像均已输出。

查看了两 configuration、两主题、全部5档场景的编辑框区域汇总，并查看代表性完整图像：Light 1.25省略提示、Dark 2复制成功、Light 1.5重新进入编辑。正文/提示可读，双行内容与边框完整、无裁切；重新进入编辑后的边框已恢复。

本验收为实际窗口/GPU上自动注入的逻辑 pointer/keyboard/composition，不作为用户人工系统中文 IME、候选窗或 Linux 验收。Linux checkbox 保持未完成。

## 重现与核验

先按仓库正式 preset 构建对应 configuration，然后在仓库根目录执行（需要 Python/Pillow）：

```text
python openspec/changes/057-20261023-complete-typography-hints-and-editor/evidence/run_native_matrix.py Debug
python openspec/changes/057-20261023-complete-typography-hints-and-editor/evidence/run_native_matrix.py Release
python openspec/changes/057-20261023-complete-typography-hints-and-editor/evidence/verify_native_evidence.py --check-executables
```

`verify_native_evidence.py` 不带 `--check-executables` 时只核对已保存产物，可在没有本机 build tree 的 checkout 使用。程序没有记录或打包 EXE；hash 对应本轮实际运行的二进制。
