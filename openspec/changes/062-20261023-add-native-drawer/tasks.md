# Tasks

## 1. 平台通用：首版 Drawer

- [x] 1.1 实现 typed API、几何验证与独立主题，使用 drawer_public_api/model/theme Debug 测试验证；同步 docs/drawer.md 记录默认值与边界。
- [x] 1.2 共享 Modal 栈实现四边 Drawer、固定 header/footer、滚动与 retained 生命周期；使用 drawer_component Debug 测试验证四边、窗口尺寸、取消、焦点、销毁、混合嵌套和精确更新。
- [ ] 1.3 执行原 Modal model/public_api/theme/boundary/component 聚焦 Debug 回归、format-code --check、OpenSpec doctor/严格全量文档校验与 git diff --check；记录实际 MSVC preset 和命令证据后英文提交、push、核对远端 SHA。

## 2. 集成边界（父任务）

Gallery、真实 Windows/Linux 窗口、DPI/GPU 与人工系统 IME 不属于本子任务验收，不在此勾选或宣称完成；由父任务独立集成。
