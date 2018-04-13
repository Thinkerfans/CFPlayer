//
// Created by cfans on 2018/3/3.
//

#include <malloc.h>
#include <jni.h>
#include "GLViewContext.h"
#include "GLUtil.h"
#include "FFContext.h"

const char *vYUVShader =
        "attribute vec4 position;    \n"
                "attribute vec2 texCoordIn;    \n"
                "varying lowp vec2 texCoordOut;       \n"
                "void main() {                  \n"
                "   texCoordOut = texCoordIn;  \n"
                "   gl_Position = position;   \n"
                "}\n";

const char *fYUVShader =
        "varying lowp vec2 texCoordOut;                           \n"
                "uniform sampler2D SamplerY;                              \n"
                "uniform sampler2D SamplerU;                              \n"
                "uniform sampler2D SamplerV;                              \n"
                "void main()                                              \n"
                "{                                                        \n"
                "mediump vec3 yuv;                                \n"
                "lowp vec3 rgb;                                   \n"
                "yuv.x = texture2D(SamplerY, texCoordOut).r;      \n"
                "yuv.y = texture2D(SamplerU, texCoordOut).r - 0.5;\n"
                "yuv.z = texture2D(SamplerV, texCoordOut).r - 0.5;\n"
                "rgb = mat3( 1,       1,         1,0,       -0.39465,  2.03211,1.13983, -0.58060,  0) * yuv; \n"
                "gl_FragColor = vec4(rgb, 1); \n"
                "}                                                  \n";


static jfieldID gYUVContext;

static jboolean compileShaders(GLViewContext *context);//编译着色程序
static void setVideoSize(GLViewContext *context, int width, int height);//设置视频参数
static void setupYUVTexture(GLViewContext *context);

static void render(GLViewContext *context);//画视频数据
static void fillGLData(GLViewContext * context,const void * data,int width,int height);

JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLYUVView_glInit
        (JNIEnv *env, jobject object, jint width, jint height) {

    GLViewContext *context = (GLViewContext *) (*env)->GetLongField(env, object, gYUVContext);
    if (context == NULL) {
        context = (GLViewContext *) calloc(1, sizeof(GLViewContext));//分配一个空间，大小为UPUAVContext的大小
    }

    context->_viewWidth = width;
    context->_viewHeight = height;

    if (!compileShaders(context)) {
        free(context);
        return;
    }
    setupYUVTexture(context);
    (*env)->SetLongField(env, object, gYUVContext, (jlong) context);
    LOGE("glInit: view: [%d X %d]", context->_viewWidth, context->_viewHeight);
}

JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLYUVView_glDeinit
        (JNIEnv *env, jobject object) {
    GLViewContext *context = (GLViewContext *) (*env)->GetLongField(env, object, gYUVContext);
    if (context) {
        if (context->_program) {
            if (context->_texture[TEXY]) {
                glDeleteTextures(3, context->_texture);
            }
            if (context->_program) {
                glDeleteProgram(context->_program);
                context->_program = 0;
            }
        }
        (*env)->SetLongField(env, object, gYUVContext, 0);
        free(context);
    }
    LOGE("DE INIT");
}


JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLYUVView_glDualViewport
        (JNIEnv *env, jobject object, jboolean dual) {
    GLViewContext *context = (GLViewContext *) (*env)->GetLongField(env, object, gYUVContext);
    if (context) {
        context->_isDual = dual;
//        render(context);
    }
}

JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLYUVView_glDraw
        (JNIEnv *env, jobject object, jbyteArray array, jint w, jint h) {
    GLViewContext *context = (GLViewContext *) (*env)->GetLongField(env, object, gYUVContext);
    if (context && array) {
        jbyte *data = (*env)->GetByteArrayElements(env, array, 0);
        fillGLData(context,data,w,h);
        render(context);
        (*env)->ReleaseByteArrayElements(env, array, data, 0);
    }
}

JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLYUVView_glDraw2
        (JNIEnv *env, jobject object, jlong ffmpeg, jint w, jint h) {
    GLViewContext *context = (GLViewContext *) (*env)->GetLongField(env, object, gYUVContext);
    if (context) {
        FFDecoderContext *ctx = (FFDecoderContext *) ffmpeg;
        if (ctx->_outputBuf) {
            fillGLData(context,ctx->_outputBuf,w,h);
            render(context);
        }
    }
}

