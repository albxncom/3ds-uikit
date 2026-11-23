"""Makes the menu music's file, or turns one back into a WAV to listen to.

  python3 -I wav2bcstm.py IN.wav OUT.bcstm [--loop START END | --no-loop]
  python3 -I wav2bcstm.py --decode IN.bcstm OUT.wav [--turns N]

IN.wav: 16-bit PCM, mono or stereo, at the rate it is to play at (22050 Hz mono keeps the application small: about
11 KB a second). The whole file loops unless --loop gives the first and one past the last sample of the part that
repeats (the samples before START play once), or --no-loop. --decode writes what the console plays: the stream
once, then the looped part N more times (default 1), so the seam can be heard.

The file is a BCSTM as the 3DS reads it (3dbrew, "BCSTM"): DSP-ADPCM, 4 bits a sample in frames of 14 samples, each
with one of eight predictors and a scale. The predictors here are a fixed set, not fitted to the music as Nintendo's
tools do; every frame takes the pair and scale that come closest. bcstm.c is the kit's reader; any
theme's bgm.bcstm can stand in for the file this writes.
"""
import math
import struct
import sys
import wave

FRAME = 14                      # samples in an ADPCM frame of 8 bytes
BLOCK_BYTES = 0x2000            # per channel, as in the theme music at hand
BLOCK_SAMPLES = BLOCK_BYTES // 8 * FRAME
# predictor pairs in 1/2048: none, the last sample, and second-order ones from gentle to a straight line through
# the last two samples
COEFS = [(0, 0), (2048, 0), (1920, 0), (2816, -1024), (3136, -1760), (3680, -1664), (3904, -1920), (4096, -2048)]


def clamp16(v):
    return -32768 if v < -32768 else 32767 if v > 32767 else v


def encode_channel(samples):
    """The frames of one channel and, for the SEEK block, the two samples before each block as the reader has them."""
    out = bytearray()
    history = []
    h1 = h2 = 0
    for start in range(0, len(samples), FRAME):
        if start % BLOCK_SAMPLES == 0:
            history.append((h1, h2))
        chunk = samples[start:start + FRAME]
        best = None
        for p, (c1, c2) in enumerate(COEFS):
            # the scale the frame needs if the reader's samples were the true ones, then one finer and one coarser
            a, b, most = h1, h2, 0
            for x in chunk:
                r = abs(x - ((1024 + c1 * a + c2 * b) >> 11))
                most = r if r > most else most
                b, a = a, x
            need = 0
            while need < 12 and most > (15 << need) // 2:
                need += 1
            for scale in {max(0, need - 1), need, min(12, need + 1)}:
                a, b, error, nibbles = h1, h2, 0, []
                half = (1 << scale) >> 1
                for x in chunk:
                    base = (1024 + c1 * a + c2 * b) >> 11
                    n = (x - base + half) >> scale
                    n = -8 if n < -8 else 7 if n > 7 else n
                    y = clamp16((n << scale) + base)
                    error += (x - y) * (x - y)
                    nibbles.append(n & 15)
                    b, a = a, y
                if best is None or error < best[0]:
                    best = (error, p, scale, nibbles, a, b)
        _, p, scale, nibbles, h1, h2 = best
        nibbles += [0] * (FRAME - len(nibbles))
        out.append(p << 4 | scale)
        out += bytes(nibbles[i] << 4 | nibbles[i + 1] for i in range(0, FRAME, 2))
    return bytes(out), history


def reference(kind, offset):
    return struct.pack("<HHI", kind, 0, offset)


def pad(data, to):
    return data + bytes(-len(data) % to)


