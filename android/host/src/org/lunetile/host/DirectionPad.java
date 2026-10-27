package org.lunetile.host;

/** Converts a finger position around the center of the directional pad to one XP direction. */
public final class DirectionPad {
    public static final int NONE = -1;

    public static int direction(float x, float y, float deadZone) {
        if (x * x + y * y < deadZone * deadZone) return NONE;
        if (Math.abs(x) > Math.abs(y)) return x < 0 ? TouchInputState.LEFT : TouchInputState.RIGHT;
        return y < 0 ? TouchInputState.UP : TouchInputState.DOWN;
    }
}
