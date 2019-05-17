
# 显示完整的gcc编译信息
# set(CMAKE_VERBOSE_MAKEFILE ON)

# 指定cmake最小的版本
cmake_minimum_required(VERSION 3.4)

# 定义工程名称
project(ffmpeg_test)

# 设置编译器参数，添加C++11标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")


# ？
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}")

# ？
find_package(FFMPEG REQUIRED)
find_package(SDL REQUIRED)

# 指定所有的头文件搜索目录
include_directories(SYSTEM ${FFMPEG_INCLUDE_DIR}) 

# 设置自定义变量名，设置所有源文件
set(SOURCE_FILES main.cpp)

# 指定生成的执行文件名称, 后面是源文件
add_executable(ffmpeg_test ${SOURCE_FILES})

# 指定生成的执行文件存放路径
# SET_TARGET_PROPERTIES

# 打印相关消息消息
# MESSAGE(${FFMPEG_LIBRARIES})

# 链接动态库
target_link_libraries(ffmpeg_test ${FFMPEG_LIBRARIES} SDLmain SDL )


# 指定编译类型，调试编译
# SET(CMAKE_BUILE_TYPE DEBUG)   
# 设置编译器参数         
# SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}  sdl-config --cflags --libs")  


# 添加子目录
# ADD_SUBDIRECTORY(utility)        
# ADD_SUBDIRECTORY(server)