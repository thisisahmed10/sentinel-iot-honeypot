/* Minimal LiquidCrystal_I2C implementation */
#include "LiquidCrystal_I2C.h"

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_FUNCTIONSET 0x20

#define LCD_ENTRYLEFT 0x02
#define LCD_DISPLAYON 0x04
#define LCD_2LINE 0x08

// flags for backlight
#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00

#define En 0x04  // Enable bit
#define Rw 0x02  // Read/Write bit
#define Rs 0x01  // Register select bit

LiquidCrystal_I2C::LiquidCrystal_I2C(uint8_t address, uint8_t cols, uint8_t rows)
    : _addr(address), _cols(cols), _rows(rows), _backlightval(LCD_BACKLIGHT) {}

void LiquidCrystal_I2C::init()
{
  Wire.begin();
  // basic init sequence
  delay(50);
  expanderWrite(_backlightval);
  // put into 4-bit mode
  write4bits(0x03 << 4);
  delay(5);
  write4bits(0x03 << 4);
  delay(5);
  write4bits(0x03 << 4);
  write4bits(0x02 << 4);
  // function set
  command(LCD_FUNCTIONSET | LCD_2LINE);
  // display on
  command(LCD_DISPLAYCONTROL | LCD_DISPLAYON);
  // clear
  clear();
}

void LiquidCrystal_I2C::backlight()
{
  _backlightval = LCD_BACKLIGHT;
  expanderWrite(0);
}

void LiquidCrystal_I2C::noBacklight()
{
  _backlightval = LCD_NOBACKLIGHT;
  expanderWrite(0);
}

void LiquidCrystal_I2C::clear()
{
  command(LCD_CLEARDISPLAY);
  delay(2);
}

void LiquidCrystal_I2C::setCursor(uint8_t col, uint8_t row)
{
  if (row >= _rows)
    row = _rows - 1;
  uint8_t addr = col + ROW_OFFSETS[row];
  command(0x80 | addr);
}

void LiquidCrystal_I2C::print(const String &s)
{
  print(s.c_str());
}

void LiquidCrystal_I2C::print(const char *s)
{
  while (*s)
    print(*s++);
}

void LiquidCrystal_I2C::print(char c)
{
  send(c, Rs);
}

void LiquidCrystal_I2C::expanderWrite(uint8_t data)
{
  Wire.beginTransmission(_addr);
  Wire.write((int)(data) | _backlightval);
  Wire.endTransmission();
}

void LiquidCrystal_I2C::write4bits(uint8_t value)
{
  expanderWrite(value);
  pulseEnable(value);
}

void LiquidCrystal_I2C::pulseEnable(uint8_t data)
{
  expanderWrite(data | En);
  delayMicroseconds(1);
  expanderWrite(data & ~En);
  delayMicroseconds(50);
}

void LiquidCrystal_I2C::send(uint8_t value, uint8_t mode)
{
  uint8_t highnib = value & 0xF0;
  uint8_t lownib = (value << 4) & 0xF0;
  write4bits(highnib | mode);
  write4bits(lownib | mode);
}

void LiquidCrystal_I2C::command(uint8_t value)
{
  send(value, 0);
}
