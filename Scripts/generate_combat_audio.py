"""Original short synthesized game cues, deterministic mono PCM16 WAV sources."""
from pathlib import Path
import math
import random
import struct
import wave

root = Path(__file__).resolve().parents[1] / 'Data' / 'AudioSource'
root.mkdir(parents=True, exist_ok=True)
rate = 22050
lengths = dict(Swing=.22, Hit=.18, Cast=.35, Blast=.48, Hurt=.26, Heal=.55,
               Pickup=.22, Extract=.8, Death=.65, Click=.08, Charge=.5)
for index, (name, length) in enumerate(lengths.items()):
    rng = random.Random(4900 + index)
    samples = []
    phase = 0.0
    last_noise = 0.0
    for frame in range(int(rate * length)):
        t = frame / rate
        u = t / length
        noise = rng.uniform(-1, 1)
        smooth = .8 * last_noise + .2 * noise
        last_noise = smooth
        if name == 'Swing':
            value = (noise - smooth) * math.sin(math.pi * u) ** 1.5
        elif name in ('Hit', 'Hurt', 'Blast'):
            freq = {'Hit': 170, 'Hurt': 110, 'Blast': 65}[name]
            value = (.65 * math.sin(2 * math.pi * freq * t * (1 - .35 * u)) + .5 * smooth) * math.exp(-7 * u)
        elif name in ('Heal', 'Pickup', 'Extract'):
            notes = {'Heal': [523.25, 659.25, 783.99], 'Pickup': [880, 1318.51], 'Extract': [392, 493.88, 587.33, 783.99]}[name]
            part = min(len(notes) - 1, int(u * len(notes)))
            local = (u * len(notes)) % 1
            value = (math.sin(2 * math.pi * notes[part] * t) + .18 * math.sin(4 * math.pi * notes[part] * t)) * math.exp(-3 * local) * (1 - .5 * u)
        elif name in ('Cast', 'Charge', 'Death'):
            freq = (260 + 900 * u) if name == 'Cast' else (95 + 160 * u) if name == 'Charge' else (320 * (1 - u) + 45)
            phase += 2 * math.pi * freq / rate
            value = (.7 * math.sin(phase) + .2 * smooth) * math.sin(math.pi * u) * (1 if name != 'Death' else 1 - u)
        else:
            value = math.sin(2 * math.pi * 900 * t) * math.exp(-12 * u)
        edge = min(1, t / .004, (length - t) / .009)
        samples.append(value * max(0, edge))
    peak = max(abs(x) for x in samples) or 1
    pcm = b''.join(struct.pack('<h', round(x / peak * .58 * 32767)) for x in samples)
    path = root / ('S_AR_' + name + '.wav')
    with wave.open(str(path), 'wb') as output:
        output.setparams((1, 2, rate, len(samples), 'NONE', 'not compressed'))
        output.writeframes(pcm)
    print(f'{path.name}: {length:.2f}s')
