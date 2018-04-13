//
// Created by cfans on 2018/3/3.
//

#include <malloc.h>
#include <jni.h>
#include "GLViewContext.h"
#include "GLUtil.h"
#include "FFContext.h"

const char *vSuperYUVShader =
        "attribute vec4 position;    \n"
        "attribute vec2 texCoordIn;    \n"
        "varying lowp vec2 texCoordOut;       \n"
        "void main() {                  \n"
        "   gl_Position = position;  \n"
        "   texCoordOut = texCoordIn;   \n"
        "}\n";

const char *fSuperYUVShader =
        "precision mediump float;                        \n"
        "varying lowp vec2 texCoordOut;                  \n"

        "uniform sampler2D SamplerY;                     \n"
        "uniform sampler2D SamplerU;                     \n"
        "uniform sampler2D SamplerV;                     \n"

        "uniform float hue;\n"
        "uniform float saturation;\n"
        "uniform float value;\n"
        "uniform float contrast;\n"

        "vec3 rgbtohsv(vec3 rgb){\n"
            "float R = rgb.x;\n"
            "float G = rgb.y;\n"
            "float B = rgb.z;\n"
            "vec3 hsv;\n"
            "float max1 = max(R, max(G, B));\n"
            "float min1 = min(R, min(G, B));\n"
            "if (R == max1)\n"
            "{\n"
                 "hsv.x = (G - B) / (max1 - min1);\n"
            "}\n"
            "if (G == max1)\n"
            "{\n"
                 "hsv.x = 2.0 + (B - R) / (max1 - min1);\n"
            "}\n"
            "if (B == max1)\n"
            "{\n"
                " hsv.x = 4.0 + (R - G) / (max1 - min1);\n"
            "}\n"
            " hsv.x = hsv.x * 60.0;\n"
            "if (hsv.x  < 0.0)\n"
            "{\n"
                "hsv.x = hsv.x + 360.0;\n"
            "}\n"
            "hsv.z = max1;\n"
            " hsv.y = (max1 - min1) / max1;\n"
            " return hsv;\n"
        "}\n"
         "vec3 hsvtorgb(vec3 hsv){\n"
            "float R;\n"
            "float G;\n"
            "float B;\n"
            "if (hsv.y == 0.0)\n"
            "{\n"
                "R = G = B = hsv.z;\n"
            "}\n"
            "else\n"
            "{\n"
                " hsv.x = hsv.x / 60.0;\n"
                " int i = int(hsv.x);\n"
                "float f = hsv.x - float(i);\n"
                "float a = hsv.z * (1.0 - hsv.y);\n"
                "float b = hsv.z * (1.0 - hsv.y * f);\n"
                "float c = hsv.z * (1.0 - hsv.y * (1.0 - f));\n"
                "if (i == 0)\n"
                "{\n"
                    " R = hsv.z;\n"
                    "G = c;\n"
                    " B = a;\n"
                "}\n"
                "else if (i == 1)\n"
                "{\n"
                    " R = b;\n"
                    " G = hsv.z;\n"
                    "B = a;\n"
                "}\n"
                "else if (i == 2)\n"
                "{\n"
                    " R = a;\n"
                    "G = hsv.z;\n"
                    " B = c;\n"
                " }\n"
                "else if (i == 3)\n"
                "{\n"
                    "R = a;\n"
                    "G = b;\n"
                    "B = hsv.z;\n"
                "}\n"
                "else if (i == 4)\n"
                "{\n"
                    "R = c;\n"
                    " G = a;\n"
                    "B = hsv.z;\n"
                "}\n"
                " else\n"
                "{\n"
                    "R = hsv.z;\n"
                    "G = a;\n"
                    "B = b;\n"
                "}\n"
            "}\n"
            "return vec3(R, G, B);\n"
        "}\n"

        "void main(){                                         \n"
            "mediump vec3 yuv;                                \n"
            "lowp vec3 rgb;                                   \n"
            "yuv.x = texture2D(SamplerY, texCoordOut).r;      \n"
            "yuv.y = texture2D(SamplerU, texCoordOut).r - 0.5;\n"
            "yuv.z = texture2D(SamplerV, texCoordOut).r - 0.5;\n"
            "rgb = mat3( 1, 1, 1, 0, -0.39465, 2.03211, 1.13983, -0.58060, 0) * yuv; \n"
            "vec3 hsv;\n"
            "hsv.xyz = rgbtohsv(rgb);\n"
            "hsv.x += hue;\n"
            "hsv.x = mod(hsv.x, 360.0);\n"
            "hsv.y *= saturation;\n"
            "hsv.z *= value;\n"
            "vec3 f_color = hsvtorgb(hsv);\n"
            "f_color = ((f_color - 0.5) * max(contrast+1.0, 0.0)) + 0.5;\n"
            "gl_FragColor = vec4(f_color, 0.5);\n"
        "}\n";

