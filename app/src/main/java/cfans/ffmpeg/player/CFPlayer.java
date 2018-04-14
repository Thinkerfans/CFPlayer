package cfans.ffmpeg.player;

import android.view.Surface;
import android.view.SurfaceView;

/**
 * Created by cfans on 2018/4/13.
 */

public class CFPlayer {

    static {
        System.loadLibrary("CFPlayer");
    }
    public native static int playTest(Surface surface, String path);

    private long mContext;

    public native int init(Surface surface, String path);

    public native void play();
    public native void pause();
    public native boolean isPlaying();
    public native void stop();

    public native int getCurrentPosition();
    public native int getDuration();
    public native void seekTo(int position);

}
