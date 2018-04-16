// Created by cfans on 10/23/16.
//

#include <jni.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "FFPlayerContext.h"


static jfieldID gJNIContext;

static int initPlayer(FFPlayerContext *context, const char *file);

static void deinitPlayer(FFPlayerContext *context);

static void * playerThread(void * arg);

int registerPlayer(JNIEnv *env) {
    jclass clazz;
    if ((clazz = (*env)->FindClass(env, "cfans/ffmpeg/player/CFPlayer")) == NULL
        || (gJNIContext = (*env)->GetFieldID(env, clazz, "mContext", "J")) == NULL) {
        return JNI_ERR;
    }
    return JNI_OK;
}

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6) != JNI_OK
        | registerPlayer(env) < JNI_OK) {
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}


JNIEXPORT void JNICALL
Java_cfans_ffmpeg_player_CFPlayer_init(JNIEnv *env, jobject obj, jobject surface) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context == NULL) {
        context = (FFPlayerContext *) calloc(1, sizeof(FFPlayerContext));
        context->_nativeWindow = ANativeWindow_fromSurface(env, surface);
        (*env)->SetLongField(env, obj, gJNIContext, (jlong) context);
        pthread_mutex_init(&context->_mutex, NULL);
        pthread_cond_init(&context->_cond,NULL);
    }
    LOGE(" init  %p",context);
}


JNIEXPORT jint JNICALL Java_cfans_ffmpeg_player_CFPlayer_start(JNIEnv *env, jobject obj,jstring path) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    int ret = JNI_FALSE;
    if (context && !context->_isStarted) {
        const char *file = (*env)->GetStringUTFChars(env, path, 0);
        ret = initPlayer(context, file);
        (*env)->ReleaseStringUTFChars(env, path, file);
        if(ret >= 0){
            context->_isStarted = JNI_TRUE;
            context->_isPause = JNI_FALSE;
            pthread_create(&context->_playThread, NULL, playerThread, context);
        }else{
            deinitPlayer(context);
        }
    }

    return ret;
}

JNIEXPORT void JNICALL Java_cfans_ffmpeg_player_CFPlayer_play(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context && context->_isPause) {
        context->_isPause = JNI_FALSE;
        pthread_cond_signal(&context->_cond);
        LOGE(" play  %d",context->_isPause);
    }
}

JNIEXPORT void JNICALL Java_cfans_ffmpeg_player_CFPlayer_pause(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context && !context->_isPause) {
        context->_isPause = JNI_TRUE;
        pthread_cond_signal(&context->_cond);
        LOGE(" pause  %d",context->_isPause);
    }
}

JNIEXPORT void JNICALL Java_cfans_ffmpeg_player_CFPlayer_stop(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context) {
        context->_isStarted = JNI_FALSE;
        usleep(100);
        LOGE(" stop ");
    }
}

JNIEXPORT void JNICALL Java_cfans_ffmpeg_player_CFPlayer_destroy(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context) {
        pthread_mutex_destroy(&context->_mutex);
        pthread_cond_destroy(&context->_codecCtx);
        (*env)->SetLongField(env, obj, gJNIContext, 0);
        ANativeWindow_release(context->_nativeWindow);
        context->_nativeWindow = NULL;
        free(context);
        LOGE(" destroy ");
    }
}

JNIEXPORT jboolean JNICALL Java_cfans_ffmpeg_player_CFPlayer_isPlaying(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context) {
        return !context->_isPause;
    }
    return JNI_TRUE;
}


JNIEXPORT void JNICALL
Java_cfans_ffmpeg_player_CFPlayer_seekTo(JNIEnv *env, jobject obj, jint second) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context) {
        av_seek_frame(context->_formatCtx, context->_nb_streams, second/av_q2d(context->_formatCtx->streams[context->_nb_streams]->time_base), AVSEEK_FLAG_BACKWARD);
    }
}

JNIEXPORT jint JNICALL Java_cfans_ffmpeg_player_CFPlayer_getDuration(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context) {
        return context->_duration;
    }
    return JNI_FALSE;
}

