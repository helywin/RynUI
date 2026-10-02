# Space Separator 阶段验收

2026-10-03，Windows/MSVC，`windows-msvc-headless` Debug/Release 全部 CTest 66/66，实际耗时 103.93 / 16.66 秒。日志为工作区 `out/space-separator-{debug,release}.log`。

覆盖空/单项不执行、N-1 分隔、split alias、多个富内容根、透明 Theme 及 Space own Theme、实际交错布局/scene/Tab 顺序、RTL 不 remount/reshape、wrap retained、零约束、删除中间与首项清理分隔、slot 抛错完整回滚、销毁订阅与 scene/interaction/node 归零。

ComponentBuildContext 的 before-child hook 仅存在于该次 slot mount；透明 slot 共享，普通子树不继承，递归分隔挂载受 guard 保护。没有新增 NodeStore 重排或 renderer 上传路径。该记录属于平台通用合同，Windows 真窗口验收另列。
