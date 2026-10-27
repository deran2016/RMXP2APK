package org.lunetile.probe;

public final class ProbeMain {
    private static native String runRubyProbe();

    public static void main(String[] args) {
        System.load(args[0]);
        String result = runRubyProbe();
        System.out.println(result);
        if (!result.startsWith("PASS:")) System.exit(1);
    }
}
