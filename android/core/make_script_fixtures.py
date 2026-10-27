"""Create fresh synthetic XP script archives for Android core tests."""
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from rxdata import integer, string


def archive(entries):
    parts = [b"\x04\x08[", integer(len(entries))]
    for ident, name, compressed in entries:
        parts.extend((b"[", integer(3), b"i", integer(ident), string(name), string(compressed)))
    return b"".join(parts)


def main():
    output = Path(sys.argv[1])
    encoding = sys.argv[2] if len(sys.argv) > 2 else "UTF-8"
    if encoding not in ("UTF-8", "CP949", "CP932"):
        raise ValueError("unsupported encoding")
    output.mkdir(parents=True, exist_ok=False)
    make = lambda source: zlib.compress(source)
    sample = "あ" if encoding == "CP932" else "가"
    code = f'# coding: {encoding}\n$script_order = [1]; $script_text = "{sample}"\n'.encode(encoding)
    first = (41, sample.encode(encoding), make(code))
    second = (3, b"second", make(b"$script_order << 2; $script_ids = $RGSS_SCRIPTS.map { |e| e[0] }\n"))
    marker = (99, b"marker", make(b"$script_preflight_marker = true\n"))
    cases = {
        "good": [first, second],
        "bad_zlib": [marker, (5, b"bad", b"not-zlib")],
        "trailing_zlib": [marker, (5, b"bad", make(b"true") + b"extra")],
        "invalid_text": [marker, (5, b"bad", make(b"\xff" if encoding == "UTF-8" else b"\x81"))],
    }
    for name, entries in cases.items():
        (output / f"{name}.rxdata").write_bytes(archive(entries))
    (output / "bad_entry.rxdata").write_bytes(b"\x04\x08[" + integer(2) + archive([marker])[4:] + b"[" + integer(1) + b"i" + integer(1))
    (output / "probe.png").write_bytes(b"image bytes")
    print(output)


if __name__ == "__main__":
    main()
