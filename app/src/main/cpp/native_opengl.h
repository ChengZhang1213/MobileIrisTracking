//
// Created by Cheng Zhang on 2022/11/10.
//

#ifndef MOBILECVSOLUTIONS_NATIVE_OPENGL_H
#define MOBILECVSOLUTIONS_NATIVE_OPENGL_H

#include "common/util_texture.h"
#include "util_render_target.h"
#include "camera/camera_manager.h"
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <jni.h>
#include <vector>
#include <util_asset.h>

typedef struct gles_ctx {
    int initdone;
    int frame_count;

    char *str_glverstion;
    char *str_glvendor;
    char *str_glrender;

    int disp_w, disp_h;

    bool tex_camera_valid;
    texture_2d_t tex_static;
    texture_2d_t tex_camera;
    texture_2d_t tex_input;
    EGLImage egl_img;
    render_target_t rtarget_main;
    render_target_t rtarget_crop;
} gles_ctx_t;

class NativeOpengl {
public:
    NativeOpengl();

    virtual ~NativeOpengl();

    virtual void onCreate();

    virtual void onChange(int width, int height);

    virtual void onDraw();

    virtual void onDestroy();

    void setAssetManager(AAssetManager *assetMgr);


    // Manage NDKCamera Object
    void initCamera ();
    void onCameraPermission(jboolean granted);
    void requestCameraPermission();
    void createCamera (int facing);
    void updateTexture();
    void deleteCamera (void);

    void initTF();


private:
    gles_ctx_t          glctx{};
    AAssetManager *aAssetManager{};
    jobject  m_context;
    bool                m_cameraGranted=false;
    NDKCamera           *m_camera{};
    ImageReaderHelper   m_ImgReader;
    int                 m_camera_facing{};
    std::vector<uint8_t> m_facedet_tflite_model_buf;
    std::vector<uint8_t> m_facelandmark_tflite_model_buf;
    std::vector<uint8_t> m_irislandmark_tflite_model_buf;


};


#endif //MOBILECVSOLUTIONS_NATIVE_OPENGL_H
