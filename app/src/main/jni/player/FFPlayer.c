// Created by cfans on 10/23/16.
//

#include <jni.h>
#include <stdio.h>
#include <android/log.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "FFPlayerContext.h"


static jfieldID gJNIContext;

static int initPlayer(FFPlayerContext *context,const char * file);
static void deinitPlayer(FFPlayerContext *context);

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
    if ((*vm)->GetEnv(vm,(void **)&env,JNI_VERSION_1_6) != JNI_OK
        | registerPlayer(env) < JNI_OK)
    {
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}


JNIEXPORT jint JNICALL Java_cfans_ffmpeg_player_CFPlayer_playTest(JNIEnv * env, jobject clazz, jobject surface,jstring path){
    LOGE("play");

    // sd卡中的视频文件地址,可自行修改或者通过jni传入
    const char *file_name = (*env)->GetStringUTFChars(env, path, 0);

  //  (*env)->ReleaseStringUTFChars(env, path, file);

    av_register_all();

    AVFormatContext * pFormatCtx = avformat_alloc_context();

    // Open video file
    if(avformat_open_input(&pFormatCtx, file_name, NULL, NULL)!=0) {

        LOGE("Couldn't open file:%s\n", file_name);
        return -1; // Couldn't open file
    }

    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0) {
        LOGE("Couldn't find stream information.");
        return -1;
    }

    // Find the first video stream
    int videoStream = -1, i;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
           && videoStream < 0) {
            videoStream = i;
        }
    }
    if(videoStream==-1) {
        LOGE("Didn't find a video stream.");
        return -1; // Didn't find a video stream
    }

    // Get a pointer to the codec context for the video stream
//    AVCodecContext  * pCodecCtx = pFormatCtx->streams[videoStream]->codec;

    AVCodecContext * pCodecCtx = avcodec_alloc_context3(NULL);
    if((avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar)) < 0){
        LOGE("Couldn't find stream information.\n");
        return  -1;
    }
    // Find the decoder for the video stream
    AVCodec * pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec==NULL) {
        LOGE("Codec not found.");
        return -1; // Codec not found
    }

    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGE("Could not open codec.");
        return -1; // Could not open codec
    }

    // 获取native window
    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);

    // 获取视频宽高
    int videoWidth = pCodecCtx->width;
    int videoHeight = pCodecCtx->height;

    // 设置native window的buffer大小,可自动拉伸
    ANativeWindow_setBuffersGeometry(nativeWindow,  videoWidth, videoHeight, WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer windowBuffer;


    // Allocate video frame
    AVFrame * pFrame = av_frame_alloc();

    // 用于渲染
    AVFrame * pFrameRGBA = av_frame_alloc();
    if(pFrameRGBA == NULL || pFrame == NULL) {
        LOGE("Could not allocate video frame.");
        return -1;
    }

    // Determine required buffer size and allocate buffer
    int numBytes=av_image_get_buffer_size(AV_PIX_FMT_RGBA, pCodecCtx->width, pCodecCtx->height, 1);
    uint8_t * buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
    av_image_fill_arrays(pFrameRGBA->data, pFrameRGBA->linesize, buffer, AV_PIX_FMT_RGBA,
                         pCodecCtx->width, pCodecCtx->height, 1);

    // 由于解码出来的帧格式不是RGBA的,在渲染之前需要进行格式转换
    struct SwsContext *sws_ctx = sws_getContext(pCodecCtx->width,
                             pCodecCtx->height,
                             pCodecCtx->pix_fmt,
                             pCodecCtx->width,
                             pCodecCtx->height,
                             AV_PIX_FMT_RGBA,
                             SWS_BILINEAR,
                             NULL,
                             NULL,
                             NULL);

    int frameFinished;
    AVPacket packet;
    while(av_read_frame(pFormatCtx, &packet)>=0) {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream) {

            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            // 并不是decode一次就可解码出一帧
            if (frameFinished) {

                // lock native window buffer
                ANativeWindow_lock(nativeWindow, &windowBuffer, 0);

                // 格式转换
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                          pFrame->linesize, 0, pCodecCtx->height,
                          pFrameRGBA->data, pFrameRGBA->linesize);

                // 获取stride
                uint8_t * dst = windowBuffer.bits;
                int dstStride = windowBuffer.stride * 4;
                uint8_t * src = (uint8_t*) (pFrameRGBA->data[0]);
                int srcStride = pFrameRGBA->linesize[0];

                // 由于window的stride和帧的stride不同,因此需要逐行复制
                int h;
                for (h = 0; h < videoHeight; h++) {
                    memcpy(dst + h * dstStride, src + h * srcStride, srcStride);
                }

                ANativeWindow_unlockAndPost(nativeWindow);

            }

        }
        av_packet_unref(&packet);
    }

    av_free(buffer);
    av_free(pFrameRGBA);

    // Free the YUV frame
    av_free(pFrame);

    // Close the codecs
    avcodec_close(pCodecCtx);

    // Close the video file
    avformat_close_input(&pFormatCtx);
    return 0;
}

