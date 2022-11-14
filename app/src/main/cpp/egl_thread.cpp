//
// Created by Cheng Zhang on 2022/11/11.
//

#include "egl_thread.h"
#include "util_debug.h"
#include "util_egl.h"

EglThread::EglThread() {
    pthread_mutex_init(&pthread_mutex, NULL);
    pthread_cond_init(&pthread_cond, NULL);
}

EglThread::~EglThread() {
    pthread_mutex_destroy(&pthread_mutex);
    pthread_cond_destroy(&pthread_cond);
}


void *eglThreadImpl(void *context) {
    EglThread *eglThread = static_cast<EglThread *>(context);
    if (!eglThread) {
        LOGE("eglThreadImpl eglThread is null");
        return 0;
    }

    if (egl_init_with_native_window(eglThread->mANativeWindow) != 0) {
        LOGE("eglHelper initEgl error");
        return 0;
    }
    eglThread->isExit = false;
    while (!eglThread->isExit) {

        if (eglThread->isCreate) {
            eglThread->isCreate = false;
            eglThread->onCreate();
        }

        if (eglThread->isChange) {
            eglThread->isChange = false;
            eglThread->isStart = true;
            eglThread->onChange(eglThread->surfaceWidth, eglThread->surfaceHeight);
        }

        if (eglThread->isStart) {
            eglThread->onDraw();
            //切换缓冲区，显示
            egl_swap();

            if (eglThread->mRenderType == RENDER_MODULE_AUTO) {
                usleep(1000000 / 60);
            } else {
                pthread_mutex_lock(&eglThread->pthread_mutex);
                pthread_cond_wait(&eglThread->pthread_cond, &eglThread->pthread_mutex);
                pthread_mutex_unlock(&eglThread->pthread_mutex);
            }
        }

    }

    egl_terminate();
    //return 0表示线程结束
    return 0;

}


void EglThread::onSurfaceCreate(EGLNativeWindowType window) {
    if (mEglThread == -1) {
        isCreate = true;
        mANativeWindow = window;
        pthread_create(&mEglThread, NULL, eglThreadImpl, this);
    }
}


void EglThread::onSurfaceChange(int width, int height) {
    if (mEglThread != -1) {
        surfaceWidth = width;
        surfaceHeight = height;
        isChange = true;

        notifyRender();
    }
}

void EglThread::setRenderModule(int renderType) {
    mRenderType = renderType;
    notifyRender();
}

void EglThread::notifyRender() {
    pthread_mutex_lock(&pthread_mutex);
    pthread_cond_signal(&pthread_cond);
    pthread_mutex_unlock(&pthread_mutex);
}


void EglThread::callBackOnCreate(EglThread::OnCreate onCreate) {
    this->onCreate = onCreate;
}

void EglThread::callBackOnChange(EglThread::OnChange onChange) {
    this->onChange = onChange;
}

void EglThread::callBackOnDraw(EglThread::OnDraw onDraw) {
    this->onDraw = onDraw;
}