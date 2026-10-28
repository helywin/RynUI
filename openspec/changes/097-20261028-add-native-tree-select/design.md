# Design

## Context

动机见 proposal。基线为 main `3e49f7e`；已读取公开 Select、Tree、Popover、Form 接口，host、窗口 participant 顺序、Theme resolve 末尾和专项夹具。Ant Design 6.6.5 本地锁定源码 `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 TreeSelect 文档明确单选默认无搜索，值与 label 分离。现有 Tree 固定拓扑且以 onSelect 提供 key；Popover 已负责 window-layer 定位、边界和焦点恢复；Form 支持 String。

## Goals / Non-Goals

**Goals:** 复用真实 retained Tree 与 popup 基础设施；组合层只维护单值／显示／开合 authority、TreeSelect Theme 映射和 selector 入口。不复制树的展开、滚动或绘制算法。

**Non-Goals:** 见 proposal 的首版排除项；不扩展 FormValue、不新建 renderer 上传路径，不把此阶段 HEADLESS 结果记作 Gallery 或分平台原生验收。

## Decisions

### 值与树数据

`TreeSelectItem(key, title, children)` 提供固定数据，title/disabled/visible 为 Prop。key 必须非空，String value 的空值用作未选，因此直接绑定 FormField<String>。相比新增 optional<String> Form 类型，这与现有 Date/Time String adapter 一致且不改变 Form 存储。未知外部 key 保留并显示原 key；隐藏／disabled 变化不擅自清除已接受值。Tree 的选择回调以 info.key 激活，不能把 Tree 的重复点击 deselect 当作 TreeSelect clear。

### 组合与输入

私有 host 组合原生 readOnly Input（TreeSelect readOnly 另行封锁选择）、无箭头 Popover 和受控 selectedKeys Tree。展开可受控或本地保存。外层 LayoutStyle 只影响 selector；popup 的宽度匹配已提交 selector，并受 viewport 限制，高度由 listHeight 有限约束。使用现有 ancestor popup 定位、最近 layer 归属、内容先处理滚轮及 settled focus observer；仅补必要的 TreeSelect 入口协调。

首个键盘打开把焦点放到当前可操作节点或第一行；Tree 继续负责上下／左右／Home／End、展开与 reveal。选择／Escape 关闭后恢复 selector；Tab 以 selector 为离开锚点，避免 popup 的 portal 声明顺序跳至全局首项。所有可回调交付后按完整 generation 重取状态。零面积在真实布局后建立输入边界，恢复显式清除 suppression 才可打开。

### 生命周期

从 owner 创建开始用局部挂载事务；首次 getter、连接、主题同步或子结构异常在自动 slot 回滚之前调用既有 `retire_failed_component`，静默释放输入源并重新抛出原异常。State 由 shared_ptr 保持调用期间对象寿命，但有效性仍取决于 ComponentId、scope 与私有结构完整性；每次用户回调后重取，不以 shared_ptr 代替完整 generation 校验。

selector 固定为原生只读显示，不建立编辑 session；因此 2.3 的输入验证是实际 focus 后 session 仍未启动，而不是伪造一个首版不支持的搜索／preedit 流程。用户 Prop 先在 owner 自有 Scope 中连接为私有 Signal，再传给子组件，避免首次子组件 getter 异常早于组合退役守卫。autoFocus 属于首次布局后的行为，不作为 Content 内局部捕获挂载异常的探针。

### Theme

TreeSelect 以独立配置提供 selector（Select recipe）与 tree（Tree recipe）两组 override，组件 seed/algorithm 统一解析。快照由共享只读堆对象持有；末尾追加 resolve IIFE、identity、序列化及 dirty，不改旧块。内部 Theme 只映射到 Input、Popover 与 Tree 自有 Token；继承全局字体和算法，最终字号量化边界、有限组合尺寸在发布前验证。保留静态默认 ThemeConfig 的引用路径，不增加大栈临时值。

### 验证与发布

平台通用验证在 Windows MSVC `windows-msvc-headless`，Ninja Multi-Config，Debug/Release，最多 2 jobs；专项覆盖真实输入、authority、Form、Theme、clip、寿命与异常，复用相邻 Tree/Popover/Focus/Input 回归。比较五份 fresh golden 的旧 typed 值、原 IIFE/ordinal 前缀，检查新增 PE 默认 1 MiB 栈。阶段英文 Conventional Commit、仅 main push 并核远端 SHA；独立 Astra xhigh/default 审核完成后收口。

## Risks / Trade-offs

- 组合回调可能同步卸载 → 初始事务、内层失败守卫、完整代际和私有 graph 校验。
- Popover 默认尺寸不等于 Select → 内部有限内容尺寸和 selector 实际几何驱动，覆盖初帧与祖先同帧移动。
- portal 焦点声明顺序 → 实测 Before→selector→Tree→After 与 Escape 恢复，不仅检查 focus_visible 布尔值。
- 复用 recipe 可能污染父 Token → 自有 TreeSelect snapshot 和子 Theme override，旧组件金样与 dirty locality 对比。

## Migration Plan

公开 API 仅追加；现有使用者无需迁移。无运行时数据格式变化。若需回滚，用正常 revert 撤销本阶段提交，不重写已发布历史。
