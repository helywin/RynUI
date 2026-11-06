# Card / Empty 追加 Token（Linux 来源记录）

来源 `fca61e7e46e9afc55b14970a9900a18ebcf0c6a0`；以下为源分支结果，主线验证另见 card-empty-theme-integration.md。

- Card 追加 Meta description 色、gap 与 block margin；Empty 追加锁定插画的六色 palette。保留原 public aggregate 字段顺序、TokenIdentity 名称/顺序与 Result 域；无独立 Timer token 域。
- Linux GCC / `linux-gcc-headless` / Ninja Multi-Config / `-j2`，构建 Theme algorithm、Card theme、Empty/Result theme targets；定向 `ctest -C <Debug|Release> -R '^rynui\.(theme_algorithm|card_theme|empty_result_theme)$'` **D/R 各 3/3**。
- 使用真实 `rynui_theme_algorithm_tests --write-goldens` 写五份 FULL goldens；与 `4d722fbc` 比较，每份仍有完整 **66** 顶层字段，顺序不变，五份 **330** 字段齐全；只 identity/card/empty 变化，**315** 非 owned 值不变。未裁剪 golden 或只写局部对象。
- clang-format 22.1.3 与 diff check 通过。CPU Theme 合同不宣称 GPU/Windows/物理 IME/AT 覆盖；组件几何、Gallery 和平台汇总单独记录，29/33/40 不由此完成。原始输出只 out。
