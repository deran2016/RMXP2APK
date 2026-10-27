package org.lunetile.host;

import android.content.Context;
import android.content.res.AssetManager;
import org.json.JSONObject;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.Charset;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.UUID;

/** Copies APK game assets into immutable, content-addressed app-private directories. */
public final class GameAssets {
    private GameAssets() {}

    public interface Source {
        String[] list(String path) throws IOException;
        InputStream open(String path) throws IOException;
    }

    public static File prepare(Context context) throws IOException {
        AssetManager assets = context.getAssets();
        return prepare(context.getFilesDir(), new Source() {
            public String[] list(String path) throws IOException { return assets.list(path); }
            public InputStream open(String path) throws IOException { return assets.open(path); }
        });
    }

    public static File prepare(File filesDir, Source source) throws IOException {
        File files = filesDir.getCanonicalFile();
        if (!files.isDirectory()) throw new IOException("App files directory is missing");
        File base = new File(files, "GameAssets");
        Files.createDirectories(base.toPath());
        if (Files.isSymbolicLink(base.toPath()) || !base.getCanonicalFile().getParentFile().equals(files))
            throw new IOException("Invalid asset directory");
        File lockPath = new File(base, ".lock");
        if (Files.isSymbolicLink(lockPath.toPath())) throw new IOException("Invalid lock file");
        try (RandomAccessFile lockFile = new RandomAccessFile(lockPath, "rw");
             FileChannel channel = lockFile.getChannel(); FileLock lock = channel.lock()) {
            File stage = new File(base, ".staging-" + UUID.randomUUID());
            Files.createDirectory(stage.toPath());
            try {
                copyTree(source, "game", stage, "");
                scriptsPath(stage);
                String digest = treeHash(stage);
                File target = new File(base, digest);
                if (Files.exists(target.toPath(), LinkOption.NOFOLLOW_LINKS)) {
                    if (Files.isSymbolicLink(target.toPath()) || !target.isDirectory() || !digest.equals(treeHash(target)))
                        throw new IOException("Existing game assets changed; refusing to overwrite them");
                    return target;
                }
                Files.move(stage.toPath(), target.toPath(), StandardCopyOption.ATOMIC_MOVE);
                return target;
            } finally {
                deleteStage(stage, base);
            }
        }
    }

    private static void copyTree(Source source, String path, File root, String relative) throws IOException {
        String[] names = source.list(path);
        if (names == null) throw new IOException("Cannot list packaged assets: " + path);
        if (names.length == 0) {
            if (relative.isEmpty()) throw new IOException("Packaged game assets are missing");
            File out = checked(root, relative);
            try (InputStream in = source.open(path); FileOutputStream file = new FileOutputStream(out)) {
                byte[] buffer = new byte[16384];
                int count;
                while ((count = in.read(buffer)) != -1) file.write(buffer, 0, count);
                file.flush();
                file.getFD().sync();
            }
            return;
        }
        Arrays.sort(names);
        for (String name : names) {
            if (name.isEmpty() || name.equals(".") || name.equals("..") || name.contains("/") || name.contains("\\") || name.indexOf('\0') >= 0)
                throw new IOException("Invalid packaged asset name");
            String child = relative.isEmpty() ? name : relative + "/" + name;
            String[] sub = source.list(path + "/" + name);
            if (sub == null) throw new IOException("Cannot inspect packaged asset: " + child);
            if (sub.length > 0) Files.createDirectory(checked(root, child).toPath());
            copyTree(source, path + "/" + name, root, child);
        }
    }

    private static File checked(File root, String relative) throws IOException {
        File file = new File(root, relative).getCanonicalFile();
        if (!file.getPath().startsWith(root.getCanonicalPath() + File.separator)) throw new IOException("Asset path escapes game root");
        return file;
    }

