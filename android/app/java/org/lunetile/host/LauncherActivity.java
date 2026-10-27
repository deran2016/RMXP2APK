package org.lunetile.host;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.Gravity;
import android.widget.TextView;
import java.io.File;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;
import java.nio.channels.OverlappingFileLockException;
import java.util.UUID;

/** Waits outside the game-only process until its prior Ruby VM has exited. */
public final class LauncherActivity extends Activity {
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        TextView status = new TextView(this);
        status.setText("게임 준비 중");
        status.setGravity(Gravity.CENTER);
        setContentView(status);
        new Thread(() -> {
            try (RandomAccessFile file = new RandomAccessFile(new File(getFilesDir(), "GameSession.lock"), "rw")) {
                long deadline = SystemClock.uptimeMillis() + 20000;
                String token = UUID.randomUUID().toString();
                File ack = new File(getFilesDir(), "GameLaunchAck-" + token);
                boolean requested = false;
                while (SystemClock.uptimeMillis() < deadline && !isDestroyed()) {
                    if (ack.isFile()) { ack.delete(); runOnUiThread(this::finish); return; }
                    FileLock lock = null;
                    try { lock = file.getChannel().tryLock(); } catch (OverlappingFileLockException ignored) { }
                    if (lock != null) {
                        lock.release();
                        runOnUiThread(() -> {
                            if (!isFinishing() && !isDestroyed()) {
                                startActivity(new Intent(this, GameActivity.class));
                                finish();
                            }
                        });
                        return;
                    }
                    file.seek(0);
                    if (!requested && file.read() == 'A') {
                        requested = true;
                        runOnUiThread(() -> {
                            if (!isFinishing() && !isDestroyed()) {
                                Intent intent = new Intent(this, GameActivity.class);
                                intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                                intent.putExtra("launch_ack", token);
                                startActivity(intent);
                            }
                        });
                    }
                    Thread.sleep(100);
                }
                if (ack.isFile()) ack.delete();
                if (!isDestroyed()) runOnUiThread(() -> status.setText("이전 게임 종료를 기다리는 시간이 초과되었습니다."));
            } catch (Exception error) {
                runOnUiThread(() -> { if (!isDestroyed()) status.setText("게임을 시작할 수 없습니다: " + error); });
            }
        }, "LunetileLaunchWait").start();
    }
}
