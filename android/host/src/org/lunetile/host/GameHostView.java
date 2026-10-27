package org.lunetile.host;

import android.content.Context;
import android.graphics.Color;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;

/** UI host only: rendering and input callbacks require a separate game runtime. */
public final class GameHostView extends FrameLayout {
    public interface InputListener {
        void onInput(int key, boolean pressed);
        default void onTouchCancelled(int key) {}
        default void onAllTouchesCancelled() {}
    }

    public static final int UP = TouchInputState.UP, DOWN = TouchInputState.DOWN, LEFT = TouchInputState.LEFT,
            RIGHT = TouchInputState.RIGHT, CONFIRM = TouchInputState.CONFIRM, CANCEL = TouchInputState.CANCEL;
    private final SurfaceView surface;
    private final Button[] buttons = new Button[6];
    private final boolean[] pressed = new boolean[6];
    private final int logicalWidth, logicalHeight;
    private InputListener listener;
    private int directionPointer = -1, activeDirection = DirectionPad.NONE;
    private View directionOrigin;
    private boolean touchClick;

    public GameHostView(Context context, int logicalWidth, int logicalHeight) {
        super(context);
        if (logicalWidth <= 0 || logicalHeight <= 0) throw new IllegalArgumentException("Invalid logical size");
        this.logicalWidth = logicalWidth;
        this.logicalHeight = logicalHeight;
        setBackgroundColor(Color.BLACK);
        setMotionEventSplittingEnabled(true);
        surface = new SurfaceView(context);
        addView(surface, new LayoutParams(1, 1));
        String[] labels = {"↑", "↓", "←", "→", "결정", "취소"};
        String[] descriptions = {"위", "아래", "왼쪽", "오른쪽", "결정", "취소"};
        for (int i = 0; i < buttons.length; i++) {
            final int key = i;
            Button button = new Button(context);
            button.setText(labels[i]);
            button.setContentDescription(descriptions[i]);
            button.setAlpha(0.7f);
            button.setOnClickListener(view -> { if (!touchClick) { setPressed(key, true); setPressed(key, false); } });
            button.setOnTouchListener((view, event) -> {
                if (key <= RIGHT) return onDirectionTouch(view, event);
                int action = event.getActionMasked();
                if (action == MotionEvent.ACTION_DOWN) setPressed(key, true);
                else if (action == MotionEvent.ACTION_UP) view.performClick();
                else if (action == MotionEvent.ACTION_CANCEL) {
                    setPressed(key, false);
                    if (listener != null) listener.onTouchCancelled(key);
                }
                return true;
            });
            buttons[i] = button;
            addView(button, new LayoutParams(1, 1));
        }
    }

    public SurfaceHolder getGameSurface() { return surface.getHolder(); }
    public void setInputListener(InputListener value) { releaseAll(); listener = value; }

    private boolean onDirectionTouch(View view, MotionEvent event) {
        int action = event.getActionMasked();
        int index = event.getActionIndex();
        if (action == MotionEvent.ACTION_DOWN && directionPointer == -1) {
            directionPointer = event.getPointerId(index);
            directionOrigin = view;
            moveDirection(view, event, index);
        } else if (view != directionOrigin) {
            return true;
        } else if (action == MotionEvent.ACTION_MOVE && directionPointer != -1) {
            index = event.findPointerIndex(directionPointer);
            if (index >= 0) moveDirection(view, event, index);
            else finishDirection(true);
        } else if ((action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP)
                && event.getPointerId(index) == directionPointer) {
            finishDirection(false);
            touchClick = true;
            try { view.performClick(); } finally { touchClick = false; }
        } else if (action == MotionEvent.ACTION_CANCEL && directionPointer != -1) {
            finishDirection(true);
        }
        return true;
    }

    private void moveDirection(View origin, MotionEvent event, int index) {
        float centerX = (buttons[LEFT].getRight() + buttons[RIGHT].getLeft()) / 2f;
        float centerY = (buttons[UP].getBottom() + buttons[DOWN].getTop()) / 2f;
        float x = origin.getLeft() + event.getX(index) - centerX;
        float y = origin.getTop() + event.getY(index) - centerY;
        int next = DirectionPad.direction(x, y, buttons[UP].getWidth() * 0.2f);
        if (next == activeDirection) return;
        if (activeDirection != DirectionPad.NONE) {
            int old = activeDirection;
            setPressed(old, false);
            if (listener != null) listener.onTouchCancelled(old);
        }
        activeDirection = next;
        if (next != DirectionPad.NONE) setPressed(next, true);
    }

    private void finishDirection(boolean cancelled) {
        directionPointer = -1;
        directionOrigin = null;
        int old = activeDirection;
        activeDirection = DirectionPad.NONE;
        if (old == DirectionPad.NONE) return;
        setPressed(old, false);
        if (cancelled && listener != null) listener.onTouchCancelled(old);
    }

    private void setPressed(int key, boolean value) {
        if (pressed[key] == value) return;
        pressed[key] = value;
        buttons[key].setPressed(value);
        if (listener != null) listener.onInput(key, value);
    }

    public void releaseAll() {
        directionPointer = -1;
        directionOrigin = null;
        activeDirection = DirectionPad.NONE;
        for (int key = 0; key < pressed.length; key++) setPressed(key, false);
        if (listener != null) listener.onAllTouchesCancelled();
    }

    @Override protected void onDetachedFromWindow() {
        releaseAll();
        super.onDetachedFromWindow();
    }

    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (!focused) releaseAll();
    }

    @Override protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        Viewport v = Viewport.fit(width, height, logicalWidth, logicalHeight);
        int size = Math.max(1, Math.min(v.width / 6, v.height / 5));
        int gap = Math.max(1, size / 10);
        int marginTop = Math.round(getResources().getDisplayMetrics().density * 32);
        boolean below = height - marginTop - v.height >= 3 * size + 2 * gap;
        int sideSize = Math.min(size, Math.max(0, (v.left - gap) / 3));
        boolean sides = !below && sideSize >= Math.round(getResources().getDisplayMetrics().density * 48)
                && width - (v.left + v.width) >= sideSize * 2 + gap * 2;
        int top = below ? marginTop : v.top;
        position(surface, v.left, top, v.width, v.height);
        if (sides) { size = sideSize; gap = Math.max(1, size / 10); }
        int x = sides ? gap : v.left + gap;
        int y = below ? top + v.height + gap + size : top + v.height - size * 2 - gap;
        position(buttons[UP], x + size, y - size, size, size);
        position(buttons[DOWN], x + size, y + size, size, size);
        position(buttons[LEFT], x, y, size, size);
        position(buttons[RIGHT], x + size * 2, y, size, size);
        int actionX = sides ? v.left + v.width + gap : v.left + v.width - 2 * size - 2 * gap;
        position(buttons[CONFIRM], actionX, y + size, size, size);
        position(buttons[CANCEL], actionX + size + gap, y, size, size);
        // Layout parameters change during the initial layout; schedule a fresh measurement.
        post(this::requestLayout);
    }

    private void position(View view, int x, int y, int width, int height) {
        LayoutParams params = new LayoutParams(width, height);
        params.leftMargin = x;
        params.topMargin = y;
        view.setLayoutParams(params);
    }
}
