"""Generate short synthetic WAV and OGG tones in a new test directory."""
import math
import subprocess
import sys
import wave
from pathlib import Path

out = Path(sys.argv[1])
ffmpeg = Path(sys.argv[2])
out.mkdir()
wav = out / 'tone.wav'
with wave.open(str(wav), 'wb') as stream:
    stream.setnchannels(1)
    stream.setsampwidth(2)
    stream.setframerate(22050)
    samples = (int(8000 * math.sin(2 * math.pi * 440 * i / 22050)).to_bytes(2, 'little', signed=True) for i in range(8820))
    stream.writeframes(b''.join(samples))
subprocess.run([str(ffmpeg), '-nostdin', '-v', 'error', '-i', str(wav), '-c:a', 'libvorbis', str(out / 'tone.ogg')], check=True)
(out / 'tone.mid').write_bytes(bytes.fromhex('4d546864000000060000000101e04d54726b0000001400ff510307a12000903c648360803c0000ff2f00'))
