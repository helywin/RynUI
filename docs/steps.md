# Steps

`std::vector<StepsItem>` 或响应式 `Prop<std::vector<StepsItem>>`，key 非空且唯一。`current` 为受控 `Prop<std::size_t>`，`defaultCurrent` 为默认索引，两者互斥；默认 0。`current >= count` 表示全部完成，不回写 current。单项 `status` 优先于按原始索引派生的状态。只有 `onChange` 存在才启用导航，受控拒绝可重试。

标题、副标题、说明、禁用、可见和状态响应式更新；typed 富 slots 只挂载一次。响应式集合按 key 增删和重排，未变项保留身份。相同 key 的富 slot 工厂是构造内容；替换富结构需使用新 key，文本和状态可直接更新。

在窗口 `Content` 中调用。以下受控示例由应用明确接受请求；保留 `current` 值即可拒绝，后续显式激活可以重试。函数体作为 API 示例编译验证：

```cpp
#include <ryn/rynui.hpp>

void steps_example(ryn::Signal<std::size_t>& current, ryn::Signal<bool>& review_visible) {
    using namespace ryn;
    Steps(StepsProps{}.current(current).size(StepsSize::Small)
              .onChange([&current](std::size_t next) { current.set(next); }),
          {StepsItem{u8"details", u8"Details"}.description(u8"Enter details"),
           StepsItem{u8"review", u8"Review"}.subtitle(u8"Optional").visible(review_visible)
               .icon(StepsIcon{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }}),
           StepsItem{u8"complete", u8"Complete"}.richDescription(StepsDescription{[] {
               Text(u8"Ready to submit");
           }})});
}
```

`StepsProps{}.defaultCurrent(1).onChange(callback)` 使用内部 current authority；不设置 `onChange` 时为展示组件。`status(Prop<StepsStatus>)` 控制当前步骤状态，`StepsItem::status(Prop<std::optional<StepsStatus>>)` 用 `nullopt` 恢复自动推导。状态为 `Wait`、`Process`、`Finish`、`Error`，默认当前状态 `Process`。编号始终为原始索引加一，隐藏项不使后续步骤改号或改变 callback index。

`vertical(true)` 纵向排列，默认横向；`StepsSize::Default` 与 `Small` 两尺寸。横向均分可用宽度，纵向按内容排列；编号、check/error、状态圆和连接线是原生 logical scene。`richTitle`、`richSubtitle`、`richDescription`、`icon` 都是独立 typed slots。原生子 Button/Input 保留自己的激活、编辑及输入会话。

未提供 `onChange` 时只有步骤自身不参与导航/Tab，富 slots 中启用的原生 Button/Input 仍可点击、聚焦和编辑。项的显式 `disabled`、隐藏，以及 Modal/Spin/窗口失活边界屏蔽整支输入；恢复保留子控件身份。

外扩焦点轮廓使用有效的外部祖先裁剪，留有空间时四周可见；编号、状态圆、连接线、富内容和命中仍严格受内容分配范围限制。零宽或零高不绘制焦点轮廓。

只有一个可见启用步骤作为 roving Tab stop。方向键（横向 Left/Right、纵向 Up/Down）及 Home/End 仅移动焦点；Enter/Space 和主指针显式请求切换。repeat/keyup 不额外激活，激活当前索引不重复回调。Modal、Spin、隐藏项和失活边界清理不可用交互；全隐藏或空 items 时不占普通布局位置，恢复保留身份。

Theme 使用 `ThemeConfig::steps` 的独立 `tokens`、`seed`、`algorithm`；颜色更新不重测量，最终派生度量先校验再发布。`LayoutStyle` 只控制外部布局。有限宽高下裁剪实际文字、富内容、表面与命中。

Change 107 增加下述原生合同；平台证据分开记录，不代表系统 IME 或辅助技术桥接。

## Gallery

Gallery 目录的 Steps 跳转到 `gallery.steps.workflow`。发布流程由应用接受 `onChange` 并回写 current，显示原始索引与操作反馈；上一步/下一步可推进到全部完成。“横纵切换”“切换尺寸”“切换错误状态”“显隐审核”“锁定资料与审核”更新真实 Props。资料说明内 Input 与“保存备注”按钮独立操作；外部重置恢复可用状态并保留备注，“编辑备注”使用原 InputRef。

方向键与 Home/End 只移动步骤焦点，Enter/Space 才请求切换；隐藏审核后发布步骤仍是原始索引 2。平台通用 Recording 命令为 `out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --navigation-only`，执行结果见 [082 证据](../openspec/changes/082-20261025-add-navigation-gallery/evidence/01-gallery.md)。该验证覆盖 Default/Dark/Compact × 1200/560 × 900 的 logical scene、真实事件和保留身份，不代表原生 GPU、窗口、Linux、IME 或辅助技术验收。


## Change 107

`type(Default|Dot|Inline|Navigation|Panel)` 分别绘制编号状态圆、居中点、带保留说明浮层的紧凑点、导航下划线与面板背景；变体更新不重新挂载富内容。`percent(optional<float>)` 为 Default 当前 Process 状态绘制逻辑单位进度环，范围 0–100。`maxCount(0)` 不折叠；大于等于 3 时在当前项周围保留有限窗口，隐藏范围以不可交互省略项表示。

`responsive(true)` 默认开启，共享窗口宽度小于 532 logical px 时使用纵向（Inline 保持紧凑横向）；`vertical(true)` 显式纵向优先。`rtl` 镜像横向顺序及方向键。动画使用 Theme motion 和组件拥有的有限 target；`animated(false)`、reduced motion、失活和销毁结束动画。

动态项要求非空唯一 key。删除当前项时，非受控索引夹紧到邻项；重排保留当前 key。受控 current 始终由应用决定。动态工厂抛异常恢复原结构，禁用、折叠和移除项清理输入边界。Gallery 增加动态重排/保留编辑器和四种展示变体。

依据：[Ant Design 固定 Steps API](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/steps/index.en-US.md)。自动验证和实际平台边界见 change 107 evidence。