static void fillGLData(GLViewContext * context,const void * data,int w,int h){
    if (w != context->_frameWidth || h != context->_frameHeight) {
        setVideoSize(context, w, h);
    }
    glBindTexture(GL_TEXTURE_2D, context->_texture[TEXY]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);

    glBindTexture(GL_TEXTURE_2D, context->_texture[TEXU]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w / 2, h / 2, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, data + w * h);

    glBindTexture(GL_TEXTURE_2D, context->_texture[TEXV]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w / 2, h / 2, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, data + w * h * 5 / 4);
}


int registerGLYUV(JNIEnv *env) {
    jclass clazz;
    if ((clazz = (*env)->FindClass(env, "cfans/ufo/sdk/view/GLYUVView")) == NULL ||
        (gYUVContext = (*env)->GetFieldID(env, clazz, "mYUVContext", "J")) == NULL) {
        return JNI_ERR;
    }
    return JNI_OK;
}

static void setVideoSize(GLViewContext *context, int width, int height) {

    context->_frameWidth = width;
    context->_frameHeight = height;
    context->_offsetY = context->_viewWidth / 2 * height / width;
    context->_startY = (context->_viewHeight - context->_offsetY) / 2;

    LOGE("frame: [%d X %d] ,startY:%d, offset:%d ", context->_frameWidth, context->_frameHeight,
         context->_startY, context->_offsetY);
}

static void render(GLViewContext *context) {
    glClear(GL_COLOR_BUFFER_BIT);
    if (context->_isDual) {
        glViewport(0, context->_startY, context->_viewWidth / 2, context->_offsetY);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glViewport(context->_viewWidth / 2, context->_startY, context->_viewWidth / 2,
                   context->_offsetY);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    } else {
        glViewport(0, 0, context->_viewWidth, context->_viewHeight);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
    }
}

static jboolean compileShaders(GLViewContext *context) {

    GLuint vshader = compileShaderCode(vYUVShader, GL_VERTEX_SHADER);
    GLuint fshader = compileShaderCode(fYUVShader, GL_FRAGMENT_SHADER);

    context->_program = glCreateProgram();
    glAttachShader(context->_program, vshader);
    glAttachShader(context->_program, fshader);
    glBindAttribLocation(context->_program, ATTRIB_VERTEX, "position");
    glBindAttribLocation(context->_program, ATTRIB_TEXTURE, "texCoordIn");
    // Link the program
    glLinkProgram(context->_program);
    GLint linked;
    glGetProgramiv(context->_program, GL_LINK_STATUS, &linked);//检测链接是否成功
    if (!linked) {
        LOGE("glLinkProgram err = %d\n", linked);
        return JNI_FALSE;
    }
    glUseProgram(context->_program);
    glDeleteShader(vshader);
    glDeleteShader(fshader);
    return JNI_TRUE;
}

static void setupYUVTexture(GLViewContext *context) {
    if (context->_texture[TEXY]) {
        glDeleteTextures(3, context->_texture);
    }
    glGenTextures(3, context->_texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, context->_texture[TEXY]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, context->_texture[TEXU]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, context->_texture[TEXV]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    GLuint textureUniformY = glGetUniformLocation(context->_program, "SamplerY");
    GLuint textureUniformU = glGetUniformLocation(context->_program, "SamplerU");
    GLuint textureUniformV = glGetUniformLocation(context->_program, "SamplerV");

    glUniform1i(textureUniformY, TEXY);
    glUniform1i(textureUniformU, TEXU);
    glUniform1i(textureUniformV, TEXV);

    static const GLfloat squareVertices[] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f, 1.0f,
            1.0f, 1.0f,
    };

    static const GLfloat coordVertices[] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f,
    };
    // Update attribute values
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices);
    glEnableVertexAttribArray(ATTRIB_VERTEX);


    glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, coordVertices);
    glEnableVertexAttribArray(ATTRIB_TEXTURE);
}