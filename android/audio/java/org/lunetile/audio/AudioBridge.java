package org.lunetile.audio;

import org.lunetile.host.GameAudio;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicBoolean;

/** Synchronous RGSS call boundary over the separate asynchronous playback prototype. */
public final class AudioBridge implements AutoCloseable {
    private static final GameAudio.Channel[] CHANNELS = {GameAudio.Channel.BGM, GameAudio.Channel.BGS, GameAudio.Channel.ME, GameAudio.Channel.SE};
    private final File root;
    private final GameAudio audio;
    private final AtomicReference<String> error = new AtomicReference<>();

    public AudioBridge(File root) throws IOException {
        this.root = root.getCanonicalFile();
        if (!this.root.isDirectory()) throw new IOException("Game resource directory is missing");
        audio = new GameAudio(new GameAudio.Listener() {
            @Override public void onStarted(GameAudio.Channel channel) { }
            @Override public void onStopped(GameAudio.Channel channel) { }
            @Override public void onError(GameAudio.Channel channel, String message) { error.compareAndSet(null, channel + ": " + message); }
        });
    }

    private GameAudio.Channel channel(int id) {
        if (id < 0 || id >= CHANNELS.length) throw new IllegalArgumentException("Unknown audio channel");
        return CHANNELS[id];
    }

    private File resolve(byte[] utf8) throws IOException {
        String name = new String(utf8, StandardCharsets.UTF_8).replace('\\', '/');
        if (name.isEmpty() || name.indexOf('\0') >= 0 || name.startsWith("/") || name.contains(":")) throw new IOException("Invalid audio path");
        File file = new File(root, name).getCanonicalFile();
        if (!file.getPath().startsWith(root.getPath() + File.separator)) throw new IOException("Audio path leaves the project");
        String lower = name.toLowerCase(Locale.ROOT);
        if (lower.endsWith(".ogg") || lower.endsWith(".wav") || lower.endsWith(".mid") || lower.endsWith(".midi")) {
            if (file.isFile()) return file;
        } else if (!new File(name).getName().contains(".")) {
            for (String ext : new String[]{".ogg", ".wav", ".mid", ".midi"}) {
                File candidate = new File(file.getPath() + ext).getCanonicalFile();
                if (!candidate.getPath().startsWith(root.getPath() + File.separator)) throw new IOException("Audio path leaves the project");
                if (candidate.isFile()) return candidate;
            }
        } else throw new IOException("Unsupported audio format: " + name);
        throw new IOException("Missing audio file: " + name);
    }

    private void checked() throws IOException {
        String message = error.getAndSet(null);
        if (message != null) throw new IOException(message);
    }

    private void flush() throws IOException {
        CountDownLatch finished = new CountDownLatch(1);
        audio.flush(finished::countDown);
        try { if (!finished.await(15, TimeUnit.SECONDS)) throw new IOException("Audio operation timed out"); }
        catch (InterruptedException exception) { Thread.currentThread().interrupt(); throw new IOException("Audio operation interrupted", exception); }
        checked();
    }

    public synchronized void play(int id, byte[] path, int volume, int pitch) throws IOException {
        checked();
        File file = resolve(path);
        audio.play(channel(id), file.getPath(), volume, pitch);
        flush();
    }

    public synchronized void stop(int id) throws IOException {
        checked();
        audio.stop(channel(id));
        flush();
    }

    public synchronized void fade(int id, int milliseconds) throws IOException {
        checked();
        audio.fade(channel(id), milliseconds);
        flush();
    }

    public synchronized boolean isPlaying(int id) throws IOException {
        checked();
        AtomicBoolean playing = new AtomicBoolean();
        audio.status(channel(id), (channel, value, position, duration) -> playing.set(value));
        flush();
        return playing.get();
    }

    public synchronized void suspend() { audio.suspend(); }
    public synchronized void resume() { audio.resume(); }

    @Override public synchronized void close() { audio.close(); }
}
