# Tooltip 平台通用验收

2026-10-02，Windows 11 / MSVC，Ninja Multi-Config。

- windows-msvc-headless-debug：完整 CTest 38/38，113.22 s。
- windows-msvc-headless-release：完整 CTest 38/38，12.21 s。
- Tooltip focused contracts：十二种方位、主轴翻转/移位、非有限负例、受控/非受控冲突、真实鼠标 hover 延迟/离开取消、keyboard focus/Escape/原子按钮激活、disabled child、受控未回写请求撤销、窗口失活、回调内销毁、Theme 纯颜色不重新测量/挂载、arrow topology、maxWidth 与 1 logical pixel 窗口。
- 原有 Input retained selection/preedit benchmark 各 20,000 次更新通过零分配合同。最初发现 MSVC Debug 空 vector 拷贝产生一次分配/同步，Tooltip 无实例时快速返回后恢复；不放宽 benchmark。
- Theme diagnostic JSON 新增 Tooltip 字段，五种算法 golden 由实际 resolver 更新。Gallery 源 overlay 和生成目录同步，测试数量随新增两个 live Tooltip anchors 更新。
- 格式：clang-format 22.1.3，395 owned sources，0 failures；Gallery generator write/check/self-test、Token catalog verify/self-test、OpenSpec doctor healthy、40/40 strict、git diff --check 均通过（后续规划 change 使总数增加）。

通用实现不包含具体 backend 类型或上传分支；真实 Windows GPU/system font/input/scale 验收另见 Windows evidence。Linux native 尚无本机证据，相关 checkbox 保持待验。
