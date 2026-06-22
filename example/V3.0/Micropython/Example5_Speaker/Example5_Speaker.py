import os
import math
import struct
from machine import I2S
from machine import Pin

# ========== NOTE FREQUENCY TABLE (C4 = 261.63Hz) ==========
NOTES = {
    'C4': 261.63, 'D4': 293.66, 'E4': 329.63, 'F4': 349.23,
    'G4': 392.00, 'A4': 440.00, 'B4': 493.88,
    'C5': 523.25, 'D5': 587.33, 'E5': 659.25, 'F5': 698.46,
    'G5': 783.99, 'A5': 880.00, 'B5': 987.77,
    'REST': 0,
}

# ========== HAPPY BIRTHDAY MELODY ==========
# Each element: (note, duration_in_beats)
# Beat unit is a quarter note
MELODY = [
    ('G4', 0.5), ('G4', 0.5), ('A4', 1.0), ('G4', 1.0), ('C5', 1.0), ('B4', 2.0),
    ('G4', 0.5), ('G4', 0.5), ('A4', 1.0), ('G4', 1.0), ('D5', 1.0), ('C5', 2.0),
    ('G4', 0.5), ('G4', 0.5), ('G5', 1.0), ('E5', 1.0), ('C5', 1.0), ('B4', 1.0), ('A4', 2.0),
    ('F5', 0.5), ('F5', 0.5), ('E5', 1.0), ('C5', 1.0), ('D5', 1.0), ('C5', 2.0),
]

def make_tone(rate, bits, frequency, duration_ms):
    """Generate sine wave samples for a given frequency and duration."""
    if frequency == 0:
        # Rest note: generate silence
        samples = bytearray(rate * (bits // 8) * duration_ms // 1000)
        return samples

    samples_per_cycle = int(rate / frequency)
    sample_size_in_bytes = bits // 8
    total_samples = int(rate * duration_ms / 1000)
    
    # Ensure total samples is an integer multiple of samples_per_cycle to avoid clicking
    cycles = total_samples // samples_per_cycle
    total_samples = cycles * samples_per_cycle
    
    samples = bytearray(total_samples * sample_size_in_bytes)
    volume_reduction_factor = 8  # Adjust volume level
    amplitude = int((pow(2, bits) // 2 - 1) / volume_reduction_factor)

    if bits == 16:
        fmt = "<h"
    else:
        fmt = "<l"

    for i in range(total_samples):
        sample = int(amplitude * math.sin(2 * math.pi * i / samples_per_cycle))
        struct.pack_into(fmt, samples, i * sample_size_in_bytes, sample)

    return samples

def make_silence(rate, bits, duration_ms):
    """Generate silence samples."""
    sample_size = bits // 8
    num_samples = rate * duration_ms // 1000
    return bytearray(num_samples * sample_size)

# ========== BOARD CONFIGURATION ==========
if os.uname().machine.count("PYBv1"):
    SCK_PIN = "Y6"
    WS_PIN = "Y5"
    SD_PIN = "Y8"
    I2S_ID = 2
    BUFFER_LENGTH_IN_BYTES = 2000

elif os.uname().machine.count("PYBD"):
    import pyb
    pyb.Pin("EN_3V3").on()  # Provide 3.3V on 3V3 output pin

    SCK_PIN = "Y6"
    WS_PIN = "Y5"
    SD_PIN = "Y8"
    I2S_ID = 2
    BUFFER_LENGTH_IN_BYTES = 2000

elif os.uname().machine.count("ESP32"):
    SCK_PIN = 42
    WS_PIN = 18
    SD_PIN = 17
    I2S_ID = 0
    BUFFER_LENGTH_IN_BYTES = 2000

elif os.uname().machine.count("Raspberry"):
    SCK_PIN = 16
    WS_PIN = 17
    SD_PIN = 18
    I2S_ID = 0
    BUFFER_LENGTH_IN_BYTES = 1000

elif os.uname().machine.count("MIMXRT"):
    SCK_PIN = 4
    WS_PIN = 3
    SD_PIN = 2
    I2S_ID = 2
    BUFFER_LENGTH_IN_BYTES = 2000

else:
    print("Warning: program not tested with this board")

# ========== AUDIO CONFIGURATION ==========
TEMPO_MS = 400  # Duration of one beat in milliseconds
SAMPLE_SIZE_IN_BITS = 16
FORMAT = I2S.MONO  # Only mono supported in this example
SAMPLE_RATE_IN_HZ = 22_050

audio_out = I2S(
    I2S_ID,
    sck=Pin(SCK_PIN),
    ws=Pin(WS_PIN),
    sd=Pin(SD_PIN),
    mode=I2S.TX,
    bits=SAMPLE_SIZE_IN_BITS,
    format=FORMAT,
    rate=SAMPLE_RATE_IN_HZ,
    ibuf=BUFFER_LENGTH_IN_BYTES,
)

# ========== PRE-GENERATE ALL NOTE SAMPLES ==========
print("Generating audio data...")

# Pre-generate note cache to avoid real-time computation during playback
note_cache = {}
for note_name, freq in NOTES.items():
    for beats in [0.5, 1.0, 2.0]:
        duration_ms = int(TEMPO_MS * beats)
        key = (note_name, beats)
        note_cache[key] = make_tone(SAMPLE_RATE_IN_HZ, SAMPLE_SIZE_IN_BITS, freq, duration_ms)

# Short silence between notes to prevent slurring
GAP_MS = 50
silence = make_silence(SAMPLE_RATE_IN_HZ, SAMPLE_SIZE_IN_BITS, GAP_MS)

# ========== PLAYBACK ==========
print("==========  PLAYING HAPPY BIRTHDAY  ==========")
print("Press Ctrl+C to stop")

try:
    while True:
        for note_name, beats in MELODY:
            key = (note_name, beats)
            samples = note_cache[key]
            
            # Write note samples
            audio_out.write(samples)
            
            # Add brief silence between notes for clearer rhythm
            audio_out.write(silence)
            
        # Pause 1 second after each full melody loop
        audio_out.write(make_silence(SAMPLE_RATE_IN_HZ, SAMPLE_SIZE_IN_BITS, 1000))

except (KeyboardInterrupt, Exception) as e:
    print("Stopped: {} {}".format(type(e).__name__, e))

# Cleanup
audio_out.deinit()
print("Done")