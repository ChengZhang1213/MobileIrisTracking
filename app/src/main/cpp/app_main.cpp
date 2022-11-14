//
// Created by Cheng Zhang on 2022/11/11.
//
#include <jni.h>
#include <string>
#include <iostream>
#include <memory>
#include <iomanip>
#include <unordered_map>
#include <android/native_window_jni.h>
#include <thread>
#include "util_asset.h"
#include "util_debug.h"
#include "egl_thread.h"
#include "native_opengl.h"
#include "camera/camera_manager.h"

#define CAMERA_RESOLUTION_W     640
#define CAMERA_RESOLUTION_H     480
EglThread *eglThread = nullptr;
NativeOpengl *native_opengl = nullptr;
using namespace std;

void callBackOnCreate() {
    native_opengl->onCreate();
}

void callBackOnChange(int width, int height) {
    native_opengl->onChange(width, height);
}

void callBackOnDraw() {
    native_opengl->onDraw();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_chengzhang_iristracking_NativeOpengl_setAssetManager(JNIEnv *env, jclass clazz,
                                                              jobject asset_manager) {
    AAssetManager *assetManager = AAssetManager_fromJava(env, asset_manager);
    native_opengl = new class NativeOpengl();
    native_opengl->setAssetManager(assetManager);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_chengzhang_iristracking_NativeOpengl_surfaceCreate(JNIEnv *env, jclass clazz,
                                                            jobject surface) {
    eglThread = new EglThread();
    eglThread->callBackOnCreate(callBackOnCreate);
    eglThread->callBackOnChange(callBackOnChange);
    eglThread->callBackOnDraw(callBackOnDraw);
    eglThread->setRenderModule(RENDER_MODULE_AUTO);


    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    eglThread->onSurfaceCreate(nativeWindow);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_chengzhang_iristracking_NativeOpengl_surfaceChange(JNIEnv *env, jclass clazz, jint width,
                                                            jint height) {
    if (eglThread) {
        eglThread->onSurfaceChange(width, height);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_chengzhang_iristracking_NativeOpengl_surfaceDestroy(JNIEnv *env, jclass clazz) {
    if (eglThread) {
        eglThread->isExit = true;
        //等待线程结束
        pthread_join(reinterpret_cast<pthread_t>(eglThread), NULL);
        delete (eglThread);
        eglThread = NULL;
    }
}


extern "C"
JNIEXPORT void JNICALL
Java_com_chengzhang_iristracking_MainActivity_notifyCameraPermission(JNIEnv *env, jclass clazz,
                                                                     jboolean granted) {
//    std::thread permissionHandler (native_opengl->onCameraPermission,  granted);
//    permissionHandler.detach();
     // native_opengl->onCameraPermission(granted);
}