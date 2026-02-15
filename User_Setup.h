// User_Setup.h for TFT_eSPI
// Assumes an ESP32 with an ST7789 240x280 display. Adjust pins if needed.

#ifndef USER_SETUP_LOADED
#define USER_SETUP_LOADED

// Setup for ST7789 240x280
#define ST7789_DRIVER

// Display size
#define TFT_WIDTH  240
#define TFT_HEIGHT 280

// SPI pins
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  17

// Backlight pin (active HIGH)
#define TFT_BL   4

// Optional: set SPI frequency for display
#define SPI_FREQUENCY  40000000

#endif // USER_SETUP_LOADED
