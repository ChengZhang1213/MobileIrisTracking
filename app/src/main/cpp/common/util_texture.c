#include <stdio.h>
#include <stdlib.h>
#include <GLES2/gl2.h>
#include "util_texture.h"
#include "assertgl.h"


#if defined (USE_INPUT_CAMERA_CAPTURE)
#include "util_camera_capture.h"
#endif

#if defined (USE_INPUT_VIDEO_DECODE)
#include "util_video_decode.h"
#endif


GLuint
create_2d_texture (void *imgbuf, int width, int height)
{
    GLuint texid;

    glGenTextures (1, &texid );
    glBindTexture (GL_TEXTURE_2D, texid);

    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef WIN32
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#else
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif

    glPixelStorei (GL_UNPACK_ALIGNMENT, 4);

    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
        width, height, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, imgbuf);

    return texid;
}

int
create_2d_texture_ex (texture_2d_t *tex2d, void *imgbuf, int width, int height, uint32_t fmt)
{
    GLuint texid;

    glGenTextures (1, &texid);
    glBindTexture (GL_TEXTURE_2D, texid);

    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef WIN32
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#else
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif

    int glw   = width;
    int glh   = height;
    int glfmt = GL_RGBA;

    if (fmt == pixfmt_fourcc('Y', 'U', 'Y', 'V') ||
        fmt == pixfmt_fourcc('U', 'Y', 'V', 'Y'))
    {
        glPixelStorei (GL_UNPACK_ALIGNMENT, 2);
        glw /= 2;
    }
    else
    {
        glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
    }

    glTexImage2D (GL_TEXTURE_2D, 0, glfmt, glw, glh, 0, glfmt, GL_UNSIGNED_BYTE, imgbuf);

    tex2d->texid  = texid;
    tex2d->width  = width;
    tex2d->height = height;
    tex2d->format = fmt;
    return 0;
}


#if defined (USE_INPUT_CAMERA_CAPTURE)
int
create_capture_texture (texture_2d_t *captex)
{
    int      cap_w, cap_h;
    uint32_t cap_fmt;

    get_capture_dimension (&cap_w, &cap_h);
    get_capture_pixformat (&cap_fmt);

    create_2d_texture_ex (captex, NULL, cap_w, cap_h, cap_fmt);
    start_capture ();

    return 0;
}

void
update_capture_texture (texture_2d_t *captex)
{
    int      cap_w, cap_h;
    uint32_t cap_fmt;
    void     *cap_buf;

    get_capture_dimension (&cap_w, &cap_h);
    get_capture_pixformat (&cap_fmt);
    get_capture_buffer (&cap_buf);
    if (cap_buf)
    {
        int texw = cap_w;
        int texh = cap_h;
        int texfmt = GL_RGBA;
        switch (cap_fmt)
        {
        case pixfmt_fourcc('Y', 'U', 'Y', 'V'):
        case pixfmt_fourcc('U', 'Y', 'V', 'Y'):
            texw = cap_w / 2;
            break;
        default:
            break;
        }

        glBindTexture (GL_TEXTURE_2D, captex->texid);
        glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, texw, texh, texfmt, GL_UNSIGNED_BYTE, cap_buf);
    }
}
#endif


#if defined (USE_INPUT_VIDEO_DECODE)
int
create_video_texture (texture_2d_t *vidtex, const char *fname)
{
    int      vid_w, vid_h;
    uint32_t vid_fmt;

    open_video_file (fname);

    get_video_dimension (&vid_w, &vid_h);
    get_video_pixformat (&vid_fmt);

    create_2d_texture_ex (vidtex, NULL, vid_w, vid_h, vid_fmt);
    start_video_decode ();

    return 0;
}

void
update_video_texture (texture_2d_t *vidtex)
{
    int   video_w, video_h;
    uint32_t video_fmt;
    void *video_buf;

    get_video_dimension (&video_w, &video_h);
    get_video_pixformat (&video_fmt);
    get_video_buffer (&video_buf);

    if (video_buf)
    {
        int texw = video_w;
        int texh = video_h;
        int texfmt = GL_RGBA;
        switch (video_fmt)
        {
        case pixfmt_fourcc('Y', 'U', 'Y', 'V'):
        case pixfmt_fourcc('U', 'Y', 'V', 'Y'):
            texw = video_w / 2;
            break;
        default:
            break;
        }

        glBindTexture (GL_TEXTURE_2D, vidtex->texid);
        glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, texw, texh, texfmt, GL_UNSIGNED_BYTE, video_buf);
    }
}

#endif /* USE_INPUT_VIDEO_DECODE */
