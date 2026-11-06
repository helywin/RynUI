# Card / Empty Token 主线集成：Linux

- 唯一来源 `fca61e7e46e9afc55b14970a9900a18ebcf0c6a0`；[来源记录](card-empty-theme-linux.md) 不代替本次结果。基线为已推送 Select 修复 `288714c0365b4ed09bacf90755b45a9deb9b4c1a`。
- 仅语义导入生产 Token/snapshot/test 增量，不复制来源五份 goldens。Card 追加 Meta description/gap/block margin；Empty 追加六色 palette。Result、Timer、TokenIdentity 和既有 public aggregate 前缀保持。
- 实际 Linux/GNU，CMakePresets / Ninja Multi-Config，全部构建 `-j2`；targets：`rynui_portable_theme_algorithm rynui_portable_theme_runtime rynui_portable_theme_public_api rynui_portable_card_theme rynui_portable_card_public_api rynui_empty_result_theme_tests rynui_empty_result_public_api_tests`。
- 实际主线 writer `out/build/linux-gcc-headless/tests/portable/Debug/rynui_portable_theme_algorithm --write-goldens` 写完整五份快照。每份实际 **66 域**、key order 不变；只 identity/card/empty 改变，**315 个非 owned 域原始 JSON 值与顺序不变**。Card/Empty 数组旧前缀与其他值保留，追加后的 owned 值另与 immutable 来源对应域比对一致。
- Debug 初次 **7 pass + 1 fail**：历史 Calendar/Carousel guard 将 Card 整个旧数组要求完全相等，明确拒绝授权追加。集成修复严格检查 Card colors+1/metrics+2、Empty colors+6 的准确长度、全部旧前缀、其余子字段值/顺序；未排除整个域或跳过守卫，并加强顶层顺序检查。
- 最终 Debug `^rynui\.(portable\.(theme_algorithm|theme_runtime|theme_public_api|card_theme|card_public_api)|empty_result_(theme|public_api)|calendar_carousel_theme_baseline)$` **8/8**，0 失败/Not Run。
- Release 同一集合 **8/8**；两配置最终均 0 失败/Not Run。
- HEADLESS Core include/link 正负 guards 与 catalog check/self-test **3/3**；clang-format 22.1.3 **1028 文件、0 失败**；doctor healthy、strict **107/107**、diff check 通过。
- 任务清单仍 **68/135**，不关闭 29/33/40 或其他功能/平台项。97 samples / 458 frozen IDs 未改；原始日志与逐字段检查仅 ignored out。交付 SHA 见本文件首次加入的 Git 历史及回执。
- 本阶段是 CPU Theme/API 合同；未新增 native/GPU、Windows/MSVC/D3D12、物理 IME/AT 或实际跨显示器证据。
