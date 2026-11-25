#!/usr/bin/env python3
"""Writes the notification sounds: a soft two-tone chime and a more insistent one for critical notifications.

The sounds are made here from sine waves, so they belong to nobody: they are
dedicated to the public domain (CC0 1.0).

Usage: generate-notification-sounds.py OUTPUT_DIRECTORY
"""

import math
import os
import struct
import sys
import wave

SAMPLE_RATE = 48000
PEAK = 0.45 * 32767

# (frequency in hertz, start in seconds, length in seconds)
CHIME_TONES = [(880.0, 0.00, 0.30), (1318.5, 0.11, 0.45)]
CRITICAL_TONES = [(1046.5, 0.00, 0.14), (1568.0, 0.16, 0.14), (1046.5, 0.40, 0.14), (1568.0, 0.56, 0.30)]


def tone_sample(frequency, age, length):
    """One sample of a bell-like tone: a quick rise, then an exponential fade."""
    if age < 0 or age > length:
        return 0.0

    attack = min(age / 0.008, 1.0)
    decay = math.exp(-5.0 * age / length)

    return attack * decay * math.sin(2 * math.pi * frequency * age)


def mixed_sample(tones, time):
    """The tones added together at one moment, scaled to 16 bits."""
    value = sum(tone_sample(frequency, time - start, length) for frequency, start, length in tones)

    return int(max(-1.0, min(1.0, value / len(tones) * 1.6)) * PEAK)


def render(tones):
    """Mixes the tones into 16-bit little-endian samples."""
    total = max(start + length for _, start, length in tones) + 0.05
    samples = [mixed_sample(tones, index / SAMPLE_RATE) for index in range(int(total * SAMPLE_RATE))]

    return struct.pack(f"<{len(samples)}h", *samples)


def write_wave(path, tones):
    """Mono, 16-bit, 48 kHz: what every player plays."""
    frames = render(tones)

    with wave.open(path, "wb") as output:
        output.setparams((1, 2, SAMPLE_RATE, 0, "NONE", "not compressed"))
        output.writeframes(frames)


def main(output_directory):
    write_wave(os.path.join(output_directory, "notification-chime.wav"), CHIME_TONES)
    write_wave(os.path.join(output_directory, "notification-critical-chime.wav"), CRITICAL_TONES)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
