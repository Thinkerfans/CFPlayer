package cfans.ffmpeg.player;

import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.Surface;
/**
 * Created by cfans on 2018/4/13.
 */

public class SilentPlayer {

    static {
        System.loadLibrary("SilentPlayer");
    }

    private long mContext;
    private Handler mHandler;

    public native void init(Surface surface);
    public native int start(String path);
    public native void play();
    public native void pause();
    public native boolean isPlaying();
    public native void stop();
    public native void destroy();

    /**
     * 获取视频总时间：毫秒数
     */
    public native long getDuration();

    /**
     * 获取视频当前播放位置：毫秒数
     */
    public native long getCurrentPosition();
    /**
     * 指定位置开始播放：帧序列
     */
    public native void seekTo(long index);

    /**
     * 截图
     */
    public native int snapshot(String path);

    /**
     * JNI回调进度，大约每秒回调一次
     */
    public void postProgressFromJNI(long index, long total) {
        Log.e(" postProgressFromJNI: ","current frame:"+index+" total frame: "+total);
        if (mHandler != null){
            Message msg =  mHandler.obtainMessage();
            msg.arg1 = (int)(1000*index/total);
            mHandler.sendMessage(msg);
        }
    }

    public void setHandler(Handler handler){
        mHandler = handler;
    }

}
