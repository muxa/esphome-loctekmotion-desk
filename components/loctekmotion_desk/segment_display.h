#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace loctekmotion_desk {

const uint8_t DATA_FRAME_MAX_SIZE   = 16;
const uint8_t DATA_MAX_SIZE         = DATA_FRAME_MAX_SIZE - 3;  // exclude header, length and type
const uint8_t DATA_LENGTH_INDEX     = 1;
const uint8_t DATA_MIN_SIZE         = 2;
const uint8_t DATA_FRAME_START      = 0x9b;
const uint8_t DATA_FRAME_END        = 0x9d;
const uint8_t DATA_TYPE_DISPLAY     = 0x12;

struct DataFrame {
  union {
    uint8_t raw[DATA_FRAME_MAX_SIZE];
    struct {
      uint8_t header;
      uint8_t data_length; // includes length, type, payload and checkum
      uint8_t type;
      uint8_t data[DATA_MAX_SIZE]; // includes payload and checksum
    };
  };

  /**
   * Get size of the raw data (needs data_length set).
   * Returns 0 if not present.
   */
  size_t size() const {
    if (!validate_bounds())
      return 0;

    return data_length + 2; // data lengths excludes header and footer, and includes length, type, payload and CRC
  }

  /**
   * Check if data frame looks good size wise
   */
  bool validate_bounds() const { return data_length > DATA_MIN_SIZE && data_length <= DATA_MAX_SIZE; }

  /**
   * Check if CRC in data matches calculated CRC.
   */
  bool validate_crc() const {
    if (!validate_bounds())
      return false;

    return crc() == calculate_crc();
  }

  /**
   * Get 16bit Modbus-CRC16 Checksum.
   * Returns 0 if not yet available
   */
  uint16_t crc() const {
    if (!validate_bounds())
      return 0;

    uint8_t crc_index = 1 + data_length - 2; // exclude header, footer and crc
    return ((uint16_t)raw[crc_index]<<8) + raw[crc_index+1];
  }

  /**
   * Calculates CRC on the current data by creating an XOR sum
   */
  uint16_t calculate_crc() const {
    if (!validate_bounds())
      return 0;

    // based on https://github.com/LacobusVentura/MODBUS-CRC16/blob/master/MODBUS_CRC16.c
    uint16_t crc = 0xFFFF;
    uint8_t len = size() - 3; // exclude footer and crc
    for (uint8_t pos = 1; pos < len; pos++)
    {
      crc ^= raw[pos];

      for (uint16_t i = 0; i < 8; i++) {
          if (crc & 1) {
              crc >>= 1;
              crc ^= 0xA001;
          } else {
              crc >>= 1;
          }
      }
    }
    return crc;
  }

  void reset() {
    for (size_t i = 0; i < DATA_FRAME_MAX_SIZE; i++) {
      raw[i] = 0;
    }
  }

  std::vector<uint8_t> get_data() const { return std::vector<uint8_t>(raw, raw + size()); }
};


struct DataFrameReader {
  DataFrame frame;

  bool crc_valid;
  bool complete;
  uint8_t data_index_;

  void reset() {
    frame.reset();
    crc_valid = false;
    data_index_ = 0;
    complete = false;
  }

  bool put(uint8_t byte) {
    if (data_index_ == 0 && byte != DATA_FRAME_START)
      return false;

    frame.raw[data_index_] = byte;
    if (data_index_ > DATA_LENGTH_INDEX && (data_index_ + 1) == frame.size()) {
      // last byte
      if (byte != DATA_FRAME_END) {
        ESP_LOGW("loctekmotion_desk.segment_display", "Unexpected last byte: 0x%02x", byte);
        return false;
      }
      // ESP_LOGD("loctekmotion_desk.segment_display", "Received CRC: 0x%04x, Calculated CRC: 0x%04x", frame.crc(), frame.calculate_crc());
      crc_valid = frame.validate_crc();      
      data_index_ = 0;  // prepare for next frame
      complete = true;
      if (!crc_valid) {
        ESP_LOGW("loctekmotion_desk.segment_display", "CRC not matched!");
      }
      return crc_valid;
    } else {
      data_index_++;
      if (data_index_ == DATA_FRAME_MAX_SIZE) {
        data_index_ = 0;
        ESP_LOGW("loctekmotion_desk.segment_display", "Went over buffer");
      }
      return false;
    }
  }

 private:
};

/* Each segment is controled by the corresponding bit:

  0 0 0
5       1
5       1
5       1
  6 6 6
4       2
4       2
4       2
  3 3 3    77
*/

