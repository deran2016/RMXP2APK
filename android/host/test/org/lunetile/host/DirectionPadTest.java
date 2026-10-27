package org.lunetile.host;

public final class DirectionPadTest {
    private static void check(boolean value) { if (!value) throw new AssertionError(); }
    public static void main(String[] args) {
        check(DirectionPad.direction(0, 0, 20) == DirectionPad.NONE);
        check(DirectionPad.direction(5, 10, 20) == DirectionPad.NONE);
        check(DirectionPad.direction(100, 5, 20) == TouchInputState.RIGHT);
        check(DirectionPad.direction(-100, 5, 20) == TouchInputState.LEFT);
        check(DirectionPad.direction(5, -100, 20) == TouchInputState.UP);
        check(DirectionPad.direction(5, 100, 20) == TouchInputState.DOWN);
        TouchInputState state = new TouchInputState();
        state.setTouch(TouchInputState.LEFT, true);
        state.setTouch(TouchInputState.LEFT, false);
        state.cancelTouch(TouchInputState.LEFT);
        state.setTouch(TouchInputState.UP, true);
        state.update(1);
        check(state.dir4() == 8 && !state.press(4));
        state.cancelTouch(TouchInputState.UP);
        state.update(1);
        check(state.dir4() == 0);
        System.out.println("Direction pad tests passed");
    }
}