static void * playerThread(void * arg){

    FFPlayerContext *context = arg;
    AVFormatContext * pFormatCtx = context->_formatCtx;
    AVCodecContext *pCodecCtx = context->_codecCtx;
    int width = pCodecCtx->width;
    int height = pCodecCtx->height;
    LOGE(" playerThread begin %p ,[%dX%d]", context,width,height);

    ANativeWindow_setBuffersGeometry(context->_nativeWindow, width, height,
                                     WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer windowBuffer;
    AVFrame *pFrame = av_frame_alloc();
    AVFrame *pFrameRGBA = av_frame_alloc();
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height,1);
    uint8_t *buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(pFrameRGBA->data, pFrameRGBA->linesize, buffer, AV_PIX_FMT_RGBA, width, height, 1);
    struct SwsContext *sws_ctx = sws_getContext(width,
                                                height,
                                                pCodecCtx->pix_fmt,
                                                width,
                                                height,
                                                AV_PIX_FMT_RGBA,
                                                SWS_BILINEAR,
                                                NULL,
                                                NULL,
                                                NULL);
    int frameFinished;
    AVPacket packet;
    pthread_mutex_lock(&context->_mutex);

    while (context->_isStarted){

        if (context->_isPause){
            pthread_cond_wait(&context->_cond,&context->_mutex);
        }else{
            if (av_read_frame(pFormatCtx, &packet) >= 0){
                if (packet.stream_index == context->_nb_streams) {
                    avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
                    LOGE(" avcodec_decode_video2  %d",frameFinished);
                    if (frameFinished) {
                        ANativeWindow_lock(context->_nativeWindow, &windowBuffer, 0);
                        sws_scale(sws_ctx, (uint8_t const *const *) pFrame->data,
                                  pFrame->linesize, 0, pCodecCtx->height,
                                  pFrameRGBA->data, pFrameRGBA->linesize);

                        uint8_t *dst = windowBuffer.bits;
                        int dstStride = windowBuffer.stride * 4;
                        uint8_t *src = (pFrameRGBA->data[0]);
                        int srcStride = pFrameRGBA->linesize[0];

                        int h;
                        for (h = 0; h < height; h++) {
                            memcpy(dst + h * dstStride, src + h * srcStride, srcStride);
                        }
                        ANativeWindow_unlockAndPost(context->_nativeWindow);
                    }
                }
                av_packet_unref(&packet);
            }else{
                av_seek_frame(context->_formatCtx, context->_nb_streams, 0, AVSEEK_FLAG_BACKWARD);
                context->_isPause = JNI_TRUE;
            }
        }
    }
    pthread_mutex_unlock(&context->_mutex);
    av_free(buffer);
    av_free(pFrameRGBA);
    av_free(pFrame);

    deinitPlayer(context);
    LOGE("playerThread end ");
}

static int initPlayer(FFPlayerContext *context, const char *file) {
    int ret = JNI_TRUE;
    av_register_all();
    AVFormatContext *formatCtx = avformat_alloc_context();
    context->_formatCtx = formatCtx;

    if ((ret = avformat_open_input(&formatCtx, file, NULL, NULL)) != 0) {
        LOGE("Couldn't open file:%s ret=%d\n", file, ret);
        deinitPlayer(context);
        return ret;
    }

    if ((ret = avformat_find_stream_info(formatCtx, NULL)) < 0) {
        LOGE("Couldn't find stream information. ret=%d\n", ret);
        deinitPlayer(context);
        return ret;
    }

    for (int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            context->_nb_streams = i;
            break;
        }
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3(NULL);
    context->_codecCtx = codecCtx;
    if ((ret = avcodec_parameters_to_context(codecCtx,formatCtx->streams[context->_nb_streams]->codecpar)) <
        0) {
        LOGE("Couldn't find stream information. ret=%d\n", ret);
        deinitPlayer(context);
        return ret;
    }

    av_codec_set_pkt_timebase(codecCtx, formatCtx->streams[context->_nb_streams]->time_base);
    context->_avCodec = avcodec_find_decoder(codecCtx->codec_id);
    if (context->_avCodec == NULL) {
        LOGE("avcodec_find_decoder error. \n");
        deinitPlayer(context);
        return -1;
    }

    if ((ret = avcodec_open2(codecCtx, context->_avCodec, NULL)) < 0) {
        LOGE("avcodec_open2 error= %d \n",ret);
        deinitPlayer(context);
    }

    context->_duration = (unsigned int) (formatCtx->duration + 5000) / AV_TIME_BASE;
    return ret;
}

static void deinitPlayer(FFPlayerContext *context) {
    if (context){
        if (context->_codecCtx){
            avcodec_close(context->_codecCtx);
            avcodec_free_context(&context->_codecCtx);
            context->_codecCtx = NULL;
        }

        if (context->_formatCtx){
            avformat_close_input(&context->_formatCtx);
        }
    }

}