//
// Created by Cheng Zhang on 2022/11/10.
//

#include "native_opengl.h"
#include "util_egl.h"
#include "util_render2d.h"
#include "util_pmeter.h"
#include "util_matrix.h"
#include "util_debug.h"
#include "assertgl.h"
#include "tflite_facemesh.h"

#define CAMERA_RESOLUTION_W     640
#define CAMERA_RESOLUTION_H     480
#define CAMERA_CROP_WIDTH       480 /* make a src image square */
#define CAMERA_CROP_HEIGHT      480 /* make a src image square */

NativeOpengl::NativeOpengl() {
    memset(&glctx, 0, sizeof(glctx));
}

NativeOpengl::~NativeOpengl() {
    glctx.tex_camera_valid = false;
}

void LoadInputTexture(texture_2d_t *tex, AAssetManager *assetMgr, char *fname) {
    int32_t img_w, img_h;
    uint8_t *img_buf = asset_read_image(assetMgr, fname, &img_w,
                                        &img_h);
    create_2d_texture_ex(tex, img_buf, img_w, img_h, pixfmt_fourcc('R', 'G', 'B', 'A'));
    asset_free_image(img_buf);

}

void NativeOpengl::onCreate() {
    initTF();
    initCamera();
    int ret;
    glctx.str_glverstion = (char *) glGetString(GL_VERSION);
    glctx.str_glvendor = (char *) glGetString(GL_VENDOR);
    glctx.str_glrender = (char *) glGetString(GL_RENDERER);
    int w, h;
    egl_get_current_surface_dimension(&w, &h);

    init_2d_renderer(w, h);
    init_pmeter(w, h, h - 100);
    //init_dbgstr (w, h);
    glctx.disp_w = w;
    glctx.disp_h = h;
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    LoadInputTexture(&glctx.tex_static, aAssetManager, (char *) "pexels-photo-415829.jpeg");
    /* render target for default framebuffer */
    get_render_target(&glctx.rtarget_main);

    /* render target for camera cropping */
    create_render_target(&glctx.rtarget_crop, CAMERA_CROP_WIDTH, CAMERA_CROP_HEIGHT, RTARGET_COLOR);
    glctx.tex_input.texid = glctx.rtarget_crop.texc_id;
    glctx.tex_input.width = glctx.rtarget_crop.width;
    glctx.tex_input.height = glctx.rtarget_crop.height;
    glctx.tex_input.format = pixfmt_fourcc('R', 'G', 'B', 'A');


    glctx.initdone = 1;
}

void NativeOpengl::onChange(int width, int height) {
    LOGI("NativeOpengl::onChange %d %d", width, height);
    glctx.disp_w = width;
    glctx.disp_h = height;

    // glViewport(0, 0, width, height);

}

static void
adjust_texture(int win_w, int win_h, int texw, int texh,
               int *dx, int *dy, int *dw, int *dh, int full_zoom) {
    float win_aspect = (float) win_w / (float) win_h;
    float tex_aspect = (float) texw / (float) texh;
    float scale;
    float scaled_w, scaled_h;
    float offset_x, offset_y;

    if (((full_zoom == 0) && (win_aspect > tex_aspect)) ||
        ((full_zoom == 1) && (win_aspect < tex_aspect))) {
        scale = (float) win_h / (float) texh;
        scaled_w = scale * texw;
        scaled_h = scale * texh;
        offset_x = (win_w - scaled_w) * 0.5f;
        offset_y = 0;
    } else {
        scale = (float) win_w / (float) texw;
        scaled_w = scale * texw;
        scaled_h = scale * texh;
        offset_x = 0;
        offset_y = (win_h - scaled_h) * 0.5f;
    }

    *dx = (int) offset_x;
    *dy = (int) offset_y;
    *dw = (int) scaled_w;
    *dh = (int) scaled_h;
}

