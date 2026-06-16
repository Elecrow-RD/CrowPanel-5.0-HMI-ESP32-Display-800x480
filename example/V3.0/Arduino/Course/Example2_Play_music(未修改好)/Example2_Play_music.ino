#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include "Audio.h"
Audio audio;

#define I2S_DOUT      17
#define I2S_BCLK      42
#define I2S_LRC       18

//5.0
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK 12
#define SD_CS 10

void setup() {
  // put your setup code here, to run once:
  Serial.begin( 115200 ); /*Initialize the serial port*/
  pinMode(SD_CS, OUTPUT);      digitalWrite(SD_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  SPI.setFrequency(1000000);
  SD.begin(SD_CS);
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(21); // 0...21
  audio.connecttoFS(SD, "/123.mp3");
}

void loop() {
  // put your main code here, to run repeatedly:
  audio.loop();
  //audio.stopSong();//close
}