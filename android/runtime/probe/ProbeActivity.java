package org.lunetile.probe;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

public final class ProbeActivity extends Activity {
    static { System.loadLibrary("ruby_probe"); }
    private static native String runRubyProbe();

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        String result = runRubyProbe();
        Log.i("LunetileRubyProbe", result);
        TextView text = new TextView(this);
        text.setText("Diagnostic only; no game runtime\n" + result);
        setContentView(text);
    }
}
