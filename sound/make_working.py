"""Makes the working loop, the short pattern that sounds while the application installs or loads: our own notes,
from plain tones, nothing sampled.

  python3 -I make_working.py OUT_DIR      writes OUT_DIR/working-run.wav and OUT_DIR/working-pendulum.wav
                                          (16-bit, mono, 22050 Hz); wav2bcstm.py turns one into working.bcstm

The tone is a soft mallet: a sine with its octave (a third as strong) and the octave above that (an eighth), in
over 12 ms and dying away so that a note rings on under the next ones. Every loop is rendered round its own end
(a tail that passes the end comes back in at the start), so it repeats without a joint.

  run        G major pentatonic, 108 quarter notes a minute, six beats (3.333 s): twelve notes in a
             long-short-short rhythm (an eighth, two sixteenths, four times), B4 A4 G4 E4 D4 E4 G4 A4 D4 G4 B4 D5,
             then two beats in which they ring out.
  pendulum   calmer: G major, 120 quarter notes a minute, four beats (2 s): G4 B4 D5 B4, a quarter note each.
"""
import math
import struct
import sys
import wave

RATE = 22050
PEAK = 0.20                     # of full scale: about -14 dBFS, quiet beside the interface sounds

NOTES = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}


def hertz(name):
    """A note such as F#5 in equal temperament, A4 = 440 Hz."""
    return 440.0 * 2 ** ((NOTES[name[:-1]] + 12 * (int(name[-1]) + 1) - 69) / 12.0)


def mallet(freq, ring):
    """One note: seconds of ring until it has fallen to about a third; cut where it is below a thousandth."""
    out = []
    for i in range(int(RATE * ring * 7)):
        t = i / RATE
        rise = 0.5 - 0.5 * math.cos(math.pi * min(1.0, t / 0.012))
        body = (math.sin(2 * math.pi * freq * t) + 0.33 * math.exp(-t / (ring * 0.6)) * math.sin(4 * math.pi * freq * t) +
                0.125 * math.exp(-t / (ring * 0.35)) * math.sin(8 * math.pi * freq * t))
        out.append(rise * math.exp(-t / ring) * body)
    return out


def loop(beats, tempo, notes, ring):
    """notes: (beat it starts on, name); the loop is `beats` quarter notes long at `tempo` a minute."""
    length = round(RATE * beats * 60.0 / tempo)
    mix = [0.0] * length
    for at, name in notes:
        start = round(RATE * at * 60.0 / tempo)
        for i, v in enumerate(mallet(hertz(name), ring)):
            mix[(start + i) % length] += v
    top = max(abs(v) for v in mix)
    return [round(v / top * PEAK * 32767) for v in mix]


LOOPS = {
    # an eighth and two sixteenths, four times; then two beats of ringing out
    "run": (6, 108, [(0, "B4"), (0.5, "A4"), (0.75, "G4"), (1, "E4"), (1.5, "D4"), (1.75, "E4"),
                     (2, "G4"), (2.5, "A4"), (2.75, "D4"), (3, "G4"), (3.5, "B4"), (3.75, "D5")], 0.42),
    "pendulum": (4, 120, [(0, "G4"), (1, "B4"), (2, "D5"), (3, "B4")], 0.55),
}


def main(argv):
    if len(argv) != 2:
        raise SystemExit(__doc__)
    for name, (beats, tempo, notes, ring) in LOOPS.items():
        samples = loop(beats, tempo, notes, ring)
        path = "%s/working-%s.wav" % (argv[1], name)
        with wave.open(path, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(struct.pack("<%dh" % len(samples), *samples))
        print("%s: %d samples, %.3f s" % (path, len(samples), len(samples) / RATE))


if __name__ == "__main__":
    main(sys.argv)
