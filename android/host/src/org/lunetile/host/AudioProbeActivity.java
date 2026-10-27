package org.lunetile.host;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

/** Internal audio fixture probe, never a game runtime. */
public final class AudioProbeActivity extends Activity {
    private static final String TAG = "LunetileAudioProbe";
    private final Handler main = new Handler();
    private GameAudio audio;
    private final int[] last = {-1, -1};
    private final boolean[] looped = {false, false};

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        TextView label = new TextView(this);
        label.setText("Lunetile audio fixture probe — no game runtime");
        setContentView(label);
        try {
            File wav = copy("tone.wav"), ogg = copy("tone.ogg");
            File corrupt = new File(getFilesDir(), "corrupt.ogg");
            try (FileOutputStream out = new FileOutputStream(corrupt)) { out.write(new byte[]{1, 2, 3}); }
            audio = new GameAudio(new GameAudio.Listener() {
                @Override public void onStarted(GameAudio.Channel c) { Log.i(TAG, "START " + c); }
                @Override public void onStopped(GameAudio.Channel c) {
                    Log.i(TAG, "STOP " + c);
                    if (c == GameAudio.Channel.ME) try {
                        audio.status(GameAudio.Channel.BGM, (channel, playing, pos, duration) -> Log.i(TAG, "RESUMED BGM playing=" + playing));
                    } catch (IllegalStateException ignored) { }
                }
                @Override public void onError(GameAudio.Channel c, String message) { Log.i(TAG, "ERROR " + c + " " + message); }
            });
            audio.play(GameAudio.Channel.BGM, wav.getPath(), 50);
            audio.play(GameAudio.Channel.BGS, ogg.getPath(), 50);
            audio.play(GameAudio.Channel.SE, wav.getPath(), 50, 150);
            audio.play(GameAudio.Channel.SE, corrupt.getPath(), 50);
            audio.play(GameAudio.Channel.SE, new File(getFilesDir(), "missing.wav").getPath(), 50);
            for (int i = 1; i <= 35; i++) main.postDelayed(this::sample, i * 120L);
            main.postDelayed(() -> {
                audio.play(GameAudio.Channel.ME, ogg.getPath(), 50, 75);
                audio.status(GameAudio.Channel.BGM, (c, playing, pos, duration) -> Log.i(TAG, "DURING ME BGM playing=" + playing));
            }, 900);
            main.postDelayed(() -> audio.status(GameAudio.Channel.BGM, (c, playing, pos, duration) -> Log.i(TAG, "AFTER ME BGM playing=" + playing)), 1800);
            main.postDelayed(() -> {
                audio.suspend();
                audio.status(GameAudio.Channel.BGM, (c, playing, pos, duration) -> Log.i(TAG, "SUSPENDED BGM playing=" + playing));
                audio.status(GameAudio.Channel.BGS, (c, playing, pos, duration) -> Log.i(TAG, "SUSPENDED BGS playing=" + playing));
            }, 2500);
            main.postDelayed(() -> {
                audio.resume();
                audio.status(GameAudio.Channel.BGM, (c, playing, pos, duration) -> Log.i(TAG, "RESUMED-LIFECYCLE BGM playing=" + playing));
                audio.status(GameAudio.Channel.BGS, (c, playing, pos, duration) -> Log.i(TAG, "RESUMED-LIFECYCLE BGS playing=" + playing));
            }, 3300);
            main.postDelayed(() -> {
                audio.stop(GameAudio.Channel.BGM);
                audio.stop(GameAudio.Channel.BGS);
                audio.stop(GameAudio.Channel.SE);
                audio.status(GameAudio.Channel.BGM, (c, playing, pos, duration) -> Log.i(TAG, "FINAL " + c + " " + playing));
                audio.status(GameAudio.Channel.BGS, (c, playing, pos, duration) -> Log.i(TAG, "FINAL " + c + " " + playing));
            }, 5000);
        } catch (Exception error) { Log.e(TAG, "SETUP ERROR", error); }
    }

    private File copy(String name) throws Exception {
        File dest = new File(getFilesDir(), name);
        try (InputStream in = getAssets().open(name); FileOutputStream out = new FileOutputStream(dest)) {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) != -1) out.write(buffer, 0, read);
        }
        return dest;
    }

    private void sample() {
        for (GameAudio.Channel channel : new GameAudio.Channel[]{GameAudio.Channel.BGM, GameAudio.Channel.BGS}) {
            audio.status(channel, (c, playing, position, duration) -> {
                int index = c == GameAudio.Channel.BGM ? 0 : 1;
                if (playing && last[index] >= 0 && position < last[index] - 20) looped[index] = true;
                last[index] = position;
                Log.i(TAG, "SAMPLE " + c + " playing=" + playing + " position=" + position + " duration=" + duration + " loop=" + looped[index]);
            });
        }
    }

    @Override protected void onPause() {
        main.removeCallbacksAndMessages(null);
        if (audio != null) audio.close();
        super.onPause();
    }
}
