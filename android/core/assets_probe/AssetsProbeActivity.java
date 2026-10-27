package org.lunetile.assetdiagnostic20261027;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;
import android.content.res.AssetManager;
import org.lunetile.host.GameAssets;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Arrays;

public final class AssetsProbeActivity extends Activity {
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        String result;
        try { verify(); result = "PASS assets preserve updates saves and reject unsafe input"; }
        catch (Exception error) { result = "FAIL " + error; }
        Log.i("LunetileAssetsProbe", result);
        TextView text = new TextView(this); text.setText(result); setContentView(text);
    }

    private void verify() throws Exception {
        File saves = new File(getFilesDir(), "Saves/RGSSXP");
        if (!saves.mkdirs()) throw new IOException("Could not make save fixture");
        File sentinel = new File(saves, "Save1.rxdata");
        Files.write(sentinel.toPath(), new byte[]{1, 2, 3});
        File first = GameAssets.prepare(this);
        if (!GameAssets.scriptsPath(first).equals("Data/Scripts.rxdata")) throw new IOException("Wrong scripts path");
        if (!first.equals(GameAssets.prepare(this))) throw new IOException("Same assets changed directory");
        File script = new File(first, "Data/Scripts.rxdata");
        byte[] original = Files.readAllBytes(script.toPath());
        AssetManager assets = getAssets();
        GameAssets.Source base = new GameAssets.Source() {
            public String[] list(String path) throws IOException { return assets.list(path); }
            public InputStream open(String path) throws IOException { return assets.open(path); }
        };
        GameAssets.Source updated = new GameAssets.Source() {
            public String[] list(String path) throws IOException { return base.list(path); }
            public InputStream open(String path) throws IOException {
                if (path.equals("game/Data/Scripts.rxdata")) return new ByteArrayInputStream(new byte[]{4, 8, 0});
                return base.open(path);
            }
        };
        File second = GameAssets.prepare(getFilesDir(), updated);
        if (first.equals(second) || !Arrays.equals(original, Files.readAllBytes(script.toPath()))) throw new IOException("Original version changed");
        if (!Arrays.equals(Files.readAllBytes(sentinel.toPath()), new byte[]{1, 2, 3})) throw new IOException("Save changed");
        for (String encoding : new String[]{"CP949", "CP932"}) {
            GameAssets.Source encoded = new GameAssets.Source() {
                public String[] list(String path) throws IOException { return base.list(path); }
                public InputStream open(String path) throws IOException {
                    if (path.equals("game/android-build.json")) return new ByteArrayInputStream(("{\"ini_encoding\":\"" + encoding + "\"}").getBytes(StandardCharsets.UTF_8));
                    if (path.equals("game/Game.ini")) return new ByteArrayInputStream(("[Game]\nTitle=" + (encoding.equals("CP949") ? "가" : "あ") + "\nScripts=Data/Scripts.rxdata\n").getBytes(encoding));
                    return base.open(path);
                }
            };
            if (!GameAssets.scriptsPath(GameAssets.prepare(getFilesDir(), encoded)).equals("Data/Scripts.rxdata")) throw new IOException("Invalid encoded INI");
        }
        GameAssets.Source traversal = new GameAssets.Source() {
            public String[] list(String path) { return new String[]{".."}; }
            public InputStream open(String path) { return new ByteArrayInputStream(new byte[0]); }
        };
        try { GameAssets.prepare(getFilesDir(), traversal); throw new IOException("Traversal accepted"); }
        catch (IOException expected) { if (expected.getMessage().equals("Traversal accepted")) throw expected; }
        GameAssets.Source missing = new GameAssets.Source() {
            public String[] list(String path) throws IOException { return base.list(path); }
            public InputStream open(String path) throws IOException {
                if (path.equals("game/Game.ini")) return new ByteArrayInputStream("[Game]\nScripts=Data/Missing.rxdata\n".getBytes(StandardCharsets.UTF_8));
                return base.open(path);
            }
        };
        try { GameAssets.prepare(getFilesDir(), missing); throw new IOException("Missing script accepted"); }
        catch (IOException expected) { if (expected.getMessage().equals("Missing script accepted")) throw expected; }
        GameAssets.Source invalidMetadata = new GameAssets.Source() {
            public String[] list(String path) throws IOException { return base.list(path); }
            public InputStream open(String path) throws IOException {
                if (path.equals("game/android-build.json")) return new ByteArrayInputStream(new byte[]{(byte)0xff});
                return base.open(path);
            }
        };
        try { GameAssets.prepare(getFilesDir(), invalidMetadata); throw new IOException("Invalid metadata accepted"); }
        catch (IOException expected) { if (expected.getMessage().equals("Invalid metadata accepted")) throw expected; }
        Files.write(script.toPath(), new byte[]{9});
        try { GameAssets.prepare(this); throw new IOException("Changed assets accepted"); }
        catch (IOException expected) { if (expected.getMessage().equals("Changed assets accepted")) throw expected; }
        if (!Arrays.equals(Files.readAllBytes(script.toPath()), new byte[]{9})) throw new IOException("Changed asset was overwritten");
        if (!Arrays.equals(Files.readAllBytes(sentinel.toPath()), new byte[]{1, 2, 3})) throw new IOException("Save changed");
        File[] leftovers = new File(getFilesDir(), "GameAssets").listFiles((dir, name) -> name.startsWith(".staging-"));
        if (leftovers == null || leftovers.length != 0) throw new IOException("Staging directory left behind");
    }
}
