cmake_minimum_required(VERSION 3.16...4.0)
project(xevtest VERSION 0.1.0)
include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)
add_executable(xevtest src/xevtest.cpp)
set_target_properties(xevtest PROPERTIES
  CXX_STANDARD 26
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  )
target_link_libraries(xevtest PRIVATE
  PkgConfig::XCB
  )
install(TARGETS xevtest
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# EOF #
