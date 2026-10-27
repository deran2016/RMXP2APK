package org.lunetile.host;

public final class TouchInputStateTest {
    private static void check(boolean value) { if (!value) throw new AssertionError(); }
    public static void main(String[] args) {
        TouchInputState state = new TouchInputState();
        state.setTouch(TouchInputState.CONFIRM, true);
        state.update(1);
        check(state.press(13) && state.trigger(13) && state.repeat(13));
        for (int frame = 2; frame <= 16; frame++) { state.update(1); check(!state.repeat(13) && !state.trigger(13)); }
        state.update(1); check(state.repeat(13));
        for (int frame = 18; frame <= 20; frame++) { state.update(1); check(!state.repeat(13)); }
        state.update(1); check(state.repeat(13));
        state.setTouch(TouchInputState.CONFIRM, false);
        state.update(1); check(!state.press(13) && !state.repeat(13));
        state.setTouch(TouchInputState.CANCEL, true);
        state.setTouch(TouchInputState.CANCEL, false);
        state.update(1); check(state.press(12) && state.trigger(12));
        state.update(1); check(!state.press(12));
        state.setTouch(TouchInputState.UP, true);
        state.setTouch(TouchInputState.RIGHT, true);
        state.update(1); check(state.dir4() == 6 && state.dir8() == 9);
        state.setTouch(TouchInputState.DOWN, true);
        state.update(1); check(state.dir4() == 6 && state.dir8() == 6);
        state.setTouch(TouchInputState.LEFT, true);
        state.update(1); check(state.dir4() == 0 && state.dir8() == 0);
        state.clearTouches(); state.update(1); check(state.dir4() == 0);
        state.setTouch(TouchInputState.CANCEL, true);
        state.clearTouches(); state.update(1); check(!state.press(12));
        state.setTouch(TouchInputState.CANCEL, true);
        state.cancelTouch(TouchInputState.CANCEL); state.update(1); check(!state.trigger(12));
        state.setTouch(TouchInputState.CONFIRM, true);
        for (int frame = 1; frame < 49; frame++) { state.update(3); if (frame > 1) check(!state.repeat(13)); }
        state.update(3); check(state.repeat(13));
        System.out.println("Touch input tests passed");
    }
}