def build(channels, rate, loop):
    """channels: one list of samples per channel; loop: (start, end) or None."""
    count, total = len(channels), len(channels[0])
    encoded = [encode_channel(c) for c in channels]
    blocks = (total + BLOCK_SAMPLES - 1) // BLOCK_SAMPLES
    last_samples = total - (blocks - 1) * BLOCK_SAMPLES
    last_bytes = (last_samples + FRAME - 1) // FRAME * 8
    last_padded = (last_bytes + 0x1F) & ~0x1F

    # INFO: references, the stream's numbers, one track over all channels, a channel entry and its ADPCM values each
    stream = struct.pack("<BBBB11I", 2, 1 if loop else 0, count, 0, rate, loop[0] if loop else 0,
                         loop[1] if loop else total, blocks, BLOCK_BYTES, BLOCK_SAMPLES, last_bytes, last_samples,
                         last_padded, 4, BLOCK_SAMPLES) + reference(0x1F00, 0x18)
    track_table_at = 0x18 + len(stream)             # offsets count from the first reference, 8 bytes into the block
    track_table = struct.pack("<I", 1)
    channel_table_at = track_table_at + 4 + 8
    channel_table = struct.pack("<I", count)
    track_at = channel_table_at + 4 + 8 * count
    track = struct.pack("<BBH", 127, 64, 0) + reference(0x0100, 0x0C) + pad(struct.pack("<I", count) + bytes(range(count)), 4)
    entries_at = track_at + len(track)
    adpcm_at = entries_at + 8 * count
    track_table += reference(0x4101, track_at - track_table_at)
    entries = adpcm = b""
    for c in range(count):
        channel_table += reference(0x4102, entries_at + 8 * c - channel_table_at)
        entries += reference(0x0300, adpcm_at + 0x2E * c - (entries_at + 8 * c))
        first = encoded[c][0][0]
        adpcm += b"".join(struct.pack("<hh", c1, c2) for c1, c2 in COEFS) + struct.pack("<BBhh", first, 0, 0, 0)
        if loop and loop[0]:
            at = loop[0] // FRAME * 8
            block = loop[0] // BLOCK_SAMPLES
            h1, h2 = decode_history(encoded[c][0], encoded[c][1][block], block * BLOCK_SAMPLES, loop[0])
            adpcm += struct.pack("<BBhh", encoded[c][0][at], 0, h1, h2)
        else:
            adpcm += struct.pack("<BBhh", first, 0, 0, 0)
        adpcm += bytes(2)
    body = (reference(0x4100, 0x18) + reference(0x0101, track_table_at) + reference(0x0101, channel_table_at) + stream +
            track_table + channel_table + track + entries + adpcm)
    info = b"INFO" + struct.pack("<I", (8 + len(body) + 0x1F) & ~0x1F) + body
    info = pad(info, 0x20)

    seek_data = b"".join(struct.pack("<hh", *encoded[c][1][b]) for b in range(blocks) for c in range(count))
    seek = pad(b"SEEK" + struct.pack("<I", (8 + len(seek_data) + 0x1F) & ~0x1F) + seek_data, 0x20)

    data = bytearray(0x18)
    for b in range(blocks):
        size = BLOCK_BYTES if b < blocks - 1 else last_padded
        for c in range(count):
            piece = encoded[c][0][b * BLOCK_BYTES:b * BLOCK_BYTES + (BLOCK_BYTES if b < blocks - 1 else last_bytes)]
            data += pad(piece, size) if len(piece) < size else piece
    data = pad(b"DATA" + struct.pack("<I", (8 + len(data) + 0x1F) & ~0x1F) + bytes(data), 0x20)

    size = 0x40 + len(info) + len(seek) + len(data)
    header = (b"CSTM" + struct.pack("<HHIIHH", 0xFEFF, 0x40, 0x02000000, size, 3, 0) +
              reference(0x4000, 0x40) + struct.pack("<I", len(info)) +
              reference(0x4001, 0x40 + len(info)) + struct.pack("<I", len(seek)) +
              reference(0x4002, 0x40 + len(info) + len(seek)) + struct.pack("<I", len(data)))
    return pad(header, 0x40) + info + seek + data


