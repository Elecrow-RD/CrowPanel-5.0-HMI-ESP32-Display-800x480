#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

//5.0
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK 12
#define SD_CS 10
void setup() {
  // put your setup code here, to run once:
  Serial.begin( 115200 ); /*Initialize the serial port*/
  //SD卡
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  delay(100);
  if (SD_init() == 1)
  {
    Serial.println("Card Mount Failed");
  }
  else
    Serial.println("initialize SD Card successfully");
}

void loop() {
  // put your main code here, to run repeatedly:

}

//Initialize the SD Card
int SD_init()
{

  if (!SD.begin(SD_CS))
  {
    Serial.println("Card Mount Failed");
    return 1;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No TF card attached");
    return 1;
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("TF Card Size: %lluMB\n", cardSize);
  listDir(SD, "/", 2);


  return 0;
}

//Traverse the SD card directory
void listDir(fs::FS & fs, const char *dirname, uint8_t levels)
{
  //  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    //Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  //  i = 0;
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.name(), levels - 1);
      }
    }
    else
    {
      Serial.print("FILE: ");
      Serial.print(file.name());
      //      lcd.setCursor(0, 2 * i);
      //      lcd.printf("FILE:%s", file.name());
      Serial.print("SIZE: ");
      Serial.println(file.size());
      //      lcd.setCursor(180, 2 * i);
      //      lcd.printf("SIZE:%d", file.size());
      //      i += 16;
    }

    file = root.openNextFile();
  }
}