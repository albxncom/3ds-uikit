"""The liquid's strength: the one rule by which a look's tone becomes a liquid's, in every look and scheme. The kit's
download bar takes its tone by it (art.py bar_tone), and so does the liquid of the application's installing screen."""
A = None            # art.py's module (set by art.py before anything here is drawn)


# The liquid's strength (the tones that fill the glass are stronger than the look's own, so that the liquid reads as a
# colour, not a near grey): one rule for every look and scheme, in OKLCH (schemes.py's notation: l = Y ** (1/3)). The
# look's tone keeps its hue and its luminance (so its contrast to the card and the glass, schemes.POUR_MIN, holds); its
# chroma is raised by LIQUID_GAIN and to at least LIQUID_CHROMA, as far as sRGB shows that hue there (liquid_tone). The
# grade from the surface down to the foot moves the lightness alone, at the same hue and at that chroma or more, so the
# liquid stays its colour down to the foot instead of going pale (LIQUID_STEPS: deep, mid, light, foot as l apart from
# the tone, on the light schemes and on the dark ones), each step at LIQUID_FULL of the most chroma sRGB shows at its
# hue and lightness, or the tone's chroma if that is more. On the light schemes the body and its light end go further, a
# share (LIQUID_TOWARDS) of the way to the lightness where the hue is most vivid, when that lies higher (the deep under
# the surface stays where it is, so the level reads): a yellow's or a green's liquid is then a clear gold or green, not
# an ochre or an olive.
LIQUID_CHROMA = 0.13


LIQUID_GAIN = 1.25


LIQUID_FULL = 0.92


# Own takes (Gelb lighter and nearer a yellow, Sand nearer a brown): Gelb's and Sand's pour tones lie 3 degrees
# of hue apart at the same low luminance, so the rule above made both the same amber. Their liquid takes its tone from
# a take of its own in every look (LIQUID_OWN: the scheme's name to the take's): hue (OKLCH degrees), lit (the tone's
# lightness l) and chroma (the liquid's at every step; none: LIQUID_FULL of the most sRGB shows), each on the light
# schemes and on the dark ones, steps (in place of LIQUID_STEPS on the light schemes), warm (the hue the steps darker
# than the tone turn to, all the way at reach under it), piece (the step the falling piece's tones take from the
# liquid's on the light schemes). A take's lightness is its own: its grade does not go towards the hue's most vivid
# lightness (that would make a brown orange).
# Gelb's take (an orange like the reference it is matched to, a little stronger than a photograph of it shows, since the
# photograph has a white cast): Gelb's liquid follows the fill of that photograph, measured: a deep
# orange under the surface (OKLCH hue 45, l 0.70) that turns yellower as it lightens down to the foot (hue 82,
# l 0.83), chroma about 0.13 all the way. Its takes keep that hue's course (hue at the foot, warm under the surface,
# reach the depth of the deep step) a little darker and with more chroma than the photograph's white cast shows: a one
# step stronger, b stronger still. The dark schemes' steps (LIQUID_STEPS) stay closer to the tone, so their hue starts
# warmer to stay an orange. The piece falls in the deeper orange so it reads through the frosted tile.
# Each take stays reachable by its name, so they can be compared side by side.
# The takes c and d (b with a little more red, a more orange orange): c and d keep b's lightness,
# chroma and steps and turn its hue course towards red, c by 9 degrees at the foot and 4 under the surface, d by twice
# that. On the dark schemes an orange at Nachtglas's lightness holds less chroma the redder it is (sRGB), so their
# hue goes only as far as Gelb's chroma stays clearly apart from Sand's:
# c 6 degrees, d to 63.
LIQUID_TAKES = dict(
    GELB_A=dict(hue=(82.0, 70.0), lit=(0.81, 0.78), chroma=(0.16, 0.16), warm=45.0, reach=0.13,
                steps=dict(deep=-0.13, mid=-0.075, light=0.0, foot=0.01), piece=-0.10),
    GELB_B=dict(hue=(82.0, 70.0), lit=(0.79, 0.76), chroma=(0.19, 0.19), warm=43.0, reach=0.13,
                steps=dict(deep=-0.13, mid=-0.075, light=0.0, foot=0.01), piece=-0.10),
    GELB_C=dict(hue=(73.0, 64.0), lit=(0.79, 0.76), chroma=(0.19, 0.19), warm=39.0, reach=0.13,
                steps=dict(deep=-0.13, mid=-0.075, light=0.0, foot=0.01), piece=-0.10),
    GELB_D=dict(hue=(64.0, 63.0), lit=(0.79, 0.76), chroma=(0.19, 0.19), warm=35.0, reach=0.13,
                steps=dict(deep=-0.13, mid=-0.075, light=0.0, foot=0.01), piece=-0.10),
    SAND_A=dict(hue=(65.0, 65.0), lit=(0.56, 0.58), chroma=(0.12, 0.13)),
    SAND_B=dict(hue=(62.0, 62.0), lit=(0.50, 0.58), chroma=(0.11, 0.125)),
)


LIQUID_OWN = dict(GELB="GELB_C", SAND="SAND_B")


def own_take():
    """The take (LIQUID_TAKES) the drawn scheme's liquid follows, or None for the rule alone."""
    return LIQUID_TAKES.get(LIQUID_OWN.get(A.schemes.SCHEMES[A.SCHEME]["name"]))


def liquid_tone(tone):
    """A look's liquid tone (r, g, b of 0 to 255) at the liquid's strength: its hue and luminance, its chroma
    LIQUID_GAIN times its own and LIQUID_CHROMA or more (each as far as sRGB reaches); in a scheme with a take of its
    own, the take's hue, lightness and chroma."""
    S = A.schemes
    take = own_take()
    if take:
        dark = S.luminance(A.rgb(A.L.COL_CARD)) < 0.2
        lit = take["lit"][dark]
        chroma = take["chroma"][dark] if "chroma" in take else LIQUID_FULL * most_chroma(take["hue"][dark], lit)
        return tuple(float(v) for v in S.colour_at(take["hue"][dark], chroma, lit ** 3))
    _, chroma, hue = S.oklch(tone)
    want = max(chroma * LIQUID_GAIN, LIQUID_CHROMA)
    return tuple(float(v) for v in S.colour_at(hue, want, S.luminance(tone)))


def most_chroma(hue, lit):
    """The most chroma sRGB shows at hue and lightness l."""
    return A.schemes.oklch(A.schemes.colour_at(hue, 0.4, lit ** 3))[1]
