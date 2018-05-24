// Created by cfans on 10/23/16.
//

#include <jni.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavcodec/jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/bitmap.h>

#include "FFPlayerContext.h"


static jfieldID gJNIContext;
static jmethodID gMethodID;

static void onProgressEvent(FFPlayerContext *context);

static int initPlayer(FFPlayerContext *context, const char *file);

static void deinitPlayer(FFPlayerContext *context);

static void *playerThread(void *arg);

/**
 * 截图，保存成BMP或者PPM格式
 * */
static void saveFrame(AVFrame* pFrame, int width, int height, const char * path);
static void fillBitmap(AndroidBitmapInfo*  info, void *pixels, AVFrame *pFrame);
static jobject createBitmap(JNIEnv *env,int width,int height);
static jboolean saveBitmap(JNIEnv *env,jobject bitmap,jstring path);


int registerPlayer(JNIEnv *env) {
    jclass clazz;
    if ((clazz = (*env)->FindClass(env, "cfans/ffmpeg/player/SilentPlayer")) == NULL
        || (gJNIContext = (*env)->GetFieldID(env, clazz, "mContext", "J")) == NULL) {
        return JNI_ERR;
    }
    gMethodID = (*env)->GetMethodID(env, clazz, "postProgressFromJNI", "(JJ)V");
    JavaVM *jvm;
    (*env)->GetJavaVM(env, &jvm);
    av_jni_set_java_vm(jvm, NULL);
    return JNI_OK;
}

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6) != JNI_OK
        | registerPlayer(env) < JNI_OK) {
        return JNI_ERR;
    }
//    av_jni_set_java_vm(vm, NULL);
    return JNI_VERSION_1_6;
}


JNIEXPORT void JNICALL
Java_cfans_ffmpeg_player_SilentPlayer_init(JNIEnv *env, jobject obj, jobject surface) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context == NULL) {
        context = (FFPlayerContext *) calloc(1, sizeof(FFPlayerContext));
        context->_nativeWindow = ANativeWindow_fromSurface(env, surface);
        (*env)->SetLongField(env, obj, gJNIContext, (jlong) context);
        context->_object = (*env)->NewGlobalRef(env, obj);;
        pthread_mutex_init(&context->_mutex, NULL);
        pthread_cond_init(&context->_cond, NULL);
    }
    LOGE(" init  %p", context);
}


JNIEXPORT jint JNICALL
Java_cfans_ffmpeg_player_SilentPlayer_start(JNIEnv *env, jobject obj, jstring path) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    int ret = JNI_FALSE;
    if (context && !context->_isStarted) {
        const char *file = (*env)->GetStringUTFChars(env, path, 0);
        ret = initPlayer(context, file);
        (*env)->ReleaseStringUTFChars(env, path, file);
        if (ret >= 0) {
            context->_isStarted = JNI_TRUE;
            context->_isPause = JNI_FALSE;
            pthread_create(&context->_playThread, NULL, playerThread, context);
        } else {
            deinitPlayer(context);
        }
    }
    return ret;
}

JNIEXPORT void JNICALL Java_cfans_ffmpeg_player_SilentPlayer_play(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context && context->_isPause) {
        context->_isPause = JNI_FALSE;
        pthread_cond_signal(&context->_cond);
        LOGE(" play  %d", context->_isPause);
    }
}

JNIEXPORT void JNICALL Java_cfans_ffmpeg_player_SilentPlayer_pause(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context && !context->_isPause) {
        context->_isPause = JNI_TRUE;
        pthread_cond_signal(&context->_cond);
        LOGE(" pause  %d", context->_isPause);
    }
}

JNIEXPORT void JNICALL Java_cfans_ffmpeg_player_SilentPlayer_stop(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context) {
        context->_isStarted = JNI_FALSE;
        usleep(100);
        LOGE(" stop ");
    }
}

JNIEXPORT void JNICALL Java_cfans_ffmpeg_player_SilentPlayer_destroy(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context) {
        pthread_mutex_destroy(&context->_mutex);
        pthread_cond_destroy(&context->_codecCtx);
        (*env)->SetLongField(env, obj, gJNIContext, 0);
        (*env)->DeleteGlobalRef(env, context->_object);
        ANativeWindow_release(context->_nativeWindow);
        context->_nativeWindow = NULL;
        free(context);
        LOGE(" destroy ");
    }
}

