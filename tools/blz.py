"""Decompress a 3DS ExeFS .code (backwards LZ, as ctrtool / 3dstool).
    python tools/blz.py <compressed code.bin> <output decompressed_code.bin>"""
import struct, sys

def decompress(comp):
    size = len(comp)
    top_bottom, extra = struct.unpack_from('<II', comp, size - 8)
    out = bytearray(size + extra)
    out[:size] = comp
    o = len(out)
    i = size - ((top_bottom >> 24) & 0xFF)
    stop = size - (top_bottom & 0xFFFFFF)
    while i > stop:
        i -= 1
        ctl = comp[i]
        for _ in range(8):
            if i <= stop or i <= 0 or o <= 0: break
            if ctl & 0x80:
                i -= 2
                seg = comp[i] | comp[i + 1] << 8
                n = ((seg >> 12) & 15) + 3
                off = (seg & 0x0FFF) + 2
                if o < n: raise ValueError('corrupt stream (segment)')
                for _ in range(n):
                    if o + off >= len(out): raise ValueError('corrupt stream (offset)')
                    b = out[o + off]                 # read before stepping back (as ctrtool)
                    o -= 1
                    out[o] = b
            else:
                o -= 1; i -= 1
                out[o] = comp[i]
            ctl = (ctl << 1) & 0xFF
    return bytes(out)

if __name__ == '__main__':
    data = open(sys.argv[1], 'rb').read()
    out = decompress(data)
    open(sys.argv[2], 'wb').write(out)
    print(f'{len(data):#x} -> {len(out):#x} bytes')
