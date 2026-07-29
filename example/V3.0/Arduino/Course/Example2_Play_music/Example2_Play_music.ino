#include <driver/i2s.h>
#include <math.h>

#define I2S_DOUT  17
#define I2S_BCLK  42
#define I2S_LRC   18

// I2S audio format used by the external amplifier.
const int sampleRate = 44100;
const int16_t AMPLITUDE = 32767;

// Note frequencies in hertz used by the demonstration melody.
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784

// Each entry contains one frequency and its duration in milliseconds.
struct Note {
  int freq;
  int durationMs;
};

Note melody[] = {
  // First phrase.
  {NOTE_G4, 200}, {NOTE_G4, 200}, {NOTE_A4, 400}, {NOTE_G4, 400}, {NOTE_C5, 400}, {NOTE_B4, 800},
  // Second phrase.
  {NOTE_G4, 200}, {NOTE_G4, 200}, {NOTE_A4, 400}, {NOTE_G4, 400}, {NOTE_D5, 400}, {NOTE_C5, 800},
  // Third phrase.
  {NOTE_G4, 200}, {NOTE_G4, 200}, {NOTE_G5, 400}, {NOTE_E5, 400}, {NOTE_C5, 400}, {NOTE_B4, 200}, {NOTE_A4, 600},
  // Fourth phrase.
  {NOTE_F5, 200}, {NOTE_F5, 200}, {NOTE_E5, 400}, {NOTE_C5, 400}, {NOTE_D5, 400}, {NOTE_C5, 800},
  // Zero duration marks the end of the melody table.
  {0, 0}
};

/**
 * @brief Configure I2S output and play the first melody pass.
 *
 * Called once after reset. The pin map and sample format must match the
 * amplifier connected to the 5.0-inch HMI board.
 */
void setup() {
  Serial.begin(115200);
  Serial.println("Happy Birthday I2S Test - Full Volume");

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_set_clk(I2S_NUM_0, sampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);

  playMelody();
}

/**
 * @brief Generate a sine wave (or silence) and send it to both I2S channels.
 *
 * @param freq Tone frequency in hertz; zero requests a rest.
 * @param durationMs Tone or rest duration in milliseconds.
 * @return None.
 * @note Called by playMelody() for each table entry.
 */
void playTone(int freq, int durationMs) {
  if (freq == 0) {
    // A rest is sent as zero-valued stereo samples.
    int samplesCount = sampleRate * durationMs / 1000;
    int16_t silence[128] = {0};
    size_t bytes_written;
    
    for (int i = 0; i < samplesCount; i += 64) {
      int batch = min(64, samplesCount - i);
      i2s_write(I2S_NUM_0, silence, batch * sizeof(int16_t) * 2, &bytes_written, portMAX_DELAY);
    }
    return;
  }

  // Generate the requested tone in small DMA-sized batches.
  int samplesCount = sampleRate * durationMs / 1000;
  float phase = 0;
  float phaseIncrement = 2.0 * PI * freq / sampleRate;
  
  size_t bytes_written;
  
  for (int i = 0; i < samplesCount; i += 64) {
    int16_t samples[128];
    int batch = min(64, samplesCount - i);
    
    for (int j = 0; j < batch; j++) {
      int16_t sample = (int16_t)(sin(phase) * AMPLITUDE);
      samples[j * 2]     = sample;
      samples[j * 2 + 1] = sample;
      phase += phaseIncrement;
      if (phase > 2.0 * PI) phase -= 2.0 * PI;
    }
    
    i2s_write(I2S_NUM_0, samples, batch * sizeof(int16_t) * 2, &bytes_written, portMAX_DELAY);
  }
}

/**
 * @brief Play every note until the zero-duration end marker is reached.
 *
 * @return None.
 * @note Called at startup and once per loop iteration.
 */
void playMelody() {
  int i = 0;
  while (melody[i].durationMs > 0) {
    playTone(melody[i].freq, melody[i].durationMs);
    playTone(0, 50);
    i++;
  }
}

/**
 * @brief Wait between complete melody passes and replay the melody.
 */
void loop() {
  delay(2000);
  playMelody();
}
