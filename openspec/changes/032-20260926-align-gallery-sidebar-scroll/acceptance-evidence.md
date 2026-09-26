# 验收记录

规划阶段：用户确认参照 Ant Design 6 官方组件页，并要求可见滚动条。官方组件总览和 Layout 文档已核对；正文目录内容仍以仓库锁定的 Ant Design 6.6.5 数据为准。本 change strict validate 通过；全仓 strict 为 26/32，既有 change 013、015、016、017、018、021 失败。当前 OpenSpec CLI 不支持 `doctor --json`，报 `unknown command 'doctor'`；`git diff --check` 通过。此记录只代表规划与静态校验，不代表代码或真实窗口已完成。
