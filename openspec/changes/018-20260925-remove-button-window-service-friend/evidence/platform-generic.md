# Button 窗口服务访问边界平台通用验收

schema_version=1
change=018-20260925-remove-button-window-service-friend
scope=platform-generic
status=passed
execution_platform=windows
os=Microsoft Windows 11 10.0.26200 x64
compiler=MSVC 19.51.36256.0 x64
preset=windows-msvc-debug
build_system=Ninja Multi-Config
cpp_standard=C++20
dependency_mode=BUNDLED
consumer_commit_sha=6884e08d3b518270c62398fc9fd34734391a4b9c
ctest_debug_affected=14/14
button_friend_removed=passed
button_private_window_field_references=0
button_animation_and_scene=passed
mixed_window_and_idle=passed
exit_code=0

2026-09-25，Button 的三个私有窗口状态引用和 `WindowComponentServices` 中的 Button `friend` 已删除，源码搜索未发现 Button 对这些私有字段的直接访问。正式 `windows-msvc` Ninja Multi-Config/MSVC x64 Debug 构建通过；动画 runtime、材质过渡、Button spinner/scene、Input/Search/Selection 混合窗口、Gallery frame 和 idle 共 14/14 项定向 CTest 通过（19.52 秒）。这些逻辑合同只在一个受支持 preset 验证一次。

本 change 没有重复运行全量 CTest；017 在前一源码阶段的正式全量 227/227 结果不能替代本次变更后的全量结论。本轮仅声明上述受影响范围的验证。
