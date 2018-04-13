//
// Created by cfans on 2018/3/3.
//
#include <jni.h>

extern int registerDecoder(JNIEnv *env);
extern int registerRecorder(JNIEnv *env);
extern int registerGLRGB(JNIEnv *env);
extern int registerGLYUV(JNIEnv *env);
extern int registerGLSuperRGB(JNIEnv *env);
extern int registerGLSuperYUV(JNIEnv *env);


__attribute__((visibility("default"))) jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    if ((*vm)->GetEnv(vm,(void **)&env,JNI_VERSION_1_6) != JNI_OK
            | registerGLRGB(env) < JNI_OK
            | registerGLYUV(env) < JNI_OK
            | registerGLSuperYUV(env) < JNI_OK
            | registerGLSuperRGB(env) < JNI_OK
            | registerDecoder(env) < JNI_OK
            | registerRecorder(env) < JNI_OK)
    {
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}
