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
    public native static int play(Surface surface, String path);
}
