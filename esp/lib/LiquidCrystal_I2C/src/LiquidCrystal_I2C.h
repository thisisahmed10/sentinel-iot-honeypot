/* Minimal LiquidCrystal_I2C driver (small subset)
 * Provides init(), backlight(), clear(), setCursor(), print(String)
 * Works with common PCF8574 I2C backpacks on 1602 displays.
 */
#pragma once
#include <Arduino.h>
#include <Wire.h>

class LiquidCrystal_I2C {
public:
  LiquidCrystal_I2C(uint8_t address, uint8_t cols, uint8_t rows);
  void init();
  void backlight();
  void noBacklight();
  void clear();
  void setCursor(uint8_t col, uint8_t row);
  void print(const String &s);
  void print(const char *s);
  void print(char c);

private:
  void expanderWrite(uint8_t data);
  void write4bits(uint8_t value);
  void pulseEnable(uint8_t data);
  void send(uint8_t value, uint8_t mode);
  void command(uint8_t value);
  uint8_t _addr;
  uint8_t _cols;
  uint8_t _rows;
  uint8_t _backlightval;
  const uint8_t ROW_OFFSETS[4] = {0x00, 0x40, 0x14, 0x54};
};