static void
render_detect_region(int ofstx, int ofsty, int texw, int texh,
                     face_detect_result_t *detection) {
    float col_red[] = {1.0f, 0.0f, 0.0f, 1.0f};

    for (int i = 0; i < detection->num; i++) {
        face_t *face = &(detection->faces[i]);
        float x1 = face->topleft.x * texw + ofstx;
        float y1 = face->topleft.y * texh + ofsty;
        float x2 = face->btmright.x * texw + ofstx;
        float y2 = face->btmright.y * texh + ofsty;

        /* rectangle region */
        draw_2d_rect(x1, y1, x2 - x1, y2 - y1, col_red, 2.0f);

#if 0
        float col_white[] = {1.0f, 1.0f, 1.0f, 1.0f};
        float score = face->score;

        /* detect score */
        char buf[512];
        sprintf (buf, "%d", (int)(score * 100));
        draw_dbgstr_ex (buf, x1, y1, 1.0f, col_white, col_red);

        /* key points */
        for (int j = 0; j < kFaceKeyNum; j ++)
        {
            float x = face->keys[j].x * texw + ofstx;
            float y = face->keys[j].y * texh + ofsty;

            int r = 4;
            draw_2d_fillrect (x - (r/2), y - (r/2), r, r, col_red);
        }
#endif
    }
}

static void
render_facemesh_keypoint(int ofstx, int ofsty, int texw, int texh, float *mat, fvec3 *joint,
                         int idx) {
    float col_cyan[] = {0.0f, 1.0f, 1.0f, 1.0f};

    float vec[2] = {joint[idx].x, joint[idx].y};
    matrix_multvec2(mat, vec, vec);

    float x = vec[0] * texw + ofstx;
    float y = vec[1] * texh + ofsty;

    int r = 4;
    draw_2d_fillrect(x - (r / 2), y - (r / 2), r, r, col_cyan);
}

static void
render_lines(int ofstx, int ofsty, int texw, int texh, float *mat, irismesh_result_t *irismesh,
             int *idx, int num) {
    float col_red[] = {1.0f, 0.0f, 0.0f, 1.0f};
    fvec3 *eye = irismesh->eye_landmark;

    for (int i = 1; i < num; i++) {
        float vec0[] = {eye[idx[i - 1]].x, eye[idx[i - 1]].y};
        float vec1[] = {eye[idx[i]].x, eye[idx[i]].y};
        matrix_multvec2(mat, vec0, vec0);
        matrix_multvec2(mat, vec1, vec1);
        float x0 = vec0[0] * texw + ofstx;
        float y0 = vec0[1] * texh + ofsty;
        float x1 = vec1[0] * texw + ofstx;
        float y1 = vec1[1] * texh + ofsty;

        draw_2d_line(x0, y0, x1, y1, col_red, 4.0f);
    }
}

