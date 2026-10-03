# TextArea API 与尺寸

2026-10-03，Windows/MSVC，windows-msvc-headless Debug/Release affected CTest 11/11，Debug 9.17 s、Release 6.33 s。包括 text_area、input_public_api、input_count、input_variant、input_component、password_component、search_component、theme_algorithm、theme_runtime、layout_engine、space_compact。

TextAreaProps 继承共用 Input 属性，TextAreaRef 复用 InputRef；实际 MultiLine editor 保留 LF。验证默认/动态 rows，实际空行/末尾 LF 和软折行的 autoSize 上下限，wrap=false，width reflow 不重复 shape，显式外部 width/height 优先。clear 右上停靠，count 在边框下方右对齐，隐藏后不占用。四变体、Small/Warning/disabled、count/formatter、原生提示与 autofocus/ref/onFocus/onBlur、preedit 不计数、归一化 commit/readOnly 和销毁通过。

无效初值配置在分配前拒绝；无效 reactive rows/autoSize 保留既有尺寸，后续合法值可恢复。配置更新保留唯一 content run、editor、text scene 与 ref。新 text_area CTest 同时接入 native 与 portable，实际原生阶段仍未执行。

clang-format 22.1.3 检查 436 个自有文件 0 failure；OpenSpec doctor healthy、strict validate 53/53、git diff --check 通过。此阶段没有将多行键盘、跨行 coverage、wheel/resize/onResize 或真实窗口描述为完成，继续按任务 3/5 实施。
