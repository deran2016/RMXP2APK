package org.lunetile.input;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import org.lunetile.host.TouchInputState;

/** Isolated UI-thread to Ruby-thread input diagnostic. */
public final class InputProbeActivity extends Activity {
    static { System.loadLibrary("input_ruby_probe"); }
    private final TouchInputState state = new TouchInputState();
    private native String runProbe(TouchInputState state);
    public void stage(int index) throws InterruptedException {
        CountDownLatch done = new CountDownLatch(1);
        runOnUiThread(() -> {
            switch (index) {
                case 0: state.setTouch(TouchInputState.UP, true); break;
                case 1: state.setTouch(TouchInputState.RIGHT, true); break;
                case 2: state.clearTouches(); break;
                case 3: state.setTouch(TouchInputState.CONFIRM, true); state.setTouch(TouchInputState.CONFIRM, false); break;
                case 4: state.setTouch(TouchInputState.CANCEL, true); state.cancelTouch(TouchInputState.CANCEL); break;
                default: throw new IllegalArgumentException("Unknown stage");
            }
            done.countDown();
        });
        if (!done.await(10, TimeUnit.SECONDS)) throw new IllegalStateException("UI stage timeout");
    }
    @Override public void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        TextView output = new TextView(this);
        setContentView(output);
        new Thread(() -> {
            String result = runProbe(state);
            Log.i("InputRubyProbe", result);
            runOnUiThread(() -> output.setText(result));
        }, "RubyInputProbe").start();
    }
}