static void
render_iris_landmark_on_main(int ofstx, int ofsty, int texw, int texh,
                             face_t *face, face_landmark_result_t *facemesh,
                             irismesh_result_t *irismesh) {
    float col_green[] = {0.0f, 1.0f, 0.0f, 1.0f};

    float mat_face[16];
    {
        float scale_x = face->face_w;
        float scale_y = face->face_h;
        float pivot_x = face->face_cx;
        float pivot_y = face->face_cy;
        float rotation = face->rotation;

        matrix_identity(mat_face);
        matrix_translate(mat_face, pivot_x, pivot_y, 0);
        matrix_rotate(mat_face, RAD_TO_DEG(rotation), 0, 0, 1);
        matrix_scale(mat_face, scale_x, scale_y, 1.0f);
        matrix_translate(mat_face, -0.5f, -0.5f, 0);
    }

    int key_idx[] = {1, 9, 10, 152, 78, 308, 234, 454};
    int key_num = sizeof(key_idx) / sizeof(int);
    for (int i = 0; i < key_num; i++)
        render_facemesh_keypoint(ofstx, ofsty, texw, texh, mat_face, facemesh->joint, key_idx[i]);

    for (int eye_id = 0; eye_id < 2; eye_id++) {
        fvec3 *iris = irismesh[eye_id].iris_landmark;

        float mat_eye[16];
        {
            eye_region_t *eye_rgn = &facemesh->eye_rgn[eye_id];
            float scale_x = eye_rgn->size.x;
            float scale_y = eye_rgn->size.y;
            float pivot_x = eye_rgn->center.x;
            float pivot_y = eye_rgn->center.y;
            float rotation = eye_rgn->rotation;

            matrix_identity(mat_eye);
            matrix_translate(mat_eye, pivot_x, pivot_y, 0);
            matrix_rotate(mat_eye, RAD_TO_DEG(rotation), 0, 0, 1);
            matrix_scale(mat_eye, scale_x, scale_y, 1.0f);
            matrix_translate(mat_eye, -0.5f, -0.5f, 0);
        }

        float mat[16];
        matrix_mult(mat, mat_face, mat_eye);

        int eye_idx0[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
        int idx_num0 = sizeof(eye_idx0) / sizeof(int);
        render_lines(ofstx, ofsty, texw, texh, mat, &irismesh[eye_id], eye_idx0, idx_num0);

        int eye_idx1[] = {0, 9, 10, 11, 12, 13, 14, 15, 8};
        int idx_num1 = sizeof(eye_idx1) / sizeof(int);
        render_lines(ofstx, ofsty, texw, texh, mat, &irismesh[eye_id], eye_idx1, idx_num1);

        if (0) {
            float col_red[] = {1.0f, 0.0f, 0.0f, 1.0f};
            fvec3 *eye = irismesh[eye_id].eye_landmark;
            for (int i = 0; i < 71; i++) {
                float vec[2] = {eye[i].x, eye[i].y};
                matrix_multvec2(mat, vec, vec);

                float x = vec[0] * texw + ofstx;;
                float y = vec[1] * texh + ofsty;;

                int r = 4;
                draw_2d_fillrect(x - (r / 2), y - (r / 2), r, r, col_red);
            }
        }

        /* iris circle */
        for (int i = 0; i < 5; i++) {
            float vec[2] = {iris[i].x, iris[i].y};
            matrix_multvec2(mat, vec, vec);

            float x = vec[0] * texw + ofstx;
            float y = vec[1] * texh + ofsty;

            int r = 4;
            draw_2d_fillrect(x - (r / 2), y - (r / 2), r, r, col_green);
        }

        {
            float vec0[2] = {iris[0].x, iris[0].y};
            float vec1[2] = {iris[1].x, iris[1].y};
            matrix_multvec2(mat, vec0, vec0);
            matrix_multvec2(mat, vec1, vec1);

            float x0 = vec0[0] * texw + ofstx;
            float y0 = vec0[1] * texh + ofsty;
            float x1 = vec1[0] * texw + ofstx;
            float y1 = vec1[1] * texh + ofsty;

            float len = sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
            draw_2d_circle(x0, y0, len, col_green, 4);
        }
    }
}

/* resize image to DNN network input size and convert to fp32. */
void
feed_face_detect_image(texture_2d_t *srctex, int win_w, int win_h) {
    int x, y, w, h;
    float *buf_fp32 = (float *) get_face_detect_input_buf(&w, &h);
    unsigned char *buf_ui8 = nullptr;
    static unsigned char *pui8 = nullptr;

    if (pui8 == nullptr)
        pui8 = (unsigned char *) malloc(w * h * 4);

    buf_ui8 = pui8;

    draw_2d_texture_ex(srctex, 0, win_h - h, w, h, RENDER2D_FLIP_V);

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf_ui8);

    /* convert UI8 [0, 255] ==> FP32 [-1, 1] */
    float mean = 128.0f;
    float std = 128.0f;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int r = *buf_ui8++;
            int g = *buf_ui8++;
            int b = *buf_ui8++;
            buf_ui8++;          /* skip alpha */
            *buf_fp32++ = (float) (r - mean) / std;
            *buf_fp32++ = (float) (g - mean) / std;
            *buf_fp32++ = (float) (b - mean) / std;
        }
    }

    return;
}

void
feed_face_landmark_image(texture_2d_t *srctex, int win_w, int win_h,
                         face_detect_result_t *detection, unsigned int face_id) {
    int x, y, w, h;
    float *buf_fp32 = (float *) get_facemesh_landmark_input_buf(&w, &h);
    unsigned char *buf_ui8 = nullptr;
    static unsigned char *pui8 = nullptr;

    if (pui8 == nullptr)
        pui8 = (unsigned char *) malloc(w * h * 4);

    buf_ui8 = pui8;

    float texcoord[] = {0.0f, 1.0f,
                        0.0f, 0.0f,
                        1.0f, 1.0f,
                        1.0f, 0.0f};

    if (detection->num > face_id) {
        face_t *face = &(detection->faces[face_id]);
        float x0 = face->face_pos[0].x;
        float y0 = face->face_pos[0].y;
        float x1 = face->face_pos[1].x; //    0--------1
        float y1 = face->face_pos[1].y; //    |        |
        float x2 = face->face_pos[2].x; //    |        |
        float y2 = face->face_pos[2].y; //    3--------2
        float x3 = face->face_pos[3].x;
        float y3 = face->face_pos[3].y;
        texcoord[0] = x3;
        texcoord[1] = y3;
        texcoord[2] = x0;
        texcoord[3] = y0;
        texcoord[4] = x2;
        texcoord[5] = y2;
        texcoord[6] = x1;
        texcoord[7] = y1;
    }

    draw_2d_texture_ex_texcoord(srctex, 0, win_h - h, w, h, texcoord);

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf_ui8);

    /* convert UI8 [0, 255] ==> FP32 [0, 1] */
    float mean = 0.0f;
    float std = 255.0f;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int r = *buf_ui8++;
            int g = *buf_ui8++;
            int b = *buf_ui8++;
            buf_ui8++;          /* skip alpha */
            *buf_fp32++ = (float) (r - mean) / std;
            *buf_fp32++ = (float) (g - mean) / std;
            *buf_fp32++ = (float) (b - mean) / std;
        }
    }

    return;
}


