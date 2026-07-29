#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

// SD wiring for the 5.0-inch HMI card slot.
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK 12
#define SD_CS 10
/**
 * @brief Initialize the serial port, SPI bus, and SD card.
 *
 * Called once after reset. The directory tree is printed when mounting
 * succeeds, making the card status visible in the serial monitor.
 */
void setup() {
  Serial.begin( 115200 );
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
}

/**
 * @brief Mount the SD card and list its top-level files and directories.
 *
 * @return 0 when the card is ready; 1 when mounting or card detection fails.
 * @note Called by setup() after the SPI pins have been configured.
 */
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

/**
 * @brief Recursively print directory entries to the serial monitor.
 *
 * @param fs Filesystem instance to inspect.
 * @param dirname Directory path to open.
 * @param levels Maximum recursion depth.
 * @return None.
 * @note Called by SD_init() after a successful mount.
 */
void listDir(fs::FS & fs, const char *dirname, uint8_t levels)
{
  File root = fs.open(dirname);
  if (!root)
  {
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
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
      Serial.print("SIZE: ");
      Serial.println(file.size());
    }

    file = root.openNextFile();
  }
}
