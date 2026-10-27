package org.lunetile.host;

import android.media.MediaPlayer;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.SystemClock;
import java.io.File;
import java.io.IOException;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.Locale;
import java.util.HashSet;

/** Android WAV/OGG playback prototype; independent from the RGSS runtime. */
public final class GameAudio implements AutoCloseable {
    public enum Channel { BGM, BGS, ME, SE }
    public interface Listener {
        void onStarted(Channel channel);
        void onStopped(Channel channel);
        void onError(Channel channel, String reason);
    }
    public interface StatusListener { void onStatus(Channel channel, boolean playing, int positionMs, int durationMs); }
    private final HandlerThread thread = new HandlerThread("LunetileAudio");
    private final Handler handler;
    private final Listener listener;
    private final ArrayList<MediaPlayer> effects = new ArrayList<>();
    private final HashSet<MediaPlayer> lifecyclePausedPlayers = new HashSet<>();
    private final Runnable[] fades = new Runnable[3];
    private final float[] gains = new float[3];
    private MediaPlayer bgm, bgs, me;
    private boolean closed;
    private boolean lifecyclePaused;
    private long pausedAt, totalPaused;

    public GameAudio(Listener listener) {
        if (listener == null) throw new IllegalArgumentException("Audio listener is required");
        this.listener = listener;
        thread.start();
        handler = new Handler(thread.getLooper());
    }

    public synchronized void play(Channel channel, String path, int volume) {
        play(channel, path, volume, 100);
    }

    public synchronized void play(Channel channel, String path, int volume, int pitch) {
        if (closed) throw new IllegalStateException("Audio is closed");
        if (channel == null || path == null || volume < 0 || volume > 100 || pitch < 50 || pitch > 150) throw new IllegalArgumentException("Invalid audio request");
        handler.post(() -> start(channel, path, volume, pitch));
    }

    private void start(Channel channel, String path, int volume, int pitch) {
        if (channel != Channel.SE) stopChannel(channel);
        String lower = path.toLowerCase(Locale.ROOT);
        if ((!lower.endsWith(".wav") && !lower.endsWith(".ogg") && !lower.endsWith(".mid") && !lower.endsWith(".midi")) || !new File(path).isFile()) {
            listener.onError(channel, "Expected an existing WAV, OGG or MIDI file: " + path);
            return;
        }
        if (lower.endsWith(".mid") || lower.endsWith(".midi")) {
            try (FileInputStream input = new FileInputStream(path)) {
                byte[] header = new byte[14];
                if (input.read(header) != header.length || header[0] != 'M' || header[1] != 'T' || header[2] != 'h' || header[3] != 'd' ||
                    header[4] != 0 || header[5] != 0 || header[6] != 0 || header[7] != 6 || header[8] != 0 || (header[9] != 0 && header[9] != 1)) {
                    listener.onError(channel, "Only MIDI SMF type 0 or 1 is supported: " + path);
                    return;
                }
            } catch (IOException error) { listener.onError(channel, error.toString()); return; }
        }
        MediaPlayer player = new MediaPlayer();
        try {
            player.setDataSource(path);
            player.setLooping(channel == Channel.BGM || channel == Channel.BGS);
            player.setVolume(volume / 100f, volume / 100f);
            player.setOnCompletionListener(mp -> handler.post(() -> finish(channel, mp)));
            player.setOnErrorListener((mp, what, extra) -> {
                handler.post(() -> { finish(channel, mp); listener.onError(channel, "MediaPlayer error " + what + "/" + extra); });
                return true;
            });
            player.prepare();
            if (pitch != 100) player.setPlaybackParams(player.getPlaybackParams().setSpeed(pitch / 100f).setPitch(pitch / 100f));
            if (channel == Channel.BGM) bgm = player;
            else if (channel == Channel.BGS) bgs = player;
            else if (channel == Channel.ME) me = player;
            else effects.add(player);
            if (channel != Channel.SE) gains[channel.ordinal()] = volume / 100f;
            player.start();
            if (channel == Channel.ME && bgm != null && bgm.isPlaying()) bgm.pause();
            if (channel == Channel.BGM && me != null) player.pause();
            if (lifecyclePaused && player.isPlaying()) { player.pause(); lifecyclePausedPlayers.add(player); }
            listener.onStarted(channel);
        } catch (IOException | RuntimeException error) {
            finish(channel, player);
            listener.onError(channel, error.toString());
        }
    }

    private void finish(Channel channel, MediaPlayer player) {
        boolean owned = false;
        if (channel == Channel.BGM && bgm == player) { bgm = null; owned = true; }
        if (channel == Channel.BGS && bgs == player) { bgs = null; owned = true; }
        if (channel == Channel.ME && me == player) { me = null; owned = true; }
        if (channel == Channel.SE) owned = effects.remove(player);
        lifecyclePausedPlayers.remove(player);
        if (owned) cancelFade(channel);
        if (owned) listener.onStopped(channel);
        try { player.release(); } catch (RuntimeException ignored) { }
        if (channel == Channel.ME && owned && bgm != null) {
            if (lifecyclePaused) lifecyclePausedPlayers.add(bgm);
            else try { bgm.start(); } catch (RuntimeException error) { listener.onError(Channel.BGM, error.toString()); }
        }
    }