JNIEXPORT jboolean JNICALL
Java_cfans_ffmpeg_player_SilentPlayer_isPlaying(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context) {
        return !context->_isPause;
    }
    return JNI_TRUE;
}


JNIEXPORT void JNICALL
Java_cfans_ffmpeg_player_SilentPlayer_seekTo(JNIEnv *env, jobject obj, jlong index) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context) {
        context->_frameCurrent = context->_nb_frames * index / 1000;
        int ret = av_seek_frame(context->_formatCtx, context->_nb_streams,
                                context->_frameCurrent * context->_frameDuration, AVSEEK_FLAG_ANY);
        LOGE(" seekTo index=%d,time=%lld ,ret=%d", context->_frameCurrent, ret,
             context->_frameCurrent * context->_frameDuration, ret);
    }
}

JNIEXPORT jlong JNICALL
Java_cfans_ffmpeg_player_SilentPlayer_getDuration(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context) {
        return context->_duration / 1000;
    }
    return JNI_FALSE;
}

JNIEXPORT jlong JNICALL
Java_cfans_ffmpeg_player_SilentPlayer_getCurrentPosition(JNIEnv *env, jobject obj) {
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context) {
        return context->_duration * context->_frameCurrent / context->_nb_frames / 1000;
    }
    return JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_cfans_ffmpeg_player_SilentPlayer_snapshot(JNIEnv *env, jobject obj,jstring path)
{
    FFPlayerContext *context = (FFPlayerContext *) (*env)->GetLongField(env, obj, gJNIContext);
    if (context) {
//        struct timeval tvs, tve;
//        gettimeofday(&tvs, NULL);
        AndroidBitmapInfo  info;
        void*              pixels;
        int                ret;
        jobject bitmap = createBitmap(env,context->_codecCtx->width,context->_codecCtx->height);
        if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
            LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
            return JNI_FALSE;
        }

        if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
            LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
            return JNI_FALSE;
        }
        fillBitmap(&info, pixels,context->_rgbFrame);
        saveBitmap(env,bitmap,path);

        AndroidBitmap_unlockPixels(env, bitmap);
        (*env)->DeleteLocalRef(env,bitmap);
//        gettimeofday(&tve, NULL);
//        long time = (tve.tv_sec - tvs.tv_sec) * AV_TIME_BASE + (tve.tv_usec - tvs.tv_usec);
//        LOGE("decode time:%d",time);
//       const char *file = (*env)->GetStringUTFChars(env, path, 0);
//       saveFrame(context->_rgbFrame,640,360,file);
//       (*env)->ReleaseStringUTFChars(env, path, file);
        return JNI_TRUE;
    }
    return JNI_FALSE;
}

static void onProgressEvent(FFPlayerContext *context) {
    JNIEnv *env = NULL;
    JavaVM *jvm = av_jni_get_java_vm(NULL);
    if ((*jvm)->GetEnv(jvm, (void **) &env, JNI_VERSION_1_6) >= 0) {
        (*env)->CallVoidMethod(env, context->_object, gMethodID, context->_frameCurrent,
                               context->_nb_frames);
    } else {
        if ((*jvm)->AttachCurrentThread(jvm, &env, NULL) < 0) {
            return;
        }
        (*env)->CallVoidMethod(env, context->_object, gMethodID, context->_frameCurrent,
                               context->_nb_frames);
        (*jvm)->DetachCurrentThread(jvm);
    }
}

