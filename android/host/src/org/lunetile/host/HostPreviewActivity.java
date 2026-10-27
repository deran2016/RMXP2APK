package org.lunetile.host;

import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.widget.Button;
import android.view.Gravity;
import android.graphics.Color;
import android.util.Log;

/** Standalone placeholder; no RGSS runtime is attached. */
public final class HostPreviewActivity extends Activity {
    private GameHostView game;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        game = new GameHostView(this, 640, 480);
        game.setInputListener((key, pressed) -> Log.i("LunetileHostPreview", "input " + key + " " + pressed));
        TextView status = new TextView(this);
        status.setText("Android 게임 런타임이 연결되지 않았습니다");
        status.setTextColor(Color.WHITE);
        status.setGravity(Gravity.CENTER);
        FrameLayout container = new FrameLayout(this);
        container.setOnApplyWindowInsetsListener((view, insets) -> {
            status.setPadding(0, insets.getSystemWindowInsetTop(), 0, 0);
            return insets;
        });
        container.addView(game);
        container.addView(status, new FrameLayout.LayoutParams(-1, -2, Gravity.TOP));
        Button audioProbe = new Button(this);
        audioProbe.setText("오디오 테스트");
        audioProbe.setOnClickListener(view -> startActivity(new Intent(this, AudioProbeActivity.class)));
        container.addView(audioProbe, new FrameLayout.LayoutParams(-2, -2, Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL));
        setContentView(container);
    }

    @Override protected void onPause() {
        game.releaseAll();
        super.onPause();
    }
}
