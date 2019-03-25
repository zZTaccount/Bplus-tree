##
# @file CMakeLists.txt
# @brief
# tests目录下cmake文件
#
# @author niexw
# @email xiaowen.nie.cn@gmail.com
#
# include_directories(${CMAKE_SOURCE_DIR}/include)

# catch要求打开异常
string(REGEX REPLACE "-fno-exceptions" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

# 设置单元测试的文件
set(TEST 
    test.cc 
)

# 测试文件
add_executable(utest ${TEST})
add_dependencies(utest bplustree)
target_link_libraries(utest bplustree)
