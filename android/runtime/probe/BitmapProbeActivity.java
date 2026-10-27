package org.lunetile.probe;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import org.lunetile.runtime.BitmapCodec;
import java.io.ByteArrayOutputStream;

public final class BitmapProbeActivity extends Activity implements SurfaceHolder.Callback {
    static { System.loadLibrary("bitmap_probe"); }
    private static native String runBitmapProbe(Class<?> codec, Surface surface, byte[] png, byte[] graphics, byte[] tilemap);
    private byte[] readAsset(String path) throws java.io.IOException {
        try (java.io.InputStream input = getAssets().open(path)) {
            ByteArrayOutputStream source = new ByteArrayOutputStream();
            byte[] chunk = new byte[4096];
            for (int count; (count = input.read(chunk)) != -1;) source.write(chunk, 0, count);
            return source.toByteArray();
        }
    }
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        SurfaceView view = new SurfaceView(this);
        view.getHolder().addCallback(this);
        setContentView(view);
    }
    @Override public void surfaceCreated(SurfaceHolder holder) {
        Bitmap image = Bitmap.createBitmap(2, 2, Bitmap.Config.ARGB_8888);
        image.setPixels(new int[] {0xffff0000, 0xffffffff, 0xffffffff, 0xff0000ff}, 0, 2, 0, 0, 2, 2);
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        image.compress(Bitmap.CompressFormat.PNG, 100, bytes);
        image.recycle();
        byte[] graphics, tilemap;
        try {
            graphics = readAsset("graphics.rb");
            tilemap = readAsset("tilemap.rb");
        } catch (java.io.IOException error) {
            Log.e("LunetileBitmapProbe", "FAIL: graphics source", error);
            return;
        }
        String result = runBitmapProbe(BitmapCodec.class, holder.getSurface(), bytes.toByteArray(), graphics, tilemap);
        Log.i("LunetileBitmapProbe", result);
    }
    @Override public void surfaceChanged(SurfaceHolder h, int f, int w, int height) {}
    @Override public void surfaceDestroyed(SurfaceHolder h) {}
}
