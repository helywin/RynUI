# 折叠面板与分段选择 Gallery 验收

日期：2026-10-24。源基线为 main `c3190b00210112a3a1ccf3a350cf0290082a4b14` 加本 change 的 Gallery 改动；包含 Collapse `bfe3562`、Segmented `d5d5ead` 和已复核的共享主题栈修复。

## 平台通用场景

实际环境为 Windows x64、MSVC、Ninja Multi-Config、`windows-msvc` Debug。构建 `rynui_token_gallery` 和 `rynui_token_gallery_frame_tests`，使用真实组件、字体、场景与 Recording renderer 数据。

- 新增 `--panels-only` 聚焦入口通过：实际 1200×900 整页平移后操作 Input、Collapse header、extra Button、Select 与 Segmented，全部指针目标位于可见视口。
- Collapse：文本编辑与迟到 session 拒绝；关闭/重开草稿和 slot/editor 身份保持；独立帮助反馈不切换面板；禁用→启用刷新真实命中；多开→accordion 受控规范化、Space 关闭；外部按钮打开未聚焦的受控 Select，再通过外部 Button 的键盘操作关闭面板，重复三次不复活旧 popup/session。
- Segmented：横向等宽与纵向小尺寸共享受控值；大尺寸富标签使用独立非受控选择；指针/左右/上下键跳过响应式禁用项；恢复可用后的真实命中；标签更新保持 Text/scene 身份。
- Default/Dark/Compact 与 1200/560 宽度变化时，检查实际 item 等宽/纵向顺序、裁剪后的可见 glyph 范围，保留全部面板与选项身份、草稿、组件/内容挂载次数。
- 两项 overlay 更新为 `partial`，明确固定拓扑、动画/RTL/a11y 等首版边界及独立原生平台范围；锁定目录重新生成。

实际 inventory：262 stable IDs、93 navigation controls、304 Buttons、900 interactions、577 visible interactions、273 live samples、67 Theme content runs、134 Inputs、21 Selects、13 Modal/Drawer、45 selection controls、2 Menus、1 Dropdown、1 Collapse、3 Segmented。两项 interaction 总数由实测更新，没有放宽断言。

完整 `rynui.token_gallery_frame` **214.29 秒通过**；连同 catalog generator/contract、reference catalog、document/viewport contracts、token Gallery contract、acceptance runner contract 与 evidence schema，CTest **9/9**，214.83 秒。clang-format 575 文件、doctor healthy、strict validate **73/73** 与 diff check 通过。

Astra R1 对相对 `c3190b0` 的七文件样例/测试/文档/目录差异只读复核，没有可操作 P1/P2；没有构建或运行测试，运行结果来自上述父任务。

## Windows 原生烟雾测试

相同 Gallery executable 运行 `--smoke`：实际 `direct3d12` / `DXIL`，display scale 1.5，262 stable IDs、273 live samples、7 frame samples，`exit_code=0`。此项只确认真实窗口/GPU 路径的启动、自动主题切换与退出；没有人工逐组件视觉、系统 IME、完整 DPI 矩阵、屏幕阅读器或 Linux 原生验收。

原始日志保留在 ignored `out/panel-gallery-{build,final-build,focused,inventory,full,format,doctor,validate,native-smoke}.log`。未执行 PR 或 archive；父任务按阶段提交推送 main，不推送远端 `t3/*` 分支。