    public static String scriptsPath(File root) throws IOException {
        File ini = checked(root, "Game.ini"), metadata = checked(root, "android-build.json");
        if (!ini.isFile() || !metadata.isFile() || !checked(root, "Data").isDirectory())
            throw new IOException("Required Game.ini, build metadata or Data directory is missing");
        if (Files.size(ini.toPath()) > 1024 * 1024 || Files.size(metadata.toPath()) > 1024 * 1024)
            throw new IOException("Game configuration exceeds 1 MiB");
        String encoding;
        try {
            String json = StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT).onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(Files.readAllBytes(metadata.toPath()))).toString();
            JSONObject config = new JSONObject(json);
            encoding = config.isNull("ini_encoding") ? "UTF-8" : config.getString("ini_encoding");
            if (!Arrays.asList("UTF-8", "CP949", "CP932").contains(encoding)) throw new IOException("Invalid INI encoding");
        } catch (org.json.JSONException | CharacterCodingException error) { throw new IOException("Invalid build metadata", error); }
        byte[] bytes = Files.readAllBytes(ini.toPath());
        int offset = 0;
        if (bytes.length >= 2 && (bytes[0] & 255) == 0xff && (bytes[1] & 255) == 0xfe) { encoding = "UTF-16LE"; offset = 2; }
        else if (bytes.length >= 2 && (bytes[0] & 255) == 0xfe && (bytes[1] & 255) == 0xff) { encoding = "UTF-16BE"; offset = 2; }
        else if (bytes.length >= 3 && (bytes[0] & 255) == 0xef && (bytes[1] & 255) == 0xbb && (bytes[2] & 255) == 0xbf) { encoding = "UTF-8"; offset = 3; }
        String text;
        try { text = Charset.forName(encoding).newDecoder().onMalformedInput(CodingErrorAction.REPORT).onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes, offset, bytes.length-offset)).toString(); }
        catch (CharacterCodingException error) { throw new IOException("Invalid Game.ini text", error); }
        boolean game = false;
        String scripts = null;
        for (String line : text.split("\\r?\\n")) {
            String item = line.trim();
            if (item.startsWith("[") && item.endsWith("]")) game = item.substring(1, item.length()-1).trim().equalsIgnoreCase("Game");
            else if (game && !item.startsWith(";") && !item.startsWith("#")) {
                int equals = item.indexOf('=');
                if (equals >= 0 && item.substring(0, equals).trim().equalsIgnoreCase("Scripts")) scripts = item.substring(equals+1).trim().replace('\\', '/');
            }
        }
        if (scripts == null || !scripts.startsWith("Data/") || scripts.contains("/../") || scripts.endsWith("/..") || scripts.indexOf('\0') >= 0 || !checked(root, scripts).isFile())
            throw new IOException("Game.ini references a missing or unsafe Scripts file");
        return scripts;
    }

    private static String treeHash(File root) throws IOException {
        MessageDigest digest;
        try { digest = MessageDigest.getInstance("SHA-256"); } catch (NoSuchAlgorithmException error) { throw new AssertionError(error); }
        hashTree(root, root, digest);
        StringBuilder text = new StringBuilder(64);
        for (byte value : digest.digest()) text.append(String.format(java.util.Locale.ROOT, "%02x", value & 255));
        return text.toString();
    }

    private static void hashTree(File root, File dir, MessageDigest digest) throws IOException {
        File[] files = dir.listFiles();
        if (files == null) throw new IOException("Cannot inspect game assets");
        Arrays.sort(files, (a, b) -> a.getName().compareTo(b.getName()));
        for (File file : files) {
            if (Files.isSymbolicLink(file.toPath())) throw new IOException("Symbolic link in game assets");
            if (file.isDirectory()) { hashTree(root, file, digest); continue; }
            if (!file.isFile()) throw new IOException("Unsupported game asset");
            String relative = root.toPath().relativize(file.toPath()).toString().replace(File.separatorChar, '/');
            byte[] name = relative.getBytes(StandardCharsets.UTF_8);
            digest.update(ByteBuffer.allocate(4).putInt(name.length).array());
            digest.update(name);
            MessageDigest content;
            try { content = MessageDigest.getInstance("SHA-256"); } catch (NoSuchAlgorithmException error) { throw new AssertionError(error); }
            try (InputStream in = new FileInputStream(file)) {
                byte[] buffer = new byte[16384]; int count;
                while ((count = in.read(buffer)) != -1) content.update(buffer, 0, count);
            }
            digest.update(content.digest());
        }
    }

    private static void deleteStage(File stage, File base) throws IOException {
        if (!stage.exists() || !stage.getParentFile().equals(base)) return;
        if (Files.isSymbolicLink(stage.toPath())) throw new IOException("Invalid staging directory");
        File[] children = stage.listFiles();
        if (children == null) throw new IOException("Cannot clean staging directory");
        for (File child : children) {
            if (child.isDirectory() && !Files.isSymbolicLink(child.toPath())) deleteStage(child, stage);
            else Files.delete(child.toPath());
        }
        Files.delete(stage.toPath());
    }
}
