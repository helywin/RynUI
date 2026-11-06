# 全目录巡检修复结果

## 阶段一：栈压力与 MSVC 编译

Windows/MSVC `windows-msvc-release`、Ninja Multi-Config、最多 2 jobs，正式 preset 重建 Gallery 及 Signal/Prop/ThemeScope/Anchor/TransferComplete 五个测试 target。Anchor 已以显式类型递归遍历通过标准编译，未使用 `_CL_` 绕过；Gallery PE 栈保留仍为默认 1 MiB。

内部大值转交改为右值引用，公开 Signal/Prop 按值入口保持复制隔离；Theme 配置初始化返回后再挂载子内容，Transfer 轻量重入检查在重型刷新帧之外；验收宿主的配置更新同样在挂载前完成，避免常驻多份完整 ThemeConfig。

`ctest --test-dir out/build/windows-msvc -C Release --no-tests=error -R "^rynui\.(signal|prop|theme_scope_component|anchor_component|transfer_complete)$"`：**5/5 通过**。新增大值重入/复制隔离、25 层透明 Theme、Anchor 前序/继承/非法 key/数值/128–129 层边界断言，旧身份与失败回滚断言保留。

原隐藏 GPU 入口四组复跑：Transfer **exit 0 / 15 图**；Avatar **exit 0 / 16 图**，覆盖两个样例组；Cascader 已完成首次布局和 4 张捕获，随后 **exit 1 / ComponentId is stale or invalid**，该新暴露问题仍待修复。三入口此阶段均未再返回栈溢出，**不据此宣称 Cascader 整页通过**。原始失败、调试器异常栈、图像仅在 out。

本阶段为增量构建与隐藏 Windows/D3D12 回读；没有 Linux、桌面 swapchain、系统 IME/AT 或高刷验收结论。