void
feed_iris_landmark_image(texture_2d_t *srctex, int win_w, int win_h,
                         face_t *face, face_landmark_result_t *facemesh, int eye_id) {
    int x, y, w, h;
    float *buf_fp32 = (float *) get_irismesh_landmark_input_buf(&w, &h);
    unsigned char *buf_ui8 = nullptr;
    static unsigned char *pui8 = nullptr;

    if (pui8 == nullptr)
        pui8 = (unsigned char *) malloc(w * h * 4);

    buf_ui8 = pui8;

    float texcoord[8];

    float scale_x = face->face_w;
    float scale_y = face->face_h;
    float pivot_x = face->face_cx;
    float pivot_y = face->face_cy;
    float rotation = face->rotation;

    float x0 = facemesh->eye_pos[eye_id][0].x;
    float y0 = facemesh->eye_pos[eye_id][0].y;
    float x1 = facemesh->eye_pos[eye_id][1].x; //    0--------1
    float y1 = facemesh->eye_pos[eye_id][1].y; //    |        |
    float x2 = facemesh->eye_pos[eye_id][2].x; //    |        |
    float y2 = facemesh->eye_pos[eye_id][2].y; //    3--------2
    float x3 = facemesh->eye_pos[eye_id][3].x;
    float y3 = facemesh->eye_pos[eye_id][3].y;

    float mat[16];
    float vec[4][2] = {{x0, y0},
                       {x1, y1},
                       {x2, y2},
                       {x3, y3}};
    matrix_identity(mat);

    matrix_translate(mat, pivot_x, pivot_y, 0);
    matrix_rotate(mat, RAD_TO_DEG(rotation), 0, 0, 1);
    matrix_scale(mat, scale_x, scale_y, 1.0f);
    matrix_translate(mat, -0.5f, -0.5f, 0);

    matrix_multvec2(mat, vec[0], vec[0]);
    matrix_multvec2(mat, vec[1], vec[1]);
    matrix_multvec2(mat, vec[2], vec[2]);
    matrix_multvec2(mat, vec[3], vec[3]);

    x0 = vec[0][0];
    y0 = vec[0][1];
    x1 = vec[1][0];
    y1 = vec[1][1];
    x2 = vec[2][0];
    y2 = vec[2][1];
    x3 = vec[3][0];
    y3 = vec[3][1];

    /* Upside down */
    if (eye_id == 0) {
        texcoord[0] = x3;
        texcoord[1] = y3;
        texcoord[2] = x0;
        texcoord[3] = y0;
        texcoord[4] = x2;
        texcoord[5] = y2;
        texcoord[6] = x1;
        texcoord[7] = y1;
    } else /* need to horizontal flip for right eye */
    {
        texcoord[0] = x2;
        texcoord[1] = y2;
        texcoord[2] = x1;
        texcoord[3] = y1;
        texcoord[4] = x3;
        texcoord[5] = y3;
        texcoord[6] = x0;
        texcoord[7] = y0;
    }

    draw_2d_texture_ex_texcoord(srctex, 0, win_h - h, w, h, texcoord);

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf_ui8);

    /* convert UI8 [0, 255] ==> FP32 [-1, 1] */
    float mean = 0.0f;
    float std = 255.0f;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int r = *buf_ui8++;
            int g = *buf_ui8++;
            int b = *buf_ui8++;
            buf_ui8++;          /* skip alpha */
            *buf_fp32++ = (float) (r - mean) / std;
            *buf_fp32++ = (float) (g - mean) / std;
            *buf_fp32++ = (float) (b - mean) / std;
        }
    }

    return;
}

