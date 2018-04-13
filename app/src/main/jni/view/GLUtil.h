//
// Created by cfans on 2018/3/3.
//

#ifndef AND_OPENGL_GLUTIL_H
#define AND_OPENGL_GLUTIL_H

#include <jni.h>
#include <GLES2/gl2.h>

#include <android/log.h>


#undef TAG
#define TAG "GLView-JNI"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR ,  TAG, __VA_ARGS__)

GLuint compileShaderCode(const char * shaderCode, GLenum shaderType);

#endif //AND_OPENGL_GLUTIL_H
