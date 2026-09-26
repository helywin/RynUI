## ADDED Requirements

### Requirement: 宽窗口独立滚动
RynUI SHALL 在 Token Gallery 的宽窗口两列布局中分别维护导航与正文滚动 offset。正文滚动 SHALL 不改变导航 Node 的视觉位置，导航滚动 SHALL 不改变正文 offset；两个 offset SHALL 受各自 extent 夹紧。

#### Scenario: 指针位于正文列滚动
- **WHEN** 滚轮事件落在正文列且正文内容超过视口
- **THEN** 仅正文滚动并保持左栏可见位置与身份不变

#### Scenario: 指针位于左侧导航列滚动
- **WHEN** 滚轮事件落在导航列且导航条目超过视口
- **THEN** 仅导航滚动，正文 offset 与当前文档 section 不变

### Requirement: 窄窗口可达性
RynUI SHALL 在固定顶栏下的单列布局允许从导航到正文末尾的连续滚动，并在宽窄切换时把 offset 限定到新视口范围，保持组件挂载身份。

#### Scenario: 缩小后恢复窗口
- **WHEN** Gallery 从宽窗口切换到窄窗口再恢复
- **THEN** 所有 section 与真实样例仍可到达，且不因滚动重建无关组件

### Requirement: 可见滚动条
RynUI SHALL 为宽窗口的左栏和正文分别绘制可见的纵向轨道与滑块，为窄窗口的单列内容绘制一条；滑块 SHALL 反映当前 offset 与可滚动范围，支持指针拖动和点击轨道，短内容不得产生越界滚动。

#### Scenario: 拖动左栏滑块
- **WHEN** 用户按下左栏滑块并拖至轨道末端后抬起
- **THEN** 左栏到达自身最大 offset，正文 offset 不变，滑块停留在轨道末端

#### Scenario: 点击正文轨道
- **WHEN** 用户在正文滑块外点击轨道
- **THEN** 正文按视口步进并夹紧 offset，左栏不移动

#### Scenario: 持续往返拖动正文滑块
- **WHEN** 用户保持按下并连续拖过正文中段至末端，再返回起点并抬起
- **THEN** 每个位置的 offset 与滑块一致，所有中间帧安全提交，且结束后普通指针交互恢复