JNIEXPORT jint JNICALL Java_cfans_ffmpeg_player_CFPlayer_init(JNIEnv * env, jobject obj,jobject surface,jstring path) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj,gJNIContext);
    int ret = JNI_FALSE;
    if (context == NULL){
        context = (FFPlayerContext *) calloc(1, sizeof(FFPlayerContext));
        const char *file = (*env)->GetStringUTFChars(env, path, 0);
        ret = initPlayer(context,file);
        (*env)->ReleaseStringUTFChars(env, path, file);
        if(ret >0){
            context->_nativeWindow = ANativeWindow_fromSurface(env, surface);
            (*env)->SetLongField(env,obj,gJNIContext,(jlong)context);
        }else{
            deinitPlayer(context);
        }
    }
    return  ret;

}

JNIEXPORT void JNICALL Java_cfans_ffmpeg_player_CFPlayer_play(JNIEnv * env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj,gJNIContext);
    if (context){

    }
}

JNIEXPORT void JNICALL Java_cfans_ffmpeg_player_CFPlayer_pause(JNIEnv * env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj,gJNIContext);
    if (context){

    }
}

JNIEXPORT void JNICALL Java_cfans_ffmpeg_player_CFPlayer_stop(JNIEnv * env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj,gJNIContext);
    if (context){

    }

}
JNIEXPORT jboolean JNICALL Java_cfans_ffmpeg_player_CFPlayer_isPlaying(JNIEnv * env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj,gJNIContext);
    if (context){

    }

    return JNI_TRUE;
}


JNIEXPORT void JNICALL Java_cfans_ffmpeg_player_CFPlayer_seekTo(JNIEnv * env, jobject obj,jint position) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj,gJNIContext);
    if (context){
//        av_seek_frame(context->_formatCtx, gJNIContext, 100000*vid_time_scale/time_base, AVSEEK_FLAG_BACKWARD);
    }
}

JNIEXPORT jint JNICALL Java_cfans_ffmpeg_player_CFPlayer_getDuration(JNIEnv * env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj,gJNIContext);
    if (context){

    }
    return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_cfans_ffmpeg_player_CFPlayer_getCurrentPosition(JNIEnv * env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj,gJNIContext);
    if (context){

    }
    return JNI_TRUE;
}


static int initPlayer(FFPlayerContext *context,const char * file){
    int ret = JNI_TRUE;
    av_register_all();
    AVFormatContext * formatCtx = avformat_alloc_context();
    context->_formatCtx = formatCtx;

    if((ret = avformat_open_input(&formatCtx, file, NULL, NULL))!=0) {
        LOGE("Couldn't open file:%s\n", file);
        deinitPlayer(context);
        return ret;
    }

    if((ret=avformat_find_stream_info(formatCtx, NULL))<0) {
        LOGE("Couldn't find stream information.\n");
        deinitPlayer(context);
        return ret;
    }

    for (int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            context->nb_streams = i;
            break;
        }
    }

    AVCodecContext * codecCtx = avcodec_alloc_context3(NULL);
    context->_codecCtx =  codecCtx;
    if((ret = avcodec_parameters_to_context(codecCtx, formatCtx->streams[context->nb_streams]->codecpar)) < 0){
        LOGE("Couldn't find stream information.\n");
        deinitPlayer(context);
        return  ret;
    }

    av_codec_set_pkt_timebase(codecCtx, formatCtx->streams[context->nb_streams]->time_base);
    context->_avCodec = avcodec_find_decoder(codecCtx->codec_id);
    if(context->_avCodec==NULL) {
        LOGE("Codec not found.\n");
        deinitPlayer(context);
        return -1;
    }
    return ret;
}

static void deinitPlayer(FFPlayerContext *context){

}