static void *playerThread(void *arg) {

    FFPlayerContext *context = arg;
    AVFormatContext *pFormatCtx = context->_formatCtx;
    AVCodecContext *pCodecCtx = context->_codecCtx;
    int width = pCodecCtx->width;
    int height = pCodecCtx->height;

    ANativeWindow_setBuffersGeometry(context->_nativeWindow, width, height,
                                     WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer windowBuffer;
    AVFrame *pFrame = av_frame_alloc();
    AVFrame *pFrameRGBA = av_frame_alloc();
    context->_rgbFrame = pFrameRGBA;
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
    uint8_t *buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(pFrameRGBA->data, pFrameRGBA->linesize, buffer, AV_PIX_FMT_RGBA, width,
                         height, 1);
    struct SwsContext *sws_ctx = sws_getContext(width,
                                                height,
                                                pCodecCtx->pix_fmt,
                                                width,
                                                height,
                                                AV_PIX_FMT_RGBA,
                                                SWS_FAST_BILINEAR,
                                                NULL,
                                                NULL,
                                                NULL);
    AVPacket packet;
    pthread_mutex_lock(&context->_mutex);
    const int frameInterval = 1000000 / context->_frameRate;//微秒
    int delay;
    int decodeInterval;
    int timeInterval;

    struct timeval tvs, tve;
    LOGE(" playerThread begin %p ,[%dX%d],frame=%d,interval=%d", context, width, height,
         context->_frameRate, frameInterval);

    while (context->_isStarted) {

        if (context->_isPause) {
            pthread_cond_wait(&context->_cond, &context->_mutex);
        } else {
            gettimeofday(&tvs, NULL);
            if (av_read_frame(pFormatCtx, &packet) >= 0) {
                if (packet.stream_index == context->_nb_streams) {
                    if (avcodec_send_packet(pCodecCtx, &packet) == 0) {
                        context->_frameCurrent++;
                        while (avcodec_receive_frame(pCodecCtx, pFrame) == 0) {
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

                    gettimeofday(&tve, NULL);
                    decodeInterval =
                            (tve.tv_sec - tvs.tv_sec) * AV_TIME_BASE + (tve.tv_usec - tvs.tv_usec);
//                    LOGE(" packet:  %lld, %lld,%lld,decode time:%d", packet.dts, packet.duration,
//                         packet.pos, decodeInterval);

                    delay = frameInterval - decodeInterval;
                    if (delay > 0) {
                        usleep(delay);
                        timeInterval += frameInterval;
                    } else {
                        timeInterval += decodeInterval;
                    }

                    if (timeInterval > AV_TIME_BASE) {
                        timeInterval = 0;
                        onProgressEvent(context);
                    }

                }
                av_packet_unref(&packet);
            } else {
                onProgressEvent(context);
                context->_frameCurrent = 0;
                av_seek_frame(context->_formatCtx, context->_nb_streams, 0, AVSEEK_FLAG_BACKWARD);
                context->_isPause = JNI_TRUE;
            }
        }
    }
    pthread_mutex_unlock(&context->_mutex);
    av_free(buffer);
    av_free(pFrameRGBA);
    av_free(pFrame);

    context->_rgbFrame = NULL;
    deinitPlayer(context);
    LOGE("playerThread end");
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
    AVStream *stream = formatCtx->streams[context->_nb_streams];

    if (stream->codecpar->codec_id == AV_CODEC_ID_H264) {
        context->_avCodec = avcodec_find_decoder_by_name("h264_mediacodec");
        LOGE("use media codec id=%d\n", stream->codecpar->codec_id);
    } else {
        context->_avCodec = avcodec_find_decoder(stream->codecpar->codec_id);
        LOGE("use ffmpeg codec id=%d\n", stream->codecpar->codec_id);
    }
    if (context->_avCodec == NULL) {
        LOGE("avcodec_find_decoder error. \n");
        deinitPlayer(context);
        return -1;
    }
    AVCodecContext *codecCtx = avcodec_alloc_context3(context->_avCodec);
    context->_codecCtx = codecCtx;

    codecCtx->codec_type = stream->codecpar->codec_type;
    if ((ret = avcodec_parameters_to_context(codecCtx, stream->codecpar)) < 0) {
        LOGE("Couldn't find stream information. ret=%d\n", ret);
        deinitPlayer(context);
        return ret;
    }

    av_codec_set_pkt_timebase(codecCtx, stream->time_base);
    if ((ret = avcodec_open2(codecCtx, context->_avCodec, NULL)) < 0) {
        LOGE("avcodec_open2 error= %d \n", ret);
        deinitPlayer(context);
        return ret;
    }

    context->_nb_frames = stream->nb_frames;
    context->_frameRate = stream->r_frame_rate.num / stream->r_frame_rate.den;
    context->_frameDuration = stream->time_base.den / context->_frameRate;
    context->_duration = formatCtx->duration;
    context->_frameCurrent = 0;

    LOGE("initPlayer frames=%lld frameRate=%d,frameDuration=%d(us), duration=%lld (s) \n",
         stream->nb_frames, context->_frameRate, context->_frameDuration,
         context->_duration / AV_TIME_BASE);
//    LOGE("initPlayer  tickets =%d (s) ,tickets =%d (s),codec id=%d\n", stream->time_base.num,
//         stream->time_base.den,stream->codecpar->codec_id);
//    LOGE("initPlayer  tickets =%d (s) ,tickets =%d (s)\n", codecCtx->pkt_timebase.num,
//         codecCtx->pkt_timebase.den);

    return ret;
}

static void deinitPlayer(FFPlayerContext *context) {
    if (context) {
        if (context->_codecCtx) {
            avcodec_close(context->_codecCtx);
            avcodec_free_context(&context->_codecCtx);
            context->_codecCtx = NULL;
        }

        if (context->_formatCtx) {
            avformat_close_input(&context->_formatCtx);
        }
    }
}

static void saveFrame(AVFrame* pFrame, int width, int height, const char * path)
{
    FILE *pFile;
    pFile = fopen(path, "wb");
    if(pFile){
        fprintf(pFile, "P6\n%d %d\n255\n", width, height);
        for (int y = 0; y < height; y++)
            fwrite(pFrame->data[0] + y*pFrame->linesize[0], 1, width * 3, pFile);

        fclose(pFile);
        LOGE("saveFrame success");
    }else{
        LOGE("saveFrame error");
    }
}

//static method
static void fillBitmap(AndroidBitmapInfo*  info, void *pixels, AVFrame *pFrame)
{
    uint8_t *frameLine;
    int  yy;
    for (yy = 0; yy < info->height; yy++) {
        uint8_t*  line = (uint8_t*)pixels;
        frameLine = pFrame->data[0] + (yy * pFrame->linesize[0]);

        int xx;
        for (xx = 0; xx < info->width; xx++) {
            int out_offset = xx * 4;
            int in_offset = xx * 3;

            line[out_offset] = frameLine[in_offset];
            line[out_offset+1] = frameLine[in_offset+1];
            line[out_offset+2] = frameLine[in_offset+2];
            line[out_offset + 3] = 0xff; //主要是A值
        }
        pixels = (char*)pixels + info->stride;
    }
}

static jobject createBitmap(JNIEnv *env,int width,int height){
    jstring objFormat = (*env)->NewStringUTF(env,"ARGB_8888");
    jclass clsConfig = (*env)->FindClass(env,"android/graphics/Bitmap$Config");
    jmethodID methodValue = (*env)->GetStaticMethodID(env,clsConfig, "valueOf", "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;");

    jobject objConfig = (*env)->CallStaticObjectMethod(env,clsConfig, methodValue, objFormat);
    jclass clsBitmap = (jclass)((*env)->FindClass(env,"android/graphics/Bitmap"));
    jmethodID methodCreate = (*env)->GetStaticMethodID(env,clsBitmap,
                                           "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");

    jobject bitmap = (*env)->CallStaticObjectMethod(env,clsBitmap,
                                                    methodCreate, width, height, objConfig);
    (*env)->DeleteLocalRef(env,objConfig);
    (*env)->DeleteLocalRef(env,objFormat);

    (*env)->DeleteLocalRef(env,clsConfig);
    (*env)->DeleteLocalRef(env,clsBitmap);
    return bitmap;
}

static jboolean saveBitmap(JNIEnv *env,jobject bitmap,jstring path){

    jclass clsCompress = (*env)->FindClass(env,"android/graphics/Bitmap$CompressFormat");
    jfieldID fieldCompress = (*env)->GetStaticFieldID(env,clsCompress, "JPEG","Landroid/graphics/Bitmap$CompressFormat;");
    jobject objCompress = (*env)->GetStaticObjectField(env,clsCompress, fieldCompress);

    jclass clsBitmap = ((*env)->FindClass(env,"android/graphics/Bitmap"));
    jmethodID methodCompress = (*env)->GetMethodID(env,clsBitmap,"compress","(Landroid/graphics/Bitmap$CompressFormat;ILjava/io/OutputStream;)Z");

    jclass clsStream = ((*env)->FindClass(env,"java/io/FileOutputStream"));
    jmethodID methodInit = (*env)->GetMethodID(env,clsStream, "<init>","(Ljava/lang/String;)V");
    jobject objSteam = ((*env)->NewObject(env,clsStream,methodInit,path));
    jmethodID methodClose = (*env)->GetMethodID(env, clsStream, "close", "()V");


    (*env)->CallBooleanMethod(env,bitmap, methodCompress, objCompress, 100, objSteam);
    (*env)->CallVoidMethod (env, objSteam,methodClose);

    (*env)->DeleteLocalRef(env,clsCompress);
    (*env)->DeleteLocalRef(env,clsStream);

    (*env)->DeleteLocalRef(env,objCompress);
    (*env)->DeleteLocalRef(env,objSteam);

}