static jfieldID gSuperYUVContext;
static jfieldID gHueID;
static jfieldID gSaturationID;
static jfieldID gValueID;
static jfieldID gContrastID;

static jboolean compileShaders(GLViewContext *context);//编译着色程序
static void setVideoSize(GLViewContext * context, int width, int height);//设置视频参数
static void setupYUVTexture(GLViewContext * context);
static void render(GLViewContext * context,JNIEnv  *env,jobject object);//画视频数据
static void fillGLData(GLViewContext * context,const void * data,int width,int height);

JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLSuperYUVView_glInit
        (JNIEnv *env, jobject object, jint width, jint height) {

    GLViewContext * context = (GLViewContext *) (*env)->GetLongField(env,object,gSuperYUVContext);
    if(context == NULL){
        context = (GLViewContext *)calloc(1,sizeof(GLViewContext));//分配一个空间，大小为UPUAVContext的大小
    }

    context->_viewWidth = width;
    context->_viewHeight = height;

    if (!compileShaders(context)){
        free(context);
        return;
    }
    setupYUVTexture(context);
    (*env)->SetLongField(env,object,gSuperYUVContext,(jlong)context);
    LOGE("glInit: view: [%d X %d]",context->_viewWidth,context->_viewHeight);
}

JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLSuperYUVView_glDeinit
        (JNIEnv *env, jobject object) {
    GLViewContext * context = (GLViewContext *) (*env)->GetLongField(env,object,gSuperYUVContext);
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
        (*env)->SetLongField(env,object,gSuperYUVContext,0);
        free(context);
    }
    LOGE("DE INIT");
}


JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLSuperYUVView_glDualViewport
        (JNIEnv *env, jobject object,jboolean dual) {
    GLViewContext * context = (GLViewContext *) (*env)->GetLongField(env,object,gSuperYUVContext);
    if(context){
        context->_isDual = dual;
    }
}

JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLSuperYUVView_glDraw
        (JNIEnv *env, jobject object, jbyteArray array, jint w, jint h) {
    GLViewContext * context = (GLViewContext *) (*env)->GetLongField(env,object,gSuperYUVContext);
    if (context && array){
        jbyte *  data = (*env)->GetByteArrayElements(env,array, 0);
        fillGLData(context,data,w,h);
        render(context,env,object);
        (*env)->ReleaseByteArrayElements(env, array, data, 0);
    }
}
JNIEXPORT void JNICALL Java_cfans_ufo_sdk_view_GLSuperYUVView_glDraw2
        (JNIEnv *env, jobject object, jlong ffmpeg, jint width, jint height) {
    GLViewContext * context = (GLViewContext *) (*env)->GetLongField(env,object,gSuperYUVContext);
    if (context) {
        FFDecoderContext *ctx = (FFDecoderContext *) ffmpeg;
        if (ctx->_outputBuf) {
            fillGLData(context,ctx->_outputBuf,width,height);
            render(context,env,object);
        }
    }
}
static void fillGLData(GLViewContext * context,const void * data,int w,int h){
    if (w != context->_frameWidth || h != context->_frameHeight)
    {
        setVideoSize(context,w,h);
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

int registerGLSuperYUV(JNIEnv * env){
    jclass clazz;
    if ((clazz = (*env)->FindClass(env,"cfans/ufo/sdk/view/GLSuperYUVView")) == NULL ||
        (gSuperYUVContext = (*env)->GetFieldID(env,clazz, "mSuperYUVContext", "J")) == NULL||
        (gHueID = (*env)->GetFieldID(env,clazz, "mHue", "F")) == NULL||
        (gSaturationID = (*env)->GetFieldID(env,clazz, "mSaturation", "F")) == NULL||
        (gValueID = (*env)->GetFieldID(env,clazz, "mValue", "F")) == NULL||
        (gContrastID = (*env)->GetFieldID(env,clazz, "mContrast", "F")) == NULL) {
        return JNI_ERR;
    }
    return JNI_OK;
}

static void setVideoSize(GLViewContext * context, int width, int height){

    context->_frameWidth = width;
    context->_frameHeight = height;
    context->_offsetY = context->_viewWidth / 2 * height / width;
    context->_startY = (context->_viewHeight - context->_offsetY) / 2;

    LOGE("frame: [%d X %d] ,startY:%d, offset:%d ", context->_frameWidth, context->_frameHeight,
         context->_startY, context->_offsetY);
}

static void render(GLViewContext * context,JNIEnv  *env,jobject object){

    glClear(GL_COLOR_BUFFER_BIT);

    float _hue =  (*env)->GetFloatField(env,object,gHueID);
    float _value =  (*env)->GetFloatField(env,object,gValueID);
    float _contrast =  (*env)->GetFloatField(env,object,gContrastID);
    float _saturation =  (*env)->GetFloatField(env,object,gSaturationID);

    glUniform1f(context->_glViewUniforms[UNIFORM_HUE], _hue);
    glUniform1f(context->_glViewUniforms[UNIFORM_VALUE], _value);
    glUniform1f(context->_glViewUniforms[UNIFORM_CONTRAST], _contrast);
    glUniform1f(context->_glViewUniforms[UNIFORM_SATURATION], _saturation);

//    LOGE("render: hue:%f,value:%f,contrast:%f, saturation:%f ",_hue,_value,_contrast,_saturation);

    if (context->_isDual) {
        glViewport(0, context->_startY, context->_viewWidth/2, context->_offsetY);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glViewport(context->_viewWidth/2, context->_startY, context->_viewWidth/2, context->_offsetY);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }else{
        glViewport(0, 0, context->_viewWidth, context->_viewHeight);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
}

static jboolean compileShaders(GLViewContext *context){

    GLuint vshader = compileShaderCode(vSuperYUVShader,GL_VERTEX_SHADER);
    GLuint fshader = compileShaderCode(fSuperYUVShader,GL_FRAGMENT_SHADER);

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
        LOGE("glLinkProgram err = %d\n",linked);
        return JNI_FALSE;
    }
    glUseProgram(context->_program);

    glDeleteShader(vshader);
    glDeleteShader(fshader);
    return JNI_TRUE;
}

static void setupYUVTexture(GLViewContext * context){
    if(context->_texture[TEXY]){
        glDeleteTextures(3, context->_texture);
    }

    glGenTextures(3, context->_texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, context->_texture[TEXY]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, context->_texture[TEXU]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, context->_texture[TEXV]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    GLuint textureUniformY = (GLuint)glGetUniformLocation(context->_program, "SamplerY");
    GLuint textureUniformU = (GLuint) glGetUniformLocation(context->_program, "SamplerU");
    GLuint textureUniformV = (GLuint)glGetUniformLocation(context->_program, "SamplerV");

    glUniform1i(textureUniformY, TEXY);
    glUniform1i(textureUniformU, TEXU);
    glUniform1i(textureUniformV, TEXV);
    
    context->_glViewUniforms[UNIFORM_SATURATION] = glGetUniformLocation(context->_program, "saturation");
    context->_glViewUniforms[UNIFORM_HUE] = glGetUniformLocation(context->_program, "hue");
    context->_glViewUniforms[UNIFORM_VALUE] = glGetUniformLocation(context->_program, "value");
    context->_glViewUniforms[UNIFORM_CONTRAST] = glGetUniformLocation(context->_program, "contrast");

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