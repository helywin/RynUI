add_executable(rynui_minimal minimal/main.cpp)
rynui_configure_cpp_target(rynui_minimal)
target_link_libraries(rynui_minimal PRIVATE RynUI::RynUI)
