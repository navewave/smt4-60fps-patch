"""Usage: python tools/cadence.py CLIP.mp4 [GX GY [cx,cy]]
Per-region update cadence of the 3DS top screen in an OBS clip.
For each grid cell: frame-to-frame diffs d[i]. In motion, a 60 Hz region changes every
frame; a 30 Hz one alternates (change, ~0). Prints per cell the share of 'motion pairs'
where one diff is near zero (30 Hz signature), plus a timeline of the global pattern."""
import subprocess, sys, numpy as np
import shutil, glob, os
FF = shutil.which("ffmpeg") or (glob.glob(os.path.expandvars(r"%LOCALAPPDATA%/Microsoft/WinGet/Packages/Gyan.FFmpeg*/ffmpeg-*/bin/ffmpeg.exe")) or ["ffmpeg"])[0]
path = sys.argv[1]
W, H = 400, 240
GX, GY = int(sys.argv[2]) if len(sys.argv) > 2 else 10, int(sys.argv[3]) if len(sys.argv) > 3 else 6
p = subprocess.run([FF, '-v', 'error', '-c:v', 'libdav1d', '-i', path, '-vf',
                    f'crop=1200:720:94:171,scale={W}:{H}:flags=area,format=gray', '-f', 'rawvideo', '-'],
                   capture_output=True, check=True)
fr = np.frombuffer(p.stdout, np.uint8).reshape(-1, H, W).astype(np.float32)
n = len(fr)
d = np.abs(fr[1:] - fr[:-1])                      # n-1 x H x W
cw, ch = W // GX, H // GY
cell = d[:, :ch * GY, :cw * GX].reshape(n - 1, GY, ch, GX, cw).mean((2, 4))   # n-1 x GY x GX
THR = 0.6                                          # mean abs diff counted as "changed"
print(f'{n} frames; grid {GX}x{GY} (cells {cw}x{ch} px). Cell = 30Hz-share% / moving-frames')
for y in range(GY):
    row = []
    for x in range(GX):
        s = cell[:, y, x]
        mv = s > THR
        pairs = [(s[i], s[i + 1]) for i in range(len(s) - 1) if max(s[i], s[i + 1]) > THR * 2]
        z = sum(1 for a, b in pairs if min(a, b) < 0.25 * max(a, b))
        row.append(f'{(100*z//len(pairs)) if pairs else -1:3d}/{mv.sum():3d}')
    print('  '.join(row))
# timeline over whole screen: 1 char per frame pair
g = cell.reshape(n - 1, -1)
line = ''.join('#' if v > 2 * THR else ('+' if v > THR else '.') for v in g.max(1))
print('timeline (max cell diff per frame):')
for i in range(0, len(line), 120): print(f'{i:5d} {line[i:i+120]}')
if len(sys.argv) > 4:   # dump a cell's diff series
    cx, cy = map(int, sys.argv[4].split(','))
    print('cell', cx, cy, ' '.join(f'{v:.1f}' for v in cell[:, cy, cx]))
