cmake_minimum_required(VERSION 3.4.1)

# build native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})

set(SRC_DIR src/main/cpp)


add_library( app-glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)

add_library( AccelCube SHARED

             ${SRC_DIR}/vulkan_wrapper.cpp

             ${SRC_DIR}/AndroidMain.cpp
             ${SRC_DIR}/VulkanMain.cpp
             ${SRC_DIR}/Cube.cpp)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror \
                    -DVK_USE_PLATFORM_ANDROID_KHR")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

target_link_libraries( AccelCube app-glue log android )
