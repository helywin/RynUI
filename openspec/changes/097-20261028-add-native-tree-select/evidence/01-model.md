# API 与模型阶段

基线：`16899815ff28d23c1e00d85d10884b0b331e6f22`。此阶段仅完成 1.1；原生 host、可链接 TreeSelect 入口、Theme 和实际交互仍未完成，不能作为组件整体验收。

## 实际执行

Windows MSVC、官方 `windows-msvc-headless` configure，Ninja Multi-Config，最多 2 jobs。环境驱动命令保存在原始日志索引目录的 `driver.ps1`。

```text
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-msvc.ps1 -Stage configure
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-msvc.ps1 -Stage build-model -Configuration Debug
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-msvc.ps1 -Stage test-model -Configuration Debug
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-msvc.ps1 -Stage build-model -Configuration Release
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-msvc.ps1 -Stage test-model -Configuration Release
```

- configure：exit 0，6.5935286 s。
- Debug：fresh build exit 0，6.1943672 s；CTest **1/1**，real 0.04 s，wrapper 0.139993 s。
- Release：fresh build exit 0，4.37275 s；CTest **1/1**，real 0.03 s，wrapper 0.1359135 s。
- 测试包含公开 Props／Form String 类型编译、普通选择／clear／重复值、未知 key、响应式 presentation 的模型行为、受控拒绝重试／接受、普通 controlled 关闭和强制 suppression、显式重新打开、非法 key／父顺序／1025 节点／65 层拒绝、64 层接受。
- 两个模型 EXE 的 SHA256、时间戳及默认 PE 栈保留值记录于 artifacts/index.json；未用加大栈绕过问题。
- 显式 OpenSpec 1.14.0 doctor healthy、strict **97/97**；clang-format 22.1.3 和 diff 检查通过。新文件另已直接 clang-format 后纳入最终全量检查。

原始 configure 保留锁定依赖解析、PkgConfig 缺失及 HarfBuzz CMake community-maintained 提示；这些不是本阶段失败。

## 范围

没有运行实际 TreeSelect host、window、GPU、Gallery、Linux、系统 IME 或 AT。无 renderer、RC、字体、既有 Theme／golden／identity 变动。实现阶段后续任务保持 unchecked。
