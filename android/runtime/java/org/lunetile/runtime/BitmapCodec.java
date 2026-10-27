package org.lunetile.runtime;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;
import java.nio.charset.StandardCharsets;

public final class BitmapCodec {
    private BitmapCodec() {}

    public static int[] decode(byte[] encoded) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        BitmapFactory.decodeByteArray(encoded, 0, encoded.length, options);
        long size = (long) options.outWidth * options.outHeight;
        if (options.outWidth <= 0 || options.outHeight <= 0 || size > 32L * 1024 * 1024) return null;
        options.inJustDecodeBounds = false;
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap bitmap = BitmapFactory.decodeByteArray(encoded, 0, encoded.length, options);
        if (bitmap == null) return null;
        try {
            int[] result = new int[(int) size + 2];
            result[0] = bitmap.getWidth();
            result[1] = bitmap.getHeight();
            bitmap.getPixels(result, 2, bitmap.getWidth(), 0, 0, bitmap.getWidth(), bitmap.getHeight());
            return result;
        } finally { bitmap.recycle(); }
    }

    public static int[] text(int[] pixels, byte[] utf8, byte[] fontName, int[] options) {
        if (options.length != 12 || options[0] <= 0 || options[1] <= 0 ||
            (long) options[0] * options[1] != pixels.length) return null;
        String text = new String(utf8, StandardCharsets.UTF_8);
        String name = new String(fontName, StandardCharsets.UTF_8);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        int style = (options[3] != 0 ? Typeface.BOLD : 0) | (options[4] != 0 ? Typeface.ITALIC : 0);
        paint.setTypeface(Typeface.create(name, style));
        paint.setTextSize(options[2]);
        paint.setColor(options[5]);
        Paint.FontMetrics metrics = paint.getFontMetrics();
        int measuredWidth = (int) Math.ceil(paint.measureText(text));
        int measuredHeight = (int) Math.ceil(metrics.descent - metrics.ascent);
        if (options[11] != 0) return new int[] {measuredWidth, measuredHeight};
        Bitmap bitmap = Bitmap.createBitmap(options[0], options[1], Bitmap.Config.ARGB_8888);
        try {
            bitmap.setPixels(pixels, 0, options[0], 0, 0, options[0], options[1]);
            Canvas canvas = new Canvas(bitmap);
            canvas.clipRect(options[6], options[7], (float) options[6] + options[8], (float) options[7] + options[9]);
            float x = options[6];
            if (options[10] == 1) x += (options[8] - measuredWidth) / 2.0f;
            else if (options[10] == 2) x += options[8] - measuredWidth;
            float baseline = options[7] + (options[9] - metrics.ascent - metrics.descent) / 2.0f;
            canvas.drawText(text, x, baseline, paint);
            int[] result = new int[pixels.length + 2];
            result[0] = measuredWidth;
            result[1] = measuredHeight;
            bitmap.getPixels(result, 2, options[0], 0, 0, options[0], options[1]);
            return result;
        } finally { bitmap.recycle(); }
    }
}
