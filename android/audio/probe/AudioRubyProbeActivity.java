package org.lunetile.audio;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

/** Isolated one-shot bridge diagnostic; never a game runtime. */
public final class AudioRubyProbeActivity extends Activity {
    static { System.loadLibrary("audio_ruby_probe"); }
    private static native String runProbe(AudioBridge bridge);
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        TextView text = new TextView(this);
        setContentView(text);
        try {
            for (String name : new String[]{"tone.wav", "tone.ogg", "tone.mid"}) {
                try (InputStream in = getAssets().open(name); FileOutputStream out = new FileOutputStream(new File(getFilesDir(), name))) {
                    byte[] buffer = new byte[8192]; int count;
                    while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
                }
            }
            try (FileOutputStream out = new FileOutputStream(new File(getFilesDir(), "corrupt.ogg"))) { out.write(new byte[]{1, 2, 3}); }
            try (AudioBridge bridge = new AudioBridge(getFilesDir())) {
                String result = runProbe(bridge);
                if (bridge.isPlaying(0) || bridge.isPlaying(1) || bridge.isPlaying(2)) result = "FAIL: faded channel still playing";
                text.setText(result);
                Log.i("AudioRubyProbe", result);
            }
        } catch (Exception error) {
            text.setText("FAIL: " + error);
            Log.e("AudioRubyProbe", "FAIL", error);
        }
    }
}
