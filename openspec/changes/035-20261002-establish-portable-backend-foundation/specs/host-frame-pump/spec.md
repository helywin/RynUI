# Spec Delta

## Purpose

使宿主能够独立管理窗口、输入和系统服务，并为原生等待式与未来 callback 式宿主提供同一非阻塞帧处理入口，明确挂起呈现、唤醒合并和生命周期失效行为。

## ADDED Requirements

### Requirement: Independent host and GPU lifetime
宿主 SHALL 无需 GPU 即可创建窗口与系统服务。GPU binding MUST 自己负责设备与窗口绑定的创建和清理，绑定失败 SHALL 不销毁已创建的宿主。

#### Scenario: GPU creation or claim failure
- **WHEN** GPU 设备创建或窗口 claim 失败
- **THEN** binding 清理已创建的 GPU 资源，宿主仍可泵事件和访问系统服务，最终宿主独立清理窗口

### Requirement: Nonblocking callback tick
共同帧处理 SHALL 提供无等待的 tick，支持 wake/deadline 合并、单调时间和重入拒绝；原生等待循环 SHALL 复用此入口。

#### Scenario: Coalesced wake
- **WHEN** 多次更新在同一次 callback 前请求帧
- **THEN** 只安排一个有效 wake，tick 不调用阻塞 wait，最新场景提交一次

#### Scenario: Stale callback and reentry
- **WHEN** 宿主停止/重启后旧 callback 到达或 tick 中再次进入 tick
- **THEN** 旧 callback 与重入不执行场景更新或提交

### Requirement: Deferred presentation resume
surface unavailable 时 SHALL 保留最新待呈现 revision，并进入无立即重试的 idle 状态；显式恢复/wake 后 SHALL 呈现最新 revision，已接受的资源不得因挂起而重复上传。

#### Scenario: Unavailable surface
- **WHEN** 资源已接受但呈现 deferred，且没有新的 wake 或 deadline
- **THEN** 调度不产生忙循环；恢复后提交最新内容并清除待呈现状态
