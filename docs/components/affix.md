# Affix

原生合同：`Affix(AffixProps, AffixContent)`。Windows/MSVC Debug/Release 定向测试和真实 D3D12 Gallery 验证已完成，结果见[验收摘要](../../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/decoration-windows.md)。

`target` 必须显式提供 `AffixTarget`，包括非零目标身份、窗口逻辑坐标中的 viewport、滚动 offset 和 attached 状态。宿主仍负责实际原生滚动子树的布局或 translation；offset 只触发重新跟踪，Affix 不二次应用滚动位移，也不模拟 DOM。目标卸载发布 `attached=false`；切换 identity 或几何会重新定位同一内容。

`offsetTop` / `offsetBottom` 接受有限逻辑偏移；均省略时顶部偏移为零。顶部越界时优先固定顶部，否则检查底部越界。占位组件保留原自然布局空间，真实子树经过已有 retained translation 阶段按祖先顺序定位，固定时使用带目标 viewport clip 的 window layer；释放后恢复自然位置与祖先 clip 链。Input、Button 等仍由同一原生节点负责命中、焦点和编辑。

`onChange(bool)` 只在固定状态改变后触发，派发在布局之后；回调销毁组件不会继续访问旧状态，抛异常会退役完整组件。`visible` / `disabled` 清理输入资格并恢复非固定位置；卸载释放整个保留子树。层级由 `ThemeConfig.affix.tokens.z_index` 控制，不增加视觉 Props。普通目标、偏移和 Theme 更新不重建子内容，不产生轮询 deadline。

Gallery 使用真实文档滚动与显式窗口目标区域，提供顶部输入、底部按钮、切换目标、偏移和释放操作。自动原生输入/裁剪检查使用 hidden readback；滚动后的 onChange 反馈文案允许改变，几何与保留身份仍必须一致。Linux、其他实际 DPI、系统 IME 和 AT 未实测；原始输出仅放 `out/`。
