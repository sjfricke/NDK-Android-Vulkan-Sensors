cmake_minimum_required(VERSION 3.4.1)

# build native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library( app-glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp)
set(LIBS_DIR libs)

set(VK_VAL_LAYER_SRC_DIR ${ANDROID_NDK}/sources/third_party/vulkan/src)
include_directories(${VK_VAL_LAYER_SRC_DIR}/include)

add_library( HeartBeat SHARED

             ${SRC_DIR}/vulkan_wrapper.cpp

             ${SRC_DIR}/AndroidMain.cpp
             ${SRC_DIR}/VulkanMain.cpp
             ${SRC_DIR}/ValidationLayers.cpp
             ${SRC_DIR}/Sensor.cpp)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fexceptions \
                    -DVK_USE_PLATFORM_ANDROID_KHR \
                    -DUSE_DEBUG_EXTENTIONS \
                    -DGLM_FORCE_SIZE_T_LENGTH -DGLM_FORCE_RADIANS")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_subdirectory(src/main/cpp/glm)

add_library(assimp SHARED IMPORTED)
set_property(TARGET assimp PROPERTY IMPORTED_LOCATION ${SRC_DIR}/assimp/libs/armeabi-v7a/libassimp.so)

include_directories(${SRC_DIR}/assimp/include)

target_link_libraries( HeartBeat
    assimp
    app-glue
    log
    glm
    android)

 add_custom_command(TARGET HeartBeat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${SRC_DIR}/assimp/libs/armeabi-v7a/libassimp.so ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})