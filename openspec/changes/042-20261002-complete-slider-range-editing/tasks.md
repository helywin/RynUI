# Tasks

## 1. 规划（平台通用）

- [x] 1.1 完成 proposal/spec/design/tasks，doctor/full strict/diff 通过，保存锁定源参考与规划证据并提交；按用户授权立即 apply。

## 2. 共用动态端点（平台通用）

- [x] 2.1 新增 MultiSlider typed API 和 count/options 数值合同；统一保留 thumb 状态、动态 value topology/受控候选/独立焦点与提示，补旧 API 兼容、空/重合/64 上限/非法原子拒绝/增删 identity/cleanup 合同；headless Debug/Release focused CTest、格式/doctor/strict/diff 后记录文档/evidence 并提交。

## 3. 整段轨道（平台通用）

- [x] 3.1 实现双端/多端 draggableTrack、起始快照/边界/候选/取消/互斥配置；覆盖规则 step 与不规则 marks、reverse/vertical、controlled echo/capture/window loss/reentrant；headless Debug/Release focused CTest、格式/doctor/strict/diff 后记录文档/evidence 并提交。

## 4. 编辑与禁用（平台通用）

- [x] 4.1 实现 editable 插入、Delete/Backspace、跨轴拖出删除预览、min/max count、逐端点 disabled 和焦点迁移；覆盖 empty/maxCount/minCount、repeat、controlled 不回写、配置取消、destroy/rollback/reentrant；headless Debug/Release focused CTest、格式/doctor/strict/diff 后记录文档/evidence 并提交。

## 5. 原生 API 与整合（平台通用）

- [x] 5.1 实现安全 SliderRef focus/blur/autoFocus 与 hint overflow/默认纵向位置，补寿命/owner thread/复用/配置合同；更新 Gallery、API 文档与原生支持范围；headless Debug/Release 完整 CTest、格式/doctor/strict/diff 后记录 evidence 并提交。

## 6. Windows 原生验收

- [x] 6.1 windows-msvc Debug/Release build 和受影响平台 CTest；实际 D3D12/DXIL 多端/整段拖动/编辑/禁用/keyboard/capture/主题/resize/system fonts 和 scale=1/1.25/1.5/2；保存 GPU 图像、诊断、SHA256/退出码并独立提交。

## 7. Linux 原生验收

- [ ] 7.1 实际 Linux GCC/Clang native preset 验证 Vulkan/SPIR-V/Fontconfig/Wayland 多端与编辑输入/DPI/resize，独立保存 evidence 并提交，不重复已通过的平台通用合同。
