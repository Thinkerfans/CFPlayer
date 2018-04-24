package cfans.ffmpeg.player;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

public class MainActivity extends AppCompatActivity implements SurfaceHolder.Callback , View.OnClickListener {

    SurfaceHolder surfaceHolder;

    CFPlayer mPlayer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);


        findViewById(R.id.bt_start).setOnClickListener(this);
        findViewById(R.id.bt_pause).setOnClickListener(this);
        findViewById(R.id.bt_play).setOnClickListener(this);
        findViewById(R.id.bt_stop).setOnClickListener(this);

        SurfaceView surfaceView = (SurfaceView) findViewById(R.id.surface_view);
        surfaceView.setKeepScreenOn(true);
        surfaceHolder = surfaceView.getHolder();
        surfaceHolder.addCallback(this);

    }


    @Override
    public void onClick(View v) {

        switch (v.getId()){
            case R.id.bt_start:
                mPlayer.start("mnt/sdcard/RC-Follow/video/test.avi");
                break;
            case R.id.bt_play:
                mPlayer.play();
                break;
            case R.id.bt_pause:
                mPlayer.pause();
                break;
            case R.id.bt_stop:
                mPlayer.stop();
                break;
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mPlayer = new CFPlayer(surfaceHolder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        mPlayer.destroy();
    }

}
