add_library(stdexx INTERFACE)
target_include_directories(stdexx INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
