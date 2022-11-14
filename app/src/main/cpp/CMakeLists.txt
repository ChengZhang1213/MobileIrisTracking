cmake_minimum_required(VERSION 3.4.1)

set(${CMAKE_C_FLAGS}, "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -Wall")

set(commonDir ${CMAKE_CURRENT_SOURCE_DIR}/common)
set(thirdpDir ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
set(jnilibDir  ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs)

set(tfliteDir /Users/chengzhang/Tools/tensorflow )
set(bazelgenDir  ${tfliteDir}/bazel-bin)

file(COPY ${bazelgenDir}/tensorflow/lite/libtensorflowlite.so
        DESTINATION ${jnilibDir}/arm64-v8a)

add_library(lib_tflite SHARED IMPORTED)
set_target_properties(lib_tflite PROPERTIES IMPORTED_LOCATION
        ${jnilibDir}/arm64-v8a/libtensorflowlite.so)

include_directories(${tfliteDir}/
        ${bazelgenDir}/../../../external/flatbuffers/include
        ${bazelgenDir}/../../../external/com_google_absl
        )

# ------------------------------------------------------------
#  for TensorFlow Lite GPU Delegate
# ------------------------------------------------------------
add_compile_options(-DUSE_GPU_DELEGATEV2)

file(COPY ${bazelgenDir}/tensorflow/lite/delegates/gpu/libtensorflowlite_gpu_delegate.so
        DESTINATION ${jnilibDir}/arm64-v8a)

add_library(lib_tflite_gpu_delegate SHARED IMPORTED)
set_target_properties(lib_tflite_gpu_delegate PROPERTIES IMPORTED_LOCATION
        ${jnilibDir}/arm64-v8a/libtensorflowlite_gpu_delegate.so)

add_compile_options(-DEGL_EGLEXT_PROTOTYPES)
add_compile_options(-DGL_GLEXT_PROTOTYPES)

add_library(iris_tracking SHARED

        #        ${CMAKE_CURRENT_SOURCE_DIR}/pose/tflite_posenet.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/app_main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/native_opengl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/egl_thread.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/camera/camera_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/camera/camera_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tflite_facemesh.cpp
        ${commonDir}/util_tflite.cpp
        ${commonDir}/util_asset.cpp
        ${commonDir}/util_egl.c
        ${commonDir}/assertegl.c
        ${commonDir}/assertgl.c
        ${commonDir}/util_shader.c
        ${commonDir}/util_matrix.c
        ${commonDir}/util_texture.c
        ${commonDir}/util_pmeter.c
        ${commonDir}/util_render2d.c
        ${commonDir}/util_render_target.c



        )


include_directories(
        ${thirdpDir}
        ${commonDir})

#target_include_directories(camera_iris PRIVATE
#        ${ANDROID_NDK}/sources/android/native_app_glue)

# add lib dependencies
target_link_libraries(iris_tracking
        android
        #        native_app_glue
        m
        camera2ndk
        mediandk
        EGL
        GLESv2
        lib_tflite
        lib_tflite_gpu_delegate
        #   lib_tflite_hexagon_delegate
        log)