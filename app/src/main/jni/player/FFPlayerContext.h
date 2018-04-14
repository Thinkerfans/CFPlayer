//
//  FFContext.h
//  UFOSDK
//
//  Created by cfans on 2017/12/22.
//  Copyright © 2017年 cfans. All rights reserved.
//

#ifndef FFContext_h
#define FFContext_h

#include <jni.h>
#include <android/log.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#if 1
#define  LOG_TAG    "UFOSDK JNI :"
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#define LOGE(...)
#endif


typedef struct FFPlayerContext {
    AVFormatContext * _formatCtx;
    AVCodecContext *_codecCtx;
    AVCodec * _avCodec;
    unsigned int nb_streams;

    ANativeWindow * _nativeWindow;

} FFPlayerContext;

#endif /* FFContext_h */
