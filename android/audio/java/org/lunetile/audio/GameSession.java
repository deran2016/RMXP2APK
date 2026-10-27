package org.lunetile.audio;

import android.view.SurfaceHolder;
import org.lunetile.host.GameHostView;
import org.lunetile.host.TouchInputState;
import java.io.IOException;

/** Lifecycle coordinator prototype. A real game engine must implement Engine on its VM thread. */
public final class GameSession implements SurfaceHolder.Callback, AutoCloseable {
    public interface Engine {
        void surfaceAvailable(SurfaceHolder holder);
        void surfaceChanged(SurfaceHolder holder, int width, int height);
        void surfaceLost();
        void resume(Runnable afterResume);
        void pause(Runnable afterPause);
        /** Must stop and join the VM thread and unregister JNI references before returning. */
        void close();
    }
    private final GameHostView view;
    private final TouchInputState input;
    private final AudioBridge audio;
    private final Engine engine;
    private boolean closed;
    private boolean cleaned;

    public GameSession(GameHostView view, TouchInputState input, AudioBridge audio, Engine engine) {
        if (view == null || input == null || audio == null || engine == null) throw new IllegalArgumentException("All game session components are required");
        this.view = view;
        this.input = input;
        this.audio = audio;
        this.engine = engine;
        view.setInputListener(new GameHostView.InputListener() {
            @Override public void onInput(int key, boolean pressed) { input.setTouch(key, pressed); }
            @Override public void onTouchCancelled(int key) { input.cancelTouch(key); }
            @Override public void onAllTouchesCancelled() { input.clearTouches(); }
        });
        view.getGameSurface().addCallback(this);
        if (view.getGameSurface().getSurface().isValid()) {
            engine.surfaceAvailable(view.getGameSurface());
            android.graphics.Rect frame = view.getGameSurface().getSurfaceFrame();
            engine.surfaceChanged(view.getGameSurface(), frame.width(), frame.height());
        }
    }

    public void resume() { if (!closed) engine.resume(audio::resume); }
    public void pause() throws IOException {
        if (closed) return;
        view.releaseAll();
        input.clearTouches();
        engine.pause(audio::suspend);
    }
    @Override public void surfaceCreated(SurfaceHolder holder) { if (!closed) engine.surfaceAvailable(holder); }
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) { if (!closed) engine.surfaceChanged(holder, width, height); }
    @Override public void surfaceDestroyed(SurfaceHolder holder) { if (!closed) engine.surfaceLost(); }
    /** Must be called on the UI thread before closing on a worker. */
    public void detach() {
        if (closed) return;
        closed = true;
        view.releaseAll();
        view.getGameSurface().removeCallback(this);
        engine.surfaceLost();
        view.setInputListener(null);
    }
    @Override public synchronized void close() {
        if (!closed) throw new IllegalStateException("Detach the session on the UI thread first");
        if (cleaned) return;
        cleaned = true;
        try { engine.close(); }
        finally { audio.close(); }
    }
}
