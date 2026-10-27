package org.lunetile.host;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.FrameLayout;
import android.widget.TextView;
import org.json.JSONObject;
import org.lunetile.audio.AudioBridge;
import org.lunetile.audio.GameSession;
import org.lunetile.runtime.BitmapCodec;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;

/** Experimental game host. It requires the separately built native engine and packaged game assets. */
public final class GameActivity extends Activity {
    private GameHostView view;
    private TextView status;
    private GameSession session;
    private volatile boolean destroyed;
    private boolean resumed;
    private Thread assetWorker;
    private RandomAccessFile sessionLockFile;
    private FileLock sessionLock;

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        view = new GameHostView(this, 640, 480);
        status = new TextView(this);
        status.setText("게임 준비 중");
        status.setGravity(Gravity.CENTER);
        FrameLayout layout = new FrameLayout(this);
        layout.addView(view);
        layout.addView(status, new FrameLayout.LayoutParams(-1, -2, Gravity.TOP));
        setContentView(layout);
        try {
            sessionLockFile = new RandomAccessFile(new File(getFilesDir(), "GameSession.lock"), "rw");
            sessionLock = sessionLockFile.getChannel().tryLock();
            if (sessionLock == null) throw new IllegalStateException("Another game session is still active");
            sessionLockFile.seek(0); sessionLockFile.write('A'); sessionLockFile.getFD().sync();
            acknowledge(getIntent());
        } catch (Exception error) { status.setText("게임 세션을 시작할 수 없습니다: " + error); return; }
        assetWorker = new Thread(() -> {
            try {
                File root = GameAssets.prepare(this);
                JSONObject metadata = new JSONObject(new String(Files.readAllBytes(new File(root, "android-build.json").toPath()), StandardCharsets.UTF_8));
                String encoding = metadata.getString("data_encoding");
                String scripts = GameAssets.scriptsPath(root);
                runOnUiThread(() -> start(root, encoding, scripts));
            } catch (Exception error) { report(error.toString()); }
        }, "LunetileAssets");
        assetWorker.start();
    }

    private void acknowledge(Intent intent) {
        String token = intent == null ? null : intent.getStringExtra("launch_ack");
        if (token == null || !token.matches("[0-9a-f-]{36}")) return;
        try { new File(getFilesDir(), "GameLaunchAck-" + token).createNewFile(); }
        catch (Exception error) { android.util.Log.e("Lunetile", "Cannot acknowledge launch", error); }
    }

    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        if (!destroyed && sessionLock != null && sessionLock.isValid()) acknowledge(intent);
    }

    private void start(File root, String encoding, String scripts) {
        if (destroyed) return;
        AudioBridge audio = null;
        NativeGameEngine engine = null;
        try {
            TouchInputState input = new TouchInputState();
            audio = new AudioBridge(root);
            engine = new NativeGameEngine(BitmapCodec.class, input, audio, root.getAbsolutePath(), getFilesDir().getAbsolutePath(), encoding, scripts, 640, 480, resumed, this::report);
            status.setText("");
            session = new GameSession(view, input, audio, engine);
            if (resumed) session.resume();
        } catch (Throwable error) {
            if (engine != null) try { engine.close(); } catch (RuntimeException ignored) { }
            if (audio != null) audio.close();
            report(error.toString());
        }
    }

    private void report(String error) {
        runOnUiThread(() -> {
            if (destroyed) return;
            if (error == null) finish();
            else status.setText(error);
        });
    }

    @Override protected void onResume() {
        super.onResume(); resumed = true;
        if (session != null) session.resume();
    }
    @Override protected void onPause() {
        resumed = false;
        if (session != null) try { session.pause(); } catch (Exception error) { report(error.toString()); }
        super.onPause();
    }
    @Override protected void onDestroy() {
        destroyed = true;
        try { if (sessionLock != null && sessionLock.isValid()) { sessionLockFile.seek(0); sessionLockFile.write('C'); sessionLockFile.getFD().sync(); } }
        catch (Exception error) { android.util.Log.e("Lunetile", "Cannot mark session closing", error); }
        GameSession closing = session;
        session = null;
        if (closing != null) closing.detach();
        new Thread(() -> {
            try {
                if (closing != null) closing.close();
                if (assetWorker != null) {
                    assetWorker.join(15000);
                    if (assetWorker.isAlive()) throw new IllegalStateException("Asset preparation did not stop");
                }
                // The session lock is released by the OS only when this game-only process exits.
                android.os.Process.killProcess(android.os.Process.myPid());
            } catch (Exception error) { android.util.Log.e("Lunetile", "Session shutdown failed", error); }
        }, "LunetileShutdown").start();
        super.onDestroy();
    }
}