def decode_frames(frames, coefs, h1, h2, first, last):
    """Samples first..last-1 of a run of frames that starts at a frame boundary, and the history after them."""
    out = []
    for i in range(first // FRAME * FRAME, last):
        frame = frames[i // FRAME * 8:i // FRAME * 8 + 8]
        c1, c2 = coefs[frame[0] >> 4]
        scale = frame[0] & 15
        byte = frame[1 + i % FRAME // 2]
        n = byte & 15 if i % FRAME & 1 else byte >> 4
        n -= 16 if n > 7 else 0
        y = clamp16(((n << scale << 11) + 1024 + c1 * h1 + c2 * h2) >> 11)
        h2, h1 = h1, y
        if i >= first:
            out.append(y)
    return out, h1, h2


def decode_history(frames, at_block, block_start, sample):
    """The two samples before `sample`, decoding from its block's start."""
    _, h1, h2 = decode_frames(frames[block_start // FRAME * 8:], COEFS, at_block[0], at_block[1], 0, sample - block_start)
    return h1, h2


def read_bcstm(d):
    """Enough of a DSP-ADPCM BCSTM to decode it: what bcstm.c reads, without its checks."""
    if d[:4] != b"CSTM" or struct.unpack_from("<H", d, 4)[0] != 0xFEFF:
        raise SystemExit("not a little-endian BCSTM")
    blocks = {struct.unpack_from("<H", d, 0x14 + 12 * i)[0]: struct.unpack_from("<I", d, 0x18 + 12 * i)[0] for i in range(3)}
    info, data = blocks[0x4000], blocks[0x4002]
    base = info + 8
    stream = base + struct.unpack_from("<I", d, base + 4)[0]
    encoding, loops, count = d[stream], d[stream + 1], d[stream + 2]
    if encoding != 2:
        raise SystemExit("only DSP-ADPCM streams are decoded here")
    (rate, loop_start, loop_end, n_blocks, block_bytes, block_samples, last_bytes, last_samples,
     last_padded) = struct.unpack_from("<9I", d, stream + 4)
    start = data + 8 + struct.unpack_from("<I", d, stream + 0x34)[0]
    table = base + struct.unpack_from("<I", d, base + 0x14)[0]
    channels = []
    for c in range(count):
        entry = table + struct.unpack_from("<I", d, table + 4 + 8 * c + 4)[0]
        adpcm = entry + struct.unpack_from("<I", d, entry + 4)[0]
        coefs = [struct.unpack_from("<hh", d, adpcm + 4 * k) for k in range(8)]
        h1, h2 = struct.unpack_from("<hh", d, adpcm + 0x22)
        samples = []
        for b in range(n_blocks):
            at = start + (b * count + c) * block_bytes if b < n_blocks - 1 else \
                start + (n_blocks - 1) * count * block_bytes + c * last_padded
            n = block_samples if b < n_blocks - 1 else last_samples
            got, h1, h2 = decode_frames(d[at:at + (block_bytes if b < n_blocks - 1 else last_bytes)], coefs, h1, h2, 0, n)
            samples += got
        channels.append(samples)
    return channels, rate, (loop_start, loop_end) if loops else None


def write_wav(path, channels, rate):
    with wave.open(path, "wb") as w:
        w.setnchannels(len(channels))
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(struct.pack("<%dh" % (len(channels) * len(channels[0])),
                                  *[c[i] for i in range(len(channels[0])) for c in channels]))


def main(argv):
    args = argv[1:]
    if args and args[0] == "--decode":
        if len(args) not in (3, 5) or (len(args) == 5 and args[3] != "--turns"):
            raise SystemExit(__doc__)
        channels, rate, loop = read_bcstm(open(args[1], "rb").read())
        turns = int(args[4]) if len(args) == 5 else 1
        if loop:
            channels = [c[:loop[1]] + c[loop[0]:loop[1]] * turns for c in channels]
        write_wav(args[2], channels, rate)
        return
    if len(args) < 2:
        raise SystemExit(__doc__)
    with wave.open(args[0], "rb") as w:
        if w.getsampwidth() != 2 or w.getnchannels() not in (1, 2) or w.getcomptype() != "NONE":
            raise SystemExit("IN.wav must be 16-bit PCM, mono or stereo")
        count, rate, total = w.getnchannels(), w.getframerate(), w.getnframes()
        flat = struct.unpack("<%dh" % (count * total), w.readframes(total))
    channels = [list(flat[c::count]) for c in range(count)]
    loop = (0, total)
    if args[2:] == ["--no-loop"]:
        loop = None
    elif len(args) == 5 and args[2] == "--loop":
        loop = (int(args[3]), int(args[4]))
        if not 0 <= loop[0] < loop[1] <= total:
            raise SystemExit("--loop START END: 0 <= START < END <= %d samples" % total)
        channels = [c[:loop[1]] for c in channels]
    elif args[2:]:
        raise SystemExit(__doc__)
    out = build(channels, rate, loop)
    open(args[1], "wb").write(out)
    back, _, _ = read_bcstm(out)
    noise = sum((a - b) ** 2 for c, d in zip(channels, back) for a, b in zip(c, d))
    power = sum(a * a for c in channels for a in c) or 1
    print("%s: %d bytes, %d channel(s), %d Hz, %.1f s, loop %s, signal to noise %.1f dB" % (
        args[1], len(out), count, rate, len(channels[0]) / rate, "%d to %d" % loop if loop else "none",
        10 * math.log10(power / noise) if noise else 99.0))


if __name__ == "__main__":
    main(sys.argv)
