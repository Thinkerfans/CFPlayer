//
// Created by cfans on 2018/3/3.
//

#include <malloc.h>
#include <jni.h>
#include "GLViewContext.h"
#include "GLUtil.h"

const char *vShaderStrDisplay =
        "attribute vec4 in_position;    \n"
        "attribute vec2 in_texcoord;    \n"
        "varying vec2 v_texcoord;       \n"
        "void main() {                  \n"
        "   gl_Position = in_position;  \n"
        "   v_texcoord = in_texcoord;   \n"
        "}\n";

const char *fShaderStrDisplay =
        "precision mediump float;                            \n"
        "varying vec2 v_texcoord;                            \n"
        "uniform sampler2D sampler;                          \n"
        "void main()                                         \n"
        "{                                                   \n"
        "   gl_FragColor = texture2D( sampler, v_texcoord ); \n"
        "}                                                  \n";


static jfieldID gRGBContext;
static jboolean compileShaders(GLViewContext *context);//编译着色程序
static void setVideoSize(GLViewContext * context, int width, int height);//设置视频参数
static void setupRGBTexture(GLViewContext * context, int width, int height);
static void render(GLViewContext * context);//画视频数据
static void fillGLData(GLViewContext * context,const void * data,int width,int height);

JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLRGBView_glInit(JNIEnv *env, jobject object, jint width, jint height) {

    GLViewContext * context = (GLViewContext *) (*env)->GetLongField(env,object,gRGBContext);
    if(context == NULL){
        context = (GLViewContext *)calloc(1,sizeof(GLViewContext));//分配一个空间，大小为UPUAVContext的大小
    }

    context->_viewWidth = width;
    context->_viewHeight = height;

    if (!compileShaders(context)){
        free(context);
        return;
    }
    (*env)->SetLongField(env,object,gRGBContext,(jlong)context);
    LOGE("glInit: view: [%d X %d]",context->_viewWidth,context->_viewHeight);
}

JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLRGBView_glDeinit(JNIEnv *env, jobject object) {
    GLViewContext * context = (GLViewContext *) (*env)->GetLongField(env,object,gRGBContext);
    if (context){
        if (context->_program) {
            if (context->_texture[0]) {
                glDeleteTextures(1, &context->_texture[0]);
                context->_texture[0] = 0;
            }
            if (context->_program) {
                glDeleteProgram(context->_program);
                context->_program = 0;
            }
        }
        (*env)->SetLongField(env,object,gRGBContext,0);
        free(context);
    }
    LOGE("gl deinit");
}


JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLRGBView_glDualViewport(JNIEnv *env, jobject object,jboolean dual) {
    GLViewContext * context = (GLViewContext *) (*env)->GetLongField(env,object,gRGBContext);
    if(context){
        context->_isDual = dual;
//        render(context);
    }
}

JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLRGBView_glDraw
        (JNIEnv *env, jobject object, jbyteArray array, jint width, jint height) {
    GLViewContext * context = (GLViewContext *) (*env)->GetLongField(env,object,gRGBContext);
    if (context && array){
        jbyte *  data = (*env)->GetByteArrayElements(env,array, 0);
        fillGLData(context,data,width,height);
        render(context);
        (*env)->ReleaseByteArrayElements(env, array, data, 0);
    }
}

JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLRGBView_glDraw2(JNIEnv *env, jobject object, jlong pointer, jint width, jint height) {
    GLViewContext * context = (GLViewContext *) (*env)->GetLongField(env,object,gRGBContext);
    if (context) {
        if (pointer) {
            fillGLData(context,pointer,width,height);
            render(context);
        }
    }
}
static void fillGLData(GLViewContext * context,const void * data,int width,int height){
    if (width != context->_frameWidth || height != context->_frameHeight) {
        setVideoSize(context, width, height);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 data);
}


int registerGLRGB(JNIEnv * env){
    jclass clazz;
    if ((clazz = (*env)->FindClass(env,"cfans/ufo/sdk/view/GLRGBView")) == NULL ||
        (gRGBContext = (*env)->GetFieldID(env,clazz, "mRGBContext", "J")) == NULL) {
        return JNI_ERR;
    }
    return JNI_OK;
}

static void setVideoSize(GLViewContext * context, int width, int height){
    context->_frameWidth = width;
    context->_frameHeight = height;
    context->_offsetY = context->_viewWidth/2*height/width;
    context->_startY = (context->_viewHeight - context->_offsetY)/2;
    setupRGBTexture(context,width,height);
    LOGE("frame: [%d X %d] ,startY:%d, offset:%d ",context->_viewWidth,context->_viewHeight,context->_startY,context->_offsetY);
}

static void render(GLViewContext * context){
    static GLushort indices[] = {0, 1, 2, 0, 2, 3};
    glClear(GL_COLOR_BUFFER_BIT);
    if (context->_isDual) {
        glViewport(0, context->_startY, context->_viewWidth/2, context->_offsetY);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
        glViewport(context->_viewWidth/2, context->_startY, context->_viewWidth/2, context->_offsetY);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    }else{
        glViewport(0, 0, context->_viewWidth, context->_viewHeight);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    }
}

static jboolean compileShaders(GLViewContext *context){

    GLuint vshader = compileShaderCode(vShaderStrDisplay,GL_VERTEX_SHADER);
    GLuint fshader = compileShaderCode(fShaderStrDisplay,GL_FRAGMENT_SHADER);

    context->_program = glCreateProgram();
    glAttachShader(context->_program, vshader);
    glAttachShader(context->_program, fshader);
    glBindAttribLocation(context->_program, ATTRIB_VERTEX, "in_position");
    glBindAttribLocation(context->_program, ATTRIB_TEXTURE, "in_texcoord");
    // Link the program
    glLinkProgram(context->_program);
    GLint linked;
    glGetProgramiv(context->_program, GL_LINK_STATUS, &linked);//检测链接是否成功
    if (!linked) {
        LOGE("glLinkProgram err = %d\n"),linked;
        return JNI_FALSE;
    }
    glUseProgram(context->_program);
    glDeleteShader(vshader);
    glDeleteShader(fshader);
    return JNI_TRUE;
}

static void setupRGBTexture(GLViewContext * context, int w, int h){
    if(context->_texture[0]){
        glDeleteTextures(1, &context->_texture[0]);
    }

    glGenTextures(1, &context->_texture);
    glBindTexture(GL_TEXTURE_2D, context->_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);        //把纹理像素映射到帧缓存中
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    static GLfloat vVertices[] = {
            // 解决纹理这样贴反的问题
            -1.0f,  1.0f, 0.0f,  // Position 0
            0.0f,  0.0f,        // TexCoord 0

            -1.0f, -1.0f, 0.0f,  // Position 1
            0.0f,  1.0f,        // TexCoord 1

            1.0f, -1.0f, 0.0f,  // Position 2
            1.0f,  1.0f,   // TexCoord 2


            1.0f,  1.0f, 0.0f,  // Position 3
            1.0f,  0.0f,   // TexCoord 3

    };
    // Load the vertex position
    glVertexAttribPointer (ATTRIB_VERTEX, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vVertices);
    glEnableVertexAttribArray (ATTRIB_VERTEX);
    // Load the texture coordinate
    glVertexAttribPointer (ATTRIB_TEXTURE, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &vVertices[3]);
    glEnableVertexAttribArray (ATTRIB_TEXTURE);
}