import struct, pathlib
for name in ["AreaTexDX10.dds", "SearchTex.dds"]:
    p = pathlib.Path(__file__).with_name(name)
    b = p.read_bytes()
    assert b[:4] == b"DDS "
    h = b[4:128]
    hsize, flags, height, width, pitch, depth, mips = struct.unpack_from("<7I", h, 0)
    pf_size, pf_flags, fourcc = struct.unpack_from("<2I4s", h, 72)
    rgb_bits = struct.unpack_from("<I", h, 88)[0]
    print(name, "file", len(b), "wh", width, height, "mips", mips, "pitch", pitch, "fourcc", fourcc, "rgb_bits", rgb_bits, "pf_flags", hex(pf_flags))
    off = 128
    if fourcc == b"DX10":
        fmt, dim, misc, arraysize, misc2 = struct.unpack_from("<5I", b, 128)
        print("  dxgi", fmt, "dim", dim, "array", arraysize, "payload", len(b) - 148)
        off = 148
    # dump raw payload size expectations
    print("  payload", len(b) - off)
