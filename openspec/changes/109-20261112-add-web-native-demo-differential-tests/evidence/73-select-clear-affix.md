# Select 清除图标与输入区验证

- 固定 Ant Design 6.6.5 的 Select 默认清除图标与尾部图标在同一位置切换。原生默认路径在指针进入后显示清除图标，离开后恢复尾部图标，保持原来的编辑器、清除动作和焦点关系。显式自定义尾部继续使用原有 slot。
- Windows MSVC HEADLESS Release 的 Select、选择组件 i18n 和 Gallery native differential input 三个 CTest 入口全部通过。定向断言覆盖真实指针进入、离开及清除，核对相同输入区尺寸和保留的动作身份。
- 真实 GPU 的 Select 分片重拍与完整三例 paired 结果待集成后验证。