    private void stopChannel(Channel channel) {
        cancelFade(channel);
        if (channel == Channel.BGM && bgm != null) finish(channel, bgm);
        if (channel == Channel.BGS && bgs != null) finish(channel, bgs);
        if (channel == Channel.ME && me != null) finish(channel, me);
        if (channel == Channel.SE) for (MediaPlayer player : new ArrayList<>(effects)) finish(channel, player);
    }

    public synchronized void stop(Channel channel) {
        if (closed) throw new IllegalStateException("Audio is closed");
        if (channel == null) throw new IllegalArgumentException("Channel is required");
        handler.post(() -> stopChannel(channel));
    }

    private void cancelFade(Channel channel) {
        if (channel == Channel.SE) return;
        int index = channel.ordinal();
        if (fades[index] != null) handler.removeCallbacks(fades[index]);
        fades[index] = null;
    }

    public synchronized void fade(Channel channel, int milliseconds) {
        if (closed) throw new IllegalStateException("Audio is closed");
        if (channel == null || channel == Channel.SE || milliseconds < 0) throw new IllegalArgumentException("Invalid fade request");
        handler.post(() -> {
            cancelFade(channel);
            MediaPlayer player = channel == Channel.BGM ? bgm : channel == Channel.BGS ? bgs : me;
            if (player == null) return;
            if (milliseconds == 0) { stopChannel(channel); return; }
            int index = channel.ordinal();
            long start = SystemClock.uptimeMillis();
            long pausedBefore = totalPaused + (lifecyclePaused ? start - pausedAt : 0);
            float initial = gains[index];
            Runnable fade = new Runnable() {
                @Override public void run() {
                    long now = SystemClock.uptimeMillis();
                    long paused = totalPaused + (lifecyclePaused ? now - pausedAt : 0);
                    long elapsed = now - start - (paused - pausedBefore);
                    if (lifecyclePaused) { handler.postDelayed(this, 20); return; }
                    if (elapsed >= milliseconds) { stopChannel(channel); return; }
                    float gain = initial * (1f - (float)elapsed / milliseconds);
                    try { player.setVolume(gain, gain); gains[index] = gain; handler.postDelayed(this, 20); }
                    catch (RuntimeException failure) { stopChannel(channel); listener.onError(channel, failure.toString()); }
                }
            };
            fades[index] = fade;
            handler.post(fade);
        });
    }

    public synchronized void flush(Runnable callback) {
        if (closed) throw new IllegalStateException("Audio is closed");
        if (callback == null) throw new IllegalArgumentException("Callback is required");
        handler.post(callback);
    }

    public synchronized void suspend() {
        if (closed) throw new IllegalStateException("Audio is closed");
        handler.post(() -> {
            if (lifecyclePaused) return;
            lifecyclePaused = true;
            pausedAt = SystemClock.uptimeMillis();
            ArrayList<MediaPlayer> players = new ArrayList<>(effects);
            if (bgm != null) players.add(bgm);
            if (bgs != null) players.add(bgs);
            if (me != null) players.add(me);
            for (MediaPlayer player : players) try {
                if (player.isPlaying()) { player.pause(); lifecyclePausedPlayers.add(player); }
            } catch (RuntimeException error) { listener.onError(player == bgm ? Channel.BGM : player == bgs ? Channel.BGS : player == me ? Channel.ME : Channel.SE, error.toString()); }
        });
    }

    public synchronized void resume() {
        if (closed) throw new IllegalStateException("Audio is closed");
        handler.post(() -> {
            if (!lifecyclePaused) return;
            totalPaused += SystemClock.uptimeMillis() - pausedAt;
            lifecyclePaused = false;
            for (MediaPlayer player : new ArrayList<>(lifecyclePausedPlayers)) try {
                if (player != bgm || me == null) player.start();
            } catch (RuntimeException error) { listener.onError(player == bgm ? Channel.BGM : player == bgs ? Channel.BGS : player == me ? Channel.ME : Channel.SE, error.toString()); }
            lifecyclePausedPlayers.clear();
        });
    }

    public synchronized void status(Channel channel, StatusListener callback) {
        if (closed) throw new IllegalStateException("Audio is closed");
        if (channel == null || channel == Channel.SE || callback == null) throw new IllegalArgumentException("Status requires BGM, BGS or ME and a callback");
        handler.post(() -> {
            MediaPlayer player = channel == Channel.BGM ? bgm : channel == Channel.BGS ? bgs : me;
            if (player == null) callback.onStatus(channel, false, 0, 0);
            else try { callback.onStatus(channel, player.isPlaying(), player.getCurrentPosition(), player.getDuration()); }
            catch (RuntimeException error) { listener.onError(channel, error.toString()); }
        });
    }

    @Override public void close() {
        synchronized (this) {
            if (closed) return;
            closed = true;
            handler.post(() -> { stopChannel(Channel.BGM); stopChannel(Channel.BGS); stopChannel(Channel.ME); stopChannel(Channel.SE); });
            thread.quitSafely();
        }
        if (Looper.myLooper() != handler.getLooper()) {
            try { thread.join(15000); }
            catch (InterruptedException error) { Thread.currentThread().interrupt(); throw new IllegalStateException("Audio shutdown interrupted", error); }
            if (thread.isAlive()) throw new IllegalStateException("Audio shutdown timed out");
        }
    }
}
