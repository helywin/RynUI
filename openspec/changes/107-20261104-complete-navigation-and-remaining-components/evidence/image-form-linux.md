# Image 预览 actions 的 Form 隔离（Linux 来源记录）

来源 `8e6151b877a3ee07de741150fe79aef391198864`，以下为源分支结果；主线验证另见 image-form-integration.md。

- 独立增量基于共同 Image `7dab3ea5`；仅在 actions slot 执行时建立现有 `FormPrivateEditorScope`。不改变圆角、纹理、预览挂载或 renderer 合同。
- Linux GCC、`linux-gcc-headless`、Ninja Multi-Config、`-j2`：构建 Image component/resource/rounding/lazy-preview 与 public API object；`ctest --test-dir out/build/linux-gcc-headless -C <Debug|Release> -V -R '^rynui\.image_(component_.*|resource_.*|rounding|lazy_preview)$'` **各 28/28**。
- lazy-preview **各 84 断言**，其中保留 79，新增 5：公开 tail Input 接受 FormItem authority；预览可打开、私有工具栏可聚焦；编辑/Enter 不改外层 field 或提交 Form；预览正常关闭。几何/资源 rounding **各 810** 断言是既有共同阶段，未重复归为新增能力。
- clang-format 22.1.3 与 diff whitespace 通过。HEADLESS 为 CPU/合成输入；无新增 GPU 几何变化，不由此声明系统 IME、AT 或 Windows/MSVC/D3D12 通过。原始输出仅 out。