static void
flip_horizontal_iris_landmark(irismesh_result_t *irismesh) {
    fvec3 *eye = irismesh->eye_landmark;
    fvec3 *iris = irismesh->iris_landmark;

    for (int i = 0; i < 71; i++) {
        eye[i].x = 1.0f - eye[i].x;
    }

    for (int i = 0; i < 5; i++) {
        iris[i].x = 1.0f - iris[i].x;
    }

}

void NativeOpengl::onDraw() {


    if (glctx.initdone == 0)
        return;
    updateTexture();
    //crop camera texture  image
    //todo  get sensor orientation to confirm the image rotates
    {
        texture_2d_t srctex = glctx.tex_camera;
        if (!glctx.tex_camera_valid)
            srctex = glctx.tex_static;

        /* render to square FBO */
        render_target_t *rtarget = &glctx.rtarget_crop;
        set_render_target(rtarget);
        set_2d_projection_matrix(rtarget->width, rtarget->height);
        glClear(GL_COLOR_BUFFER_BIT);

        int draw_x, draw_y, draw_w, draw_h;
        adjust_texture(rtarget->width, rtarget->height, srctex.width, srctex.height,
                       &draw_x, &draw_y, &draw_w, &draw_h, 1);

        /* when we use inner camera, enable horizontal flip. */
        int flip = m_camera_facing ? RENDER2D_FLIP_H : 0;
        flip |= RENDER2D_FLIP_V;
        draw_2d_texture_ex_rot(&srctex, draw_x, draw_y, draw_w, draw_h, flip, 0.5, 0.5, -90);
        /* reset to the default framebuffer */
        rtarget = &glctx.rtarget_main;
        set_render_target(rtarget);
        set_2d_projection_matrix(rtarget->width, rtarget->height);
    }


    texture_2d_t srctex = glctx.tex_input;
    int win_w = glctx.disp_w;
    int win_h = glctx.disp_h;

    int draw_x, draw_y, draw_w, draw_h;
    int texw = srctex.width;
    int texh = srctex.height;
    adjust_texture(win_w, win_h, texw, texh, &draw_x, &draw_y, &draw_w, &draw_h, 0);

    glClearColor(1.f, 1.f, 1.f, 1.0f);
    int count = glctx.frame_count;
    {
        face_detect_result_t face_detect_ret = {0};
        face_landmark_result_t face_mesh_ret[MAX_FACE_NUM] = {0};
        irismesh_result_t iris_mesh_ret[MAX_FACE_NUM][2] = {0};

        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, win_w, win_h);
        /* --------------------------------------- *
         *  face detection
         * --------------------------------------- */
        feed_face_detect_image(&srctex, win_w, win_h);
        invoke_face_detect(&face_detect_ret);

        /* --------------------------------------- *
         *  face landmark
         * --------------------------------------- */
        for (int face_id = 0; face_id < face_detect_ret.num; face_id++) {
            feed_face_landmark_image(&srctex, win_w, win_h, &face_detect_ret, face_id);
            invoke_facemesh_landmark(&face_mesh_ret[face_id]);
        }
        /* --------------------------------------- *
         *  Iris landmark
         * --------------------------------------- */
        for (int face_id = 0; face_id < face_detect_ret.num; face_id++) {
            for (int eye_id = 0; eye_id < 2; eye_id++) {
                feed_iris_landmark_image(&srctex, win_w, win_h, &face_detect_ret.faces[face_id],
                                         &face_mesh_ret[face_id], eye_id);
                invoke_irismesh_landmark(&iris_mesh_ret[face_id][eye_id]);
            }
            /* need to horizontal flip for right eye */
            flip_horizontal_iris_landmark(&iris_mesh_ret[face_id][1]);
        }

        /*
         * visualize the face pose estimation results.
         * */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        /* visualize the face pose estimation results. */
        draw_2d_texture_ex(&srctex, draw_x, draw_y, draw_w, draw_h, 0);
        render_detect_region(draw_x, draw_y, draw_w, draw_h, &face_detect_ret);
        for (int face_id = 0; face_id < face_detect_ret.num; face_id++) {
            render_iris_landmark_on_main(draw_x, draw_y, draw_w, draw_h,
                                         &face_detect_ret.faces[face_id],
                                         &face_mesh_ret[face_id], iris_mesh_ret[face_id]);
        }

    }
    glctx.frame_count++;

}

