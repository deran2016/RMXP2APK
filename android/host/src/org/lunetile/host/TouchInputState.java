package org.lunetile.host;

/** Thread-safe touch state adapter; a game runtime must explicitly call update once per Input.update. */
public final class TouchInputState {
    public static final int UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3, CONFIRM = 4, CANCEL = 5;
    private static final int[] RGSS = {8, 2, 4, 6, 13, 12};
    private final boolean[] down = new boolean[30], pending = new boolean[30];
    private final boolean[] held = new boolean[30], trigger = new boolean[30], repeat = new boolean[30];
    private final long[] duration = new long[30];

    public synchronized void setTouch(int touch, boolean pressed) {
        if (touch < 0 || touch >= RGSS.length) throw new IllegalArgumentException("Unknown touch control");
        int id = RGSS[touch];
        if (pressed && !down[id]) pending[id] = true;
        down[id] = pressed;
    }

    public synchronized void clearTouches() {
        for (int i = 0; i < down.length; i++) { down[i] = false; pending[i] = false; }
    }

    public synchronized void cancelTouch(int touch) {
        if (touch < 0 || touch >= RGSS.length) throw new IllegalArgumentException("Unknown touch control");
        int id = RGSS[touch];
        down[id] = false;
        pending[id] = false;
    }

    public synchronized void update(int scale) {
        if (scale != 1 && scale != 3) throw new IllegalArgumentException("Expected XP timing scale 1 or 3");
        for (int i = 0; i < held.length; i++) {
            boolean state = down[i] || pending[i];
            trigger[i] = state && !held[i];
            held[i] = state;
            duration[i] = state ? Math.min(duration[i] + 1, 0xffff_ffffL) : 0;
            long first = 16L * scale + 1;
            repeat[i] = trigger[i] || (duration[i] >= first && (duration[i] - first) % (4L * scale) == 0);
            pending[i] = false;
        }
    }

    private void check(int id) { if (id < 0 || id >= held.length) throw new IllegalArgumentException("Unknown input button"); }
    public synchronized boolean press(int id) { check(id); return held[id]; }
    public synchronized boolean trigger(int id) { check(id); return trigger[id]; }
    public synchronized boolean repeat(int id) { check(id); return repeat[id]; }
    public synchronized int dir4() {
        boolean d = held[2], u = held[8], l = held[4], r = held[6];
        if (d && u) d = u = false;
        if (l && r) l = r = false;
        return d ? 2 : l ? 4 : r ? 6 : u ? 8 : 0;
    }
    public synchronized int dir8() {
        int x = (held[6] ? 1 : 0) - (held[4] ? 1 : 0);
        int y = (held[2] ? 1 : 0) - (held[8] ? 1 : 0);
        return x == 0 && y == 0 ? 0 : 5 + x - y * 3;
    }
}
