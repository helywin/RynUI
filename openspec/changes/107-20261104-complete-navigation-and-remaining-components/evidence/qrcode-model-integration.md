# QRCode model / export 主线集成：Linux

- 仅语义导入来源 `cee72c95f57deb307effb7a12d9a3978f12acb4b` 十三文件增量，基线 main `ff8926a547e074e676263cecfe77be513806e108`；不导父链、QR component/Tour/WIP。中央追加 Nayuki qrcodegen 1.8.0 archive/header/cpp SHA256/MIT；BUNDLED 实际通过锁验证，SYSTEM 需显式同一源码目录，不搜索/回退。旧依赖 lock 原字段保留。
- Core CPU API 支持 numeric/alphanumeric/UTF-8 ECI、binary/segments、版本/纠错/mask，返回应用持有 RGBA、PNG、SVG；不引入 renderer/SDL/device 上传或 IO。既有 Image pipeline/ABI 不变。
- 实际 Linux/GNU，`linux-gcc-headless`、Ninja Multi-Config；`cmake --build --preset linux-gcc-headless-{debug,release} --target rynui_qrcode_model_tests -j2`。CTest `^rynui.qrcode_(model|independent_decode)$` 两配置各 **2/2**。普通 model **89429 checks**；导出子进程因三条额外 PNG 确定性检查为 **89432 checks**，重复基础路径不累加。独立 ZBar PNG/SVG/UTF-8/binary/logo 解码各 **7/7**；librsvg 真实渲染 SVG，CRC/zlib 由 Python 标准库独立验证。
- Core include/link/目录 guards **3/3**，中央 dependency lock **1/1**；clang-format 22.1.3 **1041 文件、0 失败**；doctor healthy、strict **107/107**、diff check 通过。source evidence 仍标来源；raw log/exports/源码/二进制仅 ignored out。
- 本阶段保持 **74/135**、100 samples / 458 frozen IDs / 五份 FULL 66 域 golden；38.*、45.* 不在此模型阶段关闭。未新增 native/GPU、SYSTEM-mode、Windows/MSVC/D3D12、物理 IME/AT 或实际跨显示器证据。独立 main SHA 见该文件首次加入的 Git 历史及回执。
