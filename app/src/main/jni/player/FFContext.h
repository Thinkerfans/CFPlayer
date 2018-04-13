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


typedef struct FFDecoderContext {

    struct SwsContext * _swsCtx;
    enum AVCodecID _codecID;

    AVCodecContext * _codecCtx;
    AVCodec * _codec;
    AVFrame * _frame;
    AVFrame * _picture;

    int _width;
    int _height;
    int _pixFmt;

    jboolean _isMJPEG;
    jboolean _gotIFrame;

    unsigned char * _outputBuf;
    int _outputBufSize;

} FFDecoderContext;

typedef struct FFRecoderContext {
    
    AVFormatContext * _formatCtx;
    AVStream * _videoStream;
    enum AVCodecID _codecID;
    int16_t _extradataSize;
    int64_t _nextVideoPts;
    jboolean _gotIFrame;
    jboolean _isMJPEG;
    int _width;
    int _height;

    AVStream * _audioStream;
    int64_t _nextAudioPts;

    jboolean _recording;
    jboolean _recordAudio;

    AVCodecContext * _videoCtx;
    AVCodecContext * _audioCtx;

} FFRecoderContext;

#endif /* FFContext_h */
