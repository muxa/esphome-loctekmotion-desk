#pragma once

#include "segment_display.h"
#include "esphome/core/component.h"

namespace esphome {
namespace loctekmotion_desk {

enum DeskControlState : uint8_t {
  DC_STATE_UNKNOWN = 0,
  DC_STATE_OFF = 1,
  DC_STATE_MEMORY = 2,
  DC_STATE_HEIGHT = 3,
  DC_STATE_MOVING = 4,
  DC_STATE_TIMER_STARTING = 5,
  DC_STATE_TIMER_CHANGE = 6,
  DC_STATE_TIMER_ON = 7,
  DC_STATE_TIMER_OFF = 8,
  DC_STATE_TIMER_MOVING = 9,
  DC_STATE_TIMER_DONE = 10,
};

using DeskControlTrigger = SegmentDisplayState;

class DeskStateMachine {
public:
    DeskStateMachine();
    float height() {
      return height_current_;
    }
    void set_height(const float height) {
      height_current_ = height;
    }
    uint8_t timer_duration() {
      return timer_duration_current_;
    }
    void set_timer_duration(const uint8_t minutes) {
      timer_duration_current_ = minutes;
    }
    bool transition(DeskControlTrigger trigger);
    DeskControlState current_state() { return this->current_state_; }

private:
    bool has_height_changed() const {
      return height_current_ != height_previous_;
    }
    bool is_timer_done() const {
      return timer_duration_current_ == 0;
    }
    DeskControlState current_state_ = DC_STATE_UNKNOWN;

    float height_current_ = 0;
    float height_previous_ = 0;
    uint8_t timer_duration_current_ = 0;
    uint8_t timer_duration_previous_ = 0;
};

} // namespace loctekmotion_desk
} // namespace esphome
