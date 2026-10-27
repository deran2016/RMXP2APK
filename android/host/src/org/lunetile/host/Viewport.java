package org.lunetile.host;

/** Aspect-preserving logical game viewport in pixels. */
public final class Viewport {
    public final int left, top, width, height;

    private Viewport(int left, int top, int width, int height) {
        this.left = left;
        this.top = top;
        this.width = width;
        this.height = height;
    }

    public static Viewport fit(int width, int height, int logicalWidth, int logicalHeight) {
        if (width <= 0 || height <= 0 || logicalWidth <= 0 || logicalHeight <= 0) {
            return new Viewport(0, 0, 0, 0);
        }
        double scale = Math.min((double) width / logicalWidth, (double) height / logicalHeight);
        int gameWidth = Math.max(1, Math.min(width, (int) Math.round(logicalWidth * scale)));
        int gameHeight = Math.max(1, Math.min(height, (int) Math.round(logicalHeight * scale)));
        return new Viewport((width - gameWidth) / 2, (height - gameHeight) / 2, gameWidth, gameHeight);
    }
}
