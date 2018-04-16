package cfans.ffmpeg.player;

import android.view.Surface;
/**
 * Created by cfans on 2018/4/13.
 */

public class CFPlayer {

    static {
        System.loadLibrary("CFPlayer");
    }

    private long mContext;

    public CFPlayer(Surface surface){
        init(surface);
    }

    public native void init(Surface surface);
    public native int start(String path);
    public native void play();
    public native void pause();
    public native boolean isPlaying();
    public native void stop();
    public native void destroy();

    /**
     * 获取视频总时间：秒数
     */
    public native int getDuration();
    /**
     * 指定位置开始播放：秒数
     */
    public native void seekTo(int second);

}
