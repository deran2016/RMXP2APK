package org.lunetile.host;

import android.view.Surface;
import android.view.SurfaceHolder;
import org.lunetile.audio.AudioBridge;
import org.lunetile.audio.GameSession;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/** One Ruby VM worker per process. The native engine remains experimental. */
public final class NativeGameEngine implements GameSession.Engine {
    public interface ExitListener { void onExit(String error); }
    static { System.loadLibrary("lunetile_game"); }
    private static native String nativeRun(Class<?> codec, TouchInputState input, AudioBridge audio, String gameRoot, String savesRoot, String encoding, String scriptsPath, int width, int height, boolean highRefresh);
    private static native boolean nativeSurface(Surface surface);
    private static native void nativePause(boolean paused);
    private static native void nativeStop();
    private final Thread worker;
    private final ExecutorService lifecycle = Executors.newSingleThreadExecutor();
    private final ExitListener listener;
    private volatile boolean closed;

    public NativeGameEngine(Class<?> codec, TouchInputState input, AudioBridge audio, String gameRoot, String savesRoot,
                            String encoding, String scriptsPath, int width, int height, boolean highRefresh, boolean initiallyResumed, ExitListener listener) {
        if (listener == null) throw new IllegalArgumentException("Exit listener is required");
        this.listener = listener;
        nativePause(!initiallyResumed);
        worker = new Thread(() -> {
            String error;
            try { error = nativeRun(codec, input, audio, gameRoot, savesRoot, encoding, scriptsPath, width, height, highRefresh); }
            catch (Throwable failure) { error = failure.toString(); }
            listener.onExit(error);
        }, "LunetileRuby");
        worker.start();
    }

    @Override public void surfaceAvailable(SurfaceHolder holder) {
        if (!closed && !nativeSurface(holder.getSurface())) listener.onExit("Cannot bind Android game surface");
    }
    @Override public void surfaceChanged(SurfaceHolder holder, int width, int height) { }
    @Override public void surfaceLost() { if (!closed) nativeSurface(null); }
    private void pauseAsync(boolean paused, Runnable completion) {
        if (!closed) lifecycle.execute(() -> {
            try { nativePause(paused); } catch (Throwable failure) { listener.onExit(failure.toString()); }
            finally { completion.run(); }
        });
    }
    @Override public void resume(Runnable completion) { pauseAsync(false, completion); }
    @Override public void pause(Runnable completion) { pauseAsync(true, completion); }
    @Override public void close() {
        if (closed) return;
        closed = true;
        nativeStop();
        lifecycle.shutdown();
        try {
            if (!lifecycle.awaitTermination(15, TimeUnit.SECONDS)) throw new IllegalStateException("Lifecycle worker shutdown timed out");
            worker.join(15000);
        }
        catch (InterruptedException error) { Thread.currentThread().interrupt(); throw new IllegalStateException("Game thread shutdown interrupted", error); }
        if (worker.isAlive()) throw new IllegalStateException("Game thread shutdown timed out");
        nativeSurface(null);
    }
}
