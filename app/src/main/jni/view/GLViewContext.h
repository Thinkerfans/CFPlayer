//
// Created by cfans on 2018/3/3.
//

#ifndef AND_OPENGL_GLCONTEXT_H
#define AND_OPENGL_GLCONTEXT_H

#include <jni.h>
#include <GLES2/gl2.h>


enum AttribEnum
{
    ATTRIB_VERTEX = 0,
    ATTRIB_TEXTURE,
    UNIFORM_HUE,
    UNIFORM_SATURATION,
    UNIFORM_VALUE,
    UNIFORM_CONTRAST,
    NUM_UNIFORMS
};

enum TextureYUVType
{
    TEXY = 0,
    TEXU,
    TEXV,
    NUM_TEX,
};


typedef struct stGLViewContext {

    GLuint  _program;
    GLuint _texture[NUM_TEX];

    GLint _viewWidth;
    GLint _viewHeight;

    jboolean _isDual;
    int _offsetY;
    int _startY;

    GLint _frameWidth;
    GLint _frameHeight;

    GLint _glViewUniforms[NUM_UNIFORMS];

} GLViewContext;


#endif //AND_OPENGL_GLCONTEXT_H
