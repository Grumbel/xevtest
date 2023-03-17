cmake_minimum_required(VERSION 3.16)

project(xmousetest VERSION 1.0)

find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(XCB REQUIRED)
find_package(fmt REQUIRED)

add_executable(xmousetest src/xmousetest.cpp)
target_link_libraries(xmousetest PRIVATE
  XCB::XCB
  fmt::fmt
  )

# EOF #
