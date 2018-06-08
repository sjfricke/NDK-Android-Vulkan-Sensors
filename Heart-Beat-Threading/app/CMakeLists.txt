cmake_minimum_required(VERSION 3.4.1)

# build native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library( app-glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)
set(EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../external)

set(VK_VAL_LAYER_SRC_DIR ${ANDROID_NDK}/sources/third_party/vulkan/src)
include_directories(${VK_VAL_LAYER_SRC_DIR}/include)

add_library( HeartBeat SHARED
             ${SRC_DIR}/vulkan_wrapper.cpp
             ${SRC_DIR}/ValidationLayers.cpp
             ${SRC_DIR}/ModelLoader.cpp
             ${SRC_DIR}/VulkanMain.cpp
             ${SRC_DIR}/Sensor.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fexceptions \
                    -DVK_USE_PLATFORM_ANDROID_KHR \
                    -DUSE_DEBUG_EXTENTIONS \
                    -DGLM_FORCE_SIZE_T_LENGTH -DGLM_FORCE_RADIANS")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

include_directories(${EXTERNAL_DIR}/gli/external) # glm
include_directories(${EXTERNAL_DIR}/gli) # gli

target_link_libraries( HeartBeat
    app-glue
    log
    android)