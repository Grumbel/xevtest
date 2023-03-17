cmake_minimum_required(VERSION 3.16)

project(xevtest VERSION 0.0)

include(GNUInstallDirs)

find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(XCB REQUIRED)
find_package(fmt REQUIRED)

add_executable(xevtest src/xevtest.cpp)
target_link_libraries(xevtest PRIVATE
  XCB::XCB
  fmt::fmt
  )

install(TARGETS xevtest
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# EOF #