const uint8_t SEGMENT_SYMBOL_MASK   = 0b01111111;
const uint8_t SEGMENT_DOT_BIT       = 0b10000000;
const uint8_t SEGMENT_OFF           = 0b00000000;
const uint8_t SEGMENT_SYMBOL_0      = 0b00111111;
const uint8_t SEGMENT_SYMBOL_1      = 0b00000110;
const uint8_t SEGMENT_SYMBOL_2      = 0b01011011;
const uint8_t SEGMENT_SYMBOL_3      = 0b01001111;
const uint8_t SEGMENT_SYMBOL_4      = 0b01100110;
const uint8_t SEGMENT_SYMBOL_5      = 0b01101101;
const uint8_t SEGMENT_SYMBOL_6      = 0b01111101;
const uint8_t SEGMENT_SYMBOL_7      = 0b00000111;
const uint8_t SEGMENT_SYMBOL_8      = 0b01111111;
const uint8_t SEGMENT_SYMBOL_9      = 0b01101111;
const uint8_t SEGMENT_SYMBOL_DASH   = 0b01000000;
const uint8_t SEGMENT_SYMBOL_F      = 0b01110001;
const uint8_t SEGMENT_SYMBOL_S      = 0b01101101; // this is the same patter as "5", but using a separate const for better semantics
const uint8_t SEGMENT_SYMBOL_O      = 0b00111111; // this is the same patter as "0", but using a separate const for better semantics
const uint8_t SEGMENT_SYMBOL_N      = 0b00110111;
const uint8_t SEGMENT_SYMBOL_COLON  = 0b00001001;

enum SegmentDisplayState : uint8_t {
  SD_STATE_UNKNOWN = 0,
  SD_STATE_OFF = 1,
  SD_STATE_HEIGHT = 2,
  SD_STATE_MEMORY = 3,
  SD_STATE_TIMER_ON = 4,
  SD_STATE_TIMER_DURATION_ON = 5,
  SD_STATE_TIMER_DURATION_OFF = 6,
  SD_STATE_TIMER_DURATION_ONLY = 7,
  SD_STATE_TIMER_OFF = 8
};

struct SegmentDisplay {
  struct {
    uint8_t segment1;
    uint8_t segment2;
    uint8_t segment3;
  };
};

inline uint8_t segment_to_digit(uint8_t s) {
  uint8_t digit = s & SEGMENT_SYMBOL_MASK;

  switch (digit) {
    case SEGMENT_SYMBOL_0: return 0;
    case SEGMENT_SYMBOL_1: return 1;
    case SEGMENT_SYMBOL_2: return 2;
    case SEGMENT_SYMBOL_3: return 3;
    case SEGMENT_SYMBOL_4: return 4;
    case SEGMENT_SYMBOL_5: return 5;
    case SEGMENT_SYMBOL_6: return 6;
    case SEGMENT_SYMBOL_7: return 7;
    case SEGMENT_SYMBOL_8: return 8;
    case SEGMENT_SYMBOL_9: return 9;
    default:
      return 255;
  }
}

inline bool is_decimal(uint8_t b) { return (b & SEGMENT_DOT_BIT) == SEGMENT_DOT_BIT; }

inline const SegmentDisplayState get_display_state(const struct SegmentDisplay *display) {
  if (display->segment1 == SEGMENT_OFF 
      && display->segment2 == SEGMENT_OFF 
      && display->segment3 == SEGMENT_OFF)
    return SD_STATE_OFF;

  if (display->segment1 == SEGMENT_SYMBOL_S 
      && display->segment2 == SEGMENT_SYMBOL_DASH 
      && display->segment3 == SEGMENT_OFF)
    return SD_STATE_MEMORY;

  if (display->segment1 == SEGMENT_OFF 
      && display->segment2 == SEGMENT_SYMBOL_O 
      && display->segment3 == SEGMENT_SYMBOL_N)
    return SD_STATE_TIMER_ON;


  if (display->segment1 == SEGMENT_SYMBOL_O 
      && display->segment2 == SEGMENT_SYMBOL_F
      && display->segment3 == SEGMENT_SYMBOL_F)
    return SD_STATE_TIMER_OFF;

  if (display->segment1 == SEGMENT_SYMBOL_COLON 
      && display->segment2 == SEGMENT_OFF 
      && display->segment3 == SEGMENT_OFF)
    return SD_STATE_TIMER_DURATION_OFF;

  if (display->segment1 == SEGMENT_SYMBOL_COLON 
      && segment_to_digit(display->segment2) < 10
      && segment_to_digit(display->segment3) < 10)
    return SD_STATE_TIMER_DURATION_ON;

  if (display->segment1 == SEGMENT_OFF 
      && segment_to_digit(display->segment2) < 10
      && segment_to_digit(display->segment3) < 10)
    return SD_STATE_TIMER_DURATION_ONLY;

  if (segment_to_digit(display->segment1) < 10
      && segment_to_digit(display->segment2) < 10
      && segment_to_digit(display->segment3) < 10)
    return SD_STATE_HEIGHT;

  return SD_STATE_UNKNOWN;
}

/**
 * Gets height value from display if state is SD_STATE_HEIGHT
 */
inline float get_display_height(const struct SegmentDisplay *display) {
  if (get_display_state(display) != SD_STATE_HEIGHT) return 0;

  float height = segment_to_digit(display->segment1) * 100 + segment_to_digit(display->segment2) * 10 + segment_to_digit(display->segment3);
  if (is_decimal(display->segment2)) {
    return height / 10;
  }
  return height;
}

/**
 * Gets alarm timer value from display if state is SD_STATE_TIMER_DURATION_ON*
 */
inline uint8_t get_alarm_minutes(const struct SegmentDisplay *display) {
  auto state = get_display_state(display);
  if (state != SD_STATE_TIMER_DURATION_ON
      && state != SD_STATE_TIMER_DURATION_ONLY) return 0;

  uint8_t minutes = segment_to_digit(display->segment2) * 10 + segment_to_digit(display->segment3);
  return minutes;
}

} // namespace loctekmotion_desk
} // namespace esphome
