package org.lunetile.savediagnostic20261027;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

public final class ProbeActivity extends Activity {
    static { System.loadLibrary("lunetile_save_probe"); }
    private static native String run(String directory, String mode);
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        String mode = getIntent().getStringExtra("mode");
        String result = run(getFilesDir().getAbsolutePath(), mode == null ? "write" : mode);
        Log.i("LunetileSaveProbe", result);
        TextView text = new TextView(this);
        text.setText("Save diagnostic only; no game\n" + result);
        setContentView(text);
    }
}
