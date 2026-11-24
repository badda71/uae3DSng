#!/usr/bin/env python3
"""Original 2.6 s banner jingle for uae3DSng: a floppy-drive seek, then a rising chip arpeggio and a chord.
Everything is synthesised here (no samples, no existing melody). Writes 44.1 kHz 16-bit stereo WAV."""
import sys, wave, numpy as np
from scipy.signal import resample_poly, butter, sosfilt
SR, OS = 44100, 4                      # output rate, oversampling for alias-free square/saw
N = int(2.6 * SR)
rng = np.random.default_rng(3)

def tone(f, dur, shape='sq', duty=.5, vib=0.0, start_phase=0.0):
    n = int(dur * SR * OS); t = np.arange(n) / (SR * OS)
    f_t = f * (1 + vib * np.sin(2 * np.pi * 5.5 * t) * np.minimum(1, t / .25))
    ph = (np.cumsum(f_t) / (SR * OS) + start_phase) % 1.0
    if shape == 'sq': y = np.where(ph < duty, 1.0, -1.0)
    elif shape == 'tri': y = 4 * np.abs(ph - .5) - 1
    else: y = 2 * ph - 1
    return resample_poly(y, 1, OS)

def env(n, a=.004, d=.18, s=.5, r=.12, hold=None):
    t = np.arange(n) / SR; e = np.minimum(1, t / a) * (s + (1 - s) * np.exp(-t / d))
    rn = int(r * SR); e[-rn:] *= np.linspace(1, 0, rn)
    return e

L = np.zeros(N); R = np.zeros(N)
def put(sig, at, pan=0.0, gain=1.0):
    i = int(at * SR); sig = sig[:max(0, N - i)]
    L[i:i + len(sig)] += sig * gain * (1 - max(0, pan)); R[i:i + len(sig)] += sig * gain * (1 + min(0, pan))

# 1) floppy-drive seek: two short band-passed noise ticks and a low "thunk"
sos = butter(2, [900, 3800], btype='band', fs=SR, output='sos')
for at, g in ((0.02, .55), (0.10, .45)):
    n = int(.045 * SR); ns = sosfilt(sos, rng.standard_normal(n)) * np.exp(-np.arange(n) / (.008 * SR)); put(ns / np.abs(ns).max(), at, 0.1, g)
n = int(.12 * SR); th = tone(95, .12, 'tri')[:n] * np.exp(-np.arange(n) / (.03 * SR)); put(th, 0.03, 0, .5)

# 2) rising arpeggio (C major, square wave 25 % duty), ping-pong panned
notes = [(523.25, -.5), (659.25, .5), (783.99, -.5), (1046.5, .5)]
for k, (f, pan) in enumerate(notes):
    y = tone(f, .26, 'sq', .25, vib=.004); y *= env(len(y), d=.07, s=.25, r=.05); put(y, .26 + k * .085, pan, .30)

# 3) the chord: C5 E5 G5 (pulse + saw, slightly detuned), triangle bass C3, a high ping on top
at = .26 + 4 * .085 + .02
for f, pan in ((523.25, -.3), (659.25, 0.0), (783.99, .3)):
    y = tone(f, 1.7, 'sq', .5, vib=.006) * .6 + tone(f * 1.004, 1.7, 'saw') * .4
    y *= env(len(y), d=.9, s=.15, r=.5); put(y, at, pan, .20)
y = tone(130.81, 1.7, 'tri'); y *= env(len(y), d=1.1, s=.35, r=.5); put(y, at, 0, .38)
y = tone(2093.0, .5, 'sq', .5); y *= env(len(y), a=.002, d=.12, s=0, r=.1); put(y, at + .06, .4, .10)

# 4) ping-pong echo
def echo(x, y, delay, gain):
    i = int(delay * SR); x2 = x.copy(); y2 = y.copy()
    x2[i:] += y[:-i] * gain; y2[i:] += x[:-i] * gain; return x2, y2
L, R = echo(L, R, .17, .35); L, R = echo(L, R, .34, .18)
m = max(np.abs(L).max(), np.abs(R).max()); L, R = L / m * .85, R / m * .85
fade = int(.15 * SR); L[-fade:] *= np.linspace(1, 0, fade); R[-fade:] *= np.linspace(1, 0, fade)
pcm = (np.stack([L, R], 1) * 32767).astype('<i2')
with wave.open(sys.argv[1], 'wb') as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
print('%.2f s, peak %.2f, rms %.3f' % (N / SR, np.abs(pcm).max() / 32767, np.sqrt((pcm.astype(float) / 32767) ** 2).mean()))
