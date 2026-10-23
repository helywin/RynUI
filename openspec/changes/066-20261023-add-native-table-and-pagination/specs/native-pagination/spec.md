# Spec Delta

## Purpose

为原生业务列表提供可组合的有限页码导航，保持总数、当前页与每页数量的响应同步，并让应用能够明确接受或拒绝分页请求而不丢失受控状态。

## ADDED Requirements

### Requirement: 有界分页与受控回调
Pagination SHALL 支持 reactive total/current/pageSize、defaultCurrent、disabled、hideOnSinglePage 和尺寸。页码从 1 开始，有效显示页随总数收缩夹紧；每页数量必须大于零。受控输入是唯一事实来源，点击只提出 onChange(page, pageSize)，等值和禁用操作不发回调。

#### Scenario: 应用拒绝或接受翻页
- **WHEN** 用户点击下一页，应用先拒绝、再回写 current
- **THEN** 拒绝时显示页不变，回写后更新显示页，边界操作不越界

### Requirement: 原生 retained 分页视觉和生命周期
Pagination SHALL 提供前后页、首尾页与有限邻近页窗口，支持 pointer/keyboard focus、Theme typed token，普通页码和颜色更新不得重跑无关内容；销毁后释放所有子控件。

#### Scenario: 总数和主题变动
- **WHEN** 总数变为零或主题切换，之后销毁分页
- **THEN** 显示合法页码或按配置隐藏，主题更新局部生效，销毁后无交互和 scene 残留
