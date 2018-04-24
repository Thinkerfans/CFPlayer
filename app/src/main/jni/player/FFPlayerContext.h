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

    unsigned int _nb_streams; //视频流编号
    unsigned int _framerate;// 帧率
    unsigned int _duration;// 视频文件总时长 second
    jboolean _isStarted;//播放线程是否开启
    jboolean _isPause;//是否暂停

    ANativeWindow * _nativeWindow; //视频窗口

    pthread_mutex_t _mutex;//互斥锁
    pthread_cond_t _cond;//条件变量
    pthread_t _playThread;//解码线程

} FFPlayerContext;

#endif /* FFContext_h */
