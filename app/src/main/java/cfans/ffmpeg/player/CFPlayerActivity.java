package cfans.ffmpeg.player;

import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.TextView;

/**
 * Created by cfans on 2018/4/25.
 */

public class CFPlayerActivity extends AppCompatActivity implements SurfaceHolder.Callback , View.OnClickListener{

    private static final int SHOW_PROGRESS = 0;
    private static final int FADE_OUT = 1;
    public final static String EXTRA_PATH = "path";

    SurfaceView mSurfaceView;
    SilentPlayer mPlayer;

    View mController;
    ImageView mPlayPause;
    TextView mTvDuration,mTvTime;
    SeekBar mProgress;

    long mDuration;
    boolean mDragging,mShowing;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.activity_vplayer);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        mPlayer = new SilentPlayer();
        mPlayer.setHandler(mHandler);
        initViews();
        mShowing = true;
    }

    @Override
    public void onClick(View v) {

        switch (v.getId()){
            case R.id.controller_play_pause:
                if (mPlayer.isPlaying()){
                    onVideoPause();
                }else{
                    onVideoPlay();
                }
                break;
                default:
                break;
        }
    }

    private void onVideoPause(){
        mPlayer.pause();
        mPlayPause.setImageResource(R.drawable.videocontroller_play);
    }

    private void onVideoPlay(){
        mPlayer.play();
        mPlayPause.setImageResource(R.drawable.videocontroller_pause);
    }

    private void initViews() {
        mSurfaceView  = (SurfaceView) findViewById(R.id.surface_view);
        mSurfaceView.setKeepScreenOn(true);
        mSurfaceView.getHolder().addCallback(this);

        mController = findViewById(R.id.layout_controller);
        mPlayPause = (ImageView)findViewById(R.id.controller_play_pause);
        mTvDuration = (TextView) findViewById(R.id.controller_time_total);
        mTvTime = (TextView)findViewById(R.id.controller_time_current);

        mProgress = (SeekBar)findViewById(R.id.controller_seekbar);
        mProgress.setOnSeekBarChangeListener(mSeekListener);
        mPlayPause.setOnClickListener(this);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mPlayer.init(holder.getSurface());
        String path = getIntent().getStringExtra(EXTRA_PATH);
        if (path != null) {
            mPlayer.start(path);
        }else{
            mPlayer.start("mnt/sdcard/RC-Follow/video/tes.mp4");
        }
        mDuration = mPlayer.getDuration();
        mTvDuration.setText(generateTime(mDuration));
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        mPlayer.stop();
        mPlayer.destroy();
    }

    public static String generateTime(long time) {
        int totalSeconds = (int) (time / 1000);
        int seconds = totalSeconds % 60;
        int minutes = (totalSeconds / 60) % 60;
        int hours = totalSeconds / 3600;
        return hours > 0 ? String.format("%02d:%02d:%02d", hours, minutes, seconds) : String.format("%02d:%02d", minutes, seconds);
    }

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            long pos;
            switch (msg.what) {
                case FADE_OUT:
//                    hide();
                    break;
                case SHOW_PROGRESS:
                    if (!mDragging && mShowing) {
                        mProgress.setProgress(msg.arg1);
                        updateProgressText(msg.arg1);
                        if (msg.arg1 == mProgress.getMax()){
                            onVideoPause();
                        }
                    }

                    break;
            }
        }
    };

    private void updateProgressText(int progress){
        String time = generateTime((mDuration * progress) / 1000);
        mTvTime.setText(time);
    }

    private SeekBar.OnSeekBarChangeListener mSeekListener = new SeekBar.OnSeekBarChangeListener() {
        public void onStartTrackingTouch(SeekBar bar) {
            mDragging = true;
        }

        public void onProgressChanged(SeekBar bar, int progress, boolean fromuser) {
            if (fromuser) {
                updateProgressText(progress);
            }
        }

        public void onStopTrackingTouch(SeekBar bar) {
            mPlayer.seekTo(bar.getProgress());
            mDragging = false;
        }
    };

}