void NativeOpengl::onDestroy() {
    deleteCamera();
    egl_terminate();
}

void NativeOpengl::setAssetManager(AAssetManager *assetMgr) {
    aAssetManager = assetMgr;
}


void NativeOpengl::initCamera() {
    //todo verify android camera permission
    m_cameraGranted = true;
    if (!m_cameraGranted) {
        requestCameraPermission();
        return;
    }
    m_camera_facing = 1;
    createCamera(m_camera_facing);

}

void NativeOpengl::onCameraPermission(jboolean granted) {
    m_cameraGranted = (granted != JNI_FALSE);
    if (m_cameraGranted) {
        initCamera();
    }
}


void NativeOpengl::requestCameraPermission() {
    //todo
//    main_object->A
//    JNIEnv *env;
//    javaVm->GetEnv((void **) &env, JNI_VERSION_1_6);
//    javaVm->AttachCurrentThread(&env, NULL);
//    jclass clz = env->GetObjectClass(*main_object);
//    env->CallVoidMethod(*main_object, env->GetMethodID(clz, "RequestCamera", "()V"));
//    javaVm->DetachCurrentThread();
    m_cameraGranted = true;
}

void NativeOpengl::createCamera(int facing) {
    m_camera = new NDKCamera();
            ASSERT(m_camera, "Failed to Create CameraObject");

    m_camera->SelectCameraFacing(facing);

    m_ImgReader.InitImageReader(CAMERA_RESOLUTION_W, CAMERA_RESOLUTION_H);
    ANativeWindow *nativeWindow = m_ImgReader.GetNativeWindow();

    m_camera->CreateSession(nativeWindow);
    m_camera->StartPreview(true);
}

void NativeOpengl::updateTexture() {
    /* Acquire the latest AHardwareBuffer */
    AHardwareBuffer *ahw_buf = NULL;
    int ret = m_ImgReader.GetCurrentHWBuffer(&ahw_buf);
    if (ret != 0)
        return;

    /* Get EGLClientBuffer */
    EGLClientBuffer egl_buf = eglGetNativeClientBufferANDROID(ahw_buf);
    if (!egl_buf) {
        DBG_LOGE("Failed to create EGLClientBuffer");
        return;
    }

    /* (Re)Create EGLImage */
    if (glctx.egl_img != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(egl_get_display(), glctx.egl_img);
        glctx.egl_img = EGL_NO_IMAGE_KHR;
    }

    EGLint attrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE,};
    glctx.egl_img = eglCreateImageKHR(egl_get_display(), EGL_NO_CONTEXT,
                                      EGL_NATIVE_BUFFER_ANDROID, egl_buf, attrs);

    /* Bind to GL_TEXTURE_EXTERNAL_OES */
    texture_2d_t *input_tex = &glctx.tex_camera;
    if (input_tex->texid == 0) {
        GLuint texid;
        glGenTextures(1, &texid);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, texid);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        input_tex->texid = texid;
        input_tex->format = pixfmt_fourcc('E', 'X', 'T', 'X');
        m_ImgReader.GetBufferDimension(&input_tex->width, &input_tex->height);
    }
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, input_tex->texid);

    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, glctx.egl_img);
    GLASSERT();

    glctx.tex_camera_valid = true;
}

void NativeOpengl::deleteCamera(void) {
    if (m_camera) {
        delete m_camera;
        m_camera = nullptr;
    }
    m_ImgReader.ReleaseImageReader();
    glctx.tex_camera_valid = false;
}

void NativeOpengl::initTF() {
    asset_read_file(aAssetManager,
                    (char *) FACE_DETECT_MODEL_PATH, m_facedet_tflite_model_buf);

    asset_read_file(aAssetManager,
                    (char *) FACE_LANDMARK_MODEL_PATH, m_facelandmark_tflite_model_buf);

    asset_read_file(aAssetManager,
                    (char *) IRIS_LANDMARK_MODEL_PATH, m_irislandmark_tflite_model_buf);

    init_tflite_facemesh(
            (const char *) m_facedet_tflite_model_buf.data(), m_facedet_tflite_model_buf.size(),
            (const char *) m_facelandmark_tflite_model_buf.data(),
            m_facelandmark_tflite_model_buf.size(),
            (const char *) m_irislandmark_tflite_model_buf.data(),
            m_irislandmark_tflite_model_buf.size());
}







