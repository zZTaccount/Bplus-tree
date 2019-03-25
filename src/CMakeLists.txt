##
# @file CMakeLists.txt
# @brief 
#
# @author zzt
# @emial 429834658@qq.com
##


include_directories(${CMAKE_SOURCE_DIR}/include)

set(BPLUSTREE_SRC
 #Bp_node.cc
 Bp_tree.cc
 )
# ADD_EXECUTABLE(bplustree ${BPLUSTREE_SRC})
# 编译为静态库
add_library(bplustree ${BPLUSTREE_SRC}) 
