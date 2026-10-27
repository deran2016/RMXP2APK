package org.lunetile.host;

public final class ViewportTest {
    public static void main(String[] args) {
        check(1920, 1080, 640, 480, 240, 0, 1440, 1080);
        check(1080, 1920, 640, 480, 0, 555, 1080, 810);
        check(640, 480, 640, 480, 0, 0, 640, 480);
        check(0, 480, 640, 480, 0, 0, 0, 0);
        System.out.println("Viewport tests passed");
    }
    private static void check(int w, int h, int lw, int lh, int x, int y, int ew, int eh) {
        Viewport v = Viewport.fit(w, h, lw, lh);
        if (v.left != x || v.top != y || v.width != ew || v.height != eh) throw new AssertionError("viewport mismatch");
    }
}
