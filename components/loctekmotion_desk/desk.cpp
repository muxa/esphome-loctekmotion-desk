#include "desk.h"
#include "automation.h"
#include "esphome/core/log.h"

namespace esphome {
namespace loctekmotion_desk {

static const char *const TAG = "loctekmotion_desk";

void log_data_frame(const struct DataFrame *frame, size_t length = 0) {
  std::string res;
  char buf[5];
  size_t len = length > 0 ? length : frame->size();
  for (size_t i = 0; i < len; i++) {
    if (i > 0) {
      res += ':';
    }
    sprintf(buf, "%02X", frame->raw[i]);
    res += buf;
  }
  ESP_LOGD(TAG, "Data Frame: %s", res.c_str());
}

const std::string desk_control_state_to_string(DeskControlState state) {
    switch (state) {
        case DC_STATE_OFF: return "OFF";
        case DC_STATE_MEMORY: return "MEMORY";
        case DC_STATE_HEIGHT: return "HEIGHT";
        case DC_STATE_MOVING: return "MOVING";
        case DC_STATE_TIMER_STARTING: return "TIMER_STARTING";
        case DC_STATE_TIMER_CHANGE: return "TIMER_CHANGE";
        case DC_STATE_TIMER_ON: return "TIMER_ON";
        case DC_STATE_TIMER_OFF: return "TIMER_OFF";
        case DC_STATE_TIMER_MOVING: return "TIMER_MOVING";
        case DC_STATE_TIMER_DONE: return "TIMER_DONE";
        default: return "UNKNOWN";
    }
}

// ---------------------------------

LoctekMotionComponent::LoctekMotionComponent(uart::UARTComponent *uart) : uart::UARTDevice(uart) {

}

void LoctekMotionComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Loctek Motion Desk");
  LOG_BINARY_SENSOR("  ", "Connection", this->connected_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Moving", this->moving_binary_sensor_);
  this->check_uart_settings(9600);
}

void LoctekMotionComponent::setup() { }

void LoctekMotionComponent::loop() {
  uint8_t incoming_byte;
  while (available() > 0) {
    if (read_byte(&incoming_byte)) {
      this->last_packet_time_ = millis();

      if (data_reader.put(incoming_byte)) {
        // packet complete
        //log_raw_data("Packet: ", data_reader.frame.raw, data_reader.data_index_)
        auto frame = data_reader.frame;

        if (frame.type == 0x11 || frame.type == 0x15) // skip unknown packet
          return;

        if (frame.type == DATA_TYPE_DISPLAY) {

          bool display_changed = display.segment1 != frame.data[0]
            ||  display.segment2 != frame.data[1]
            ||  display.segment3 != frame.data[2];

          // 9B:04:14:7F:03:9D when alarm beeped
          // 9B:04:81:10:C3:9D 15 seconds after alarm beeped

          if (!display_changed) {
            auto display_state = get_display_state(&display);
            auto last_triggerred = millis() - desk_control_trigger_timestamps[display_state];
            if (last_triggerred < 1000) {
              // changed less then 1 second ago. don't retrigger
              return;
            }
          }

          display.segment1 = frame.data[0];
          display.segment2 = frame.data[1];
          display.segment3 = frame.data[2];

          auto display_state = get_display_state(&display);

          desk_control_trigger_timestamps[display_state] = millis();

          if (display_state != last_display_state) {

            if (display_state == SD_STATE_UNKNOWN) {
              log_data_frame(&frame);
            }
          }

          last_display_state = display_state;
          auto previous_duration = state_machine.timer_duration();

          switch (display_state)
          {
          case SD_STATE_HEIGHT:
            {
              float height = get_display_height(&display);
              state_machine.set_height(height);

              if (this->height_sensor_ && this->height_sensor_->state != height) {
                this->height_sensor_->publish_state(height);
                // this is needed to stop multiple publish calls, because publish is delayed:
                this->height_sensor_->state = height;
              }
            }
            break;
          
          case SD_STATE_TIMER_DURATION_ON:
          case SD_STATE_TIMER_DURATION_ONLY:
            {
              state_machine.set_timer_duration(get_alarm_minutes(&display));
            }
            break;

          case SD_STATE_TIMER_OFF:
            {
              state_machine.set_timer_duration(0);
            }
            break;
          default:
            break;
          }


          if (state_machine.transition(display_state)) {
            // state changed
            this->update_control_status_text_sensor_();

            switch (state_machine.current_state()) {
              case DC_STATE_OFF:
                is_timer_active_ = false;
                if (this->timer_active_binary_sensor_) {    
                  this->timer_active_binary_sensor_->publish_state(false);
                }
                break;
              case DC_STATE_TIMER_ON:
                is_timer_active_ = true;
                if (this->timer_active_binary_sensor_) {    
                  this->timer_active_binary_sensor_->publish_state(true);
                }
                if (timer_target_duration_ > 0 && timer_target_duration_ != state_machine.timer_duration()) {
                  // change duration
                  this->set_timer_duration(timer_target_duration_);
                } else {
                  // timer started
                  this->start_calculated_timer_duration_();
                }
                break;
              case DC_STATE_TIMER_STARTING:
              case DC_STATE_TIMER_CHANGE:
              case DC_STATE_HEIGHT:
                if (timer_target_duration_ > 0) {
                  this->set_timer_duration(timer_target_duration_);
                }
                break;
              case DC_STATE_TIMER_DONE:
                this->timer_done_callback_.call();
                break;
              case DC_STATE_TIMER_OFF:
                is_timer_active_ = false;
                if (this->timer_active_binary_sensor_) {    
                  this->timer_active_binary_sensor_->publish_state(false);
                }
                this->update_calculated_timer_duration_();
                break;
              default:
                break;
            }
          } else {
            switch (state_machine.current_state()) {
              case DC_STATE_TIMER_CHANGE:
                {
                  auto current_duration = state_machine.timer_duration();
                  if (timer_target_duration_ > 0 && current_duration != previous_duration) {
                    // duration on screen just changed. 
                    if (current_duration != timer_target_duration_) {
                      // wait a bit and press the button again to reach the target
                      this->set_timeout(108, [this]() { this->set_timer_duration(this->timer_target_duration_); });
                    } else {
                      // final call to start the timer
                      this->set_timer_duration(this->timer_target_duration_);
                    }
                  }
                }
                break;
              case DC_STATE_TIMER_ON:
                {
                  auto current_duration = state_machine.timer_duration();
                  if (current_duration != previous_duration) {
                    // duration on screen just changed. sync calculated timer duration in case it drifted
                    this->start_calculated_timer_duration_();
                    ESP_LOGD(TAG, "Timer display changed to %d minutes", current_duration);
                  }
                }
                break;
              default:
                break;
            }
          }

          this->update_moving_binary_sensor_();
        } else {
          log_data_frame(&frame);
        }

        data_reader.reset();
      }
    }
  }

  this->update_connected_binary_sensor_();  

  if (state_machine.current_state() == DC_STATE_TIMER_ON || state_machine.current_state() == DC_STATE_TIMER_DONE) {
    this->update_calculated_timer_duration_();
  }
}

void LoctekMotionComponent::update_connected_binary_sensor_() {
  if (this->connected_binary_sensor_) {
    uint32_t millis_since_last_packet = millis() - this->last_packet_time_;
    if (millis_since_last_packet >= 1000) {
      // disconnected
      if (this->connected_binary_sensor_->state) {
        this->connected_binary_sensor_->publish_state(false);
        // this is needed to stop multiple publish calls, because publish is delayed:
        this->connected_binary_sensor_->state = false;
      }
    } else {
      // connected
      if (!this->connected_binary_sensor_->state) {
        this->connected_binary_sensor_->publish_state(true);
        // this is needed to stop multiple publish calls, because publish is delayed:
        this->connected_binary_sensor_->state = true;
      }
    }
  }
}

void LoctekMotionComponent::update_moving_binary_sensor_() {
  if (this->moving_binary_sensor_) {
    bool currently_moving = state_machine.current_state() == DC_STATE_MOVING
                          || state_machine.current_state() == DC_STATE_TIMER_MOVING;
    this->moving_binary_sensor_->publish_state(currently_moving);
  }
}

void LoctekMotionComponent::update_control_status_text_sensor_() {
  if (this->control_status_text_sensor_) {
    std::string state = desk_control_state_to_string(state_machine.current_state());
    if (this->control_status_text_sensor_->state != state) {
      this->control_status_text_sensor_->publish_state(state);
      // this is needed to stop multiple publish calls, because publish is delayed:
      this->control_status_text_sensor_->state = state;
    }
  }
}

void LoctekMotionComponent::start_calculated_timer_duration_() {
  timer_start_time_ = millis();
  timer_total_seconds_ = state_machine.timer_duration() * 60;
}

void LoctekMotionComponent::update_calculated_timer_duration_() {
  if (!this->timer_sensor_)
    return;
  uint32_t timer = state_machine.timer_duration();
  uint32_t timer_remaining = timer > 0 ? (timer_total_seconds_ - (millis() - timer_start_time_)/ 1000) : 0;
  if (this->timer_sensor_->state != timer_remaining) {
    this->timer_sensor_->publish_state(timer_remaining);
    // this is needed to stop multiple publish calls, because publish is delayed:
    this->timer_sensor_->state = timer_remaining;
  }
}

void LoctekMotionComponent::set_timer_duration(uint8_t duration) {
  auto state = this->state_machine.current_state();
  switch (state) {
    case DC_STATE_TIMER_OFF:
      // wait for the Height state and then go into the Timer state (in the loop())
      ESP_LOGD(TAG, "Waiting for HEIGHT state to set timer to %d minutes", duration);
      break;
    case DC_STATE_TIMER_STARTING:
      ESP_LOGD(TAG, "Waiting for TIMER_CHANGE state to set timer to %d minutes", duration);
      break;
    case DC_STATE_OFF:
    case DC_STATE_TIMER_ON:
    case DC_STATE_TIMER_DONE:
      // press A button and wait for the timer change state
      ESP_LOGD(TAG, "Waiting for TIMER_CHANGE state to set timer to %d minutes", duration);
      timer_target_duration_ = duration; // this will instruct the start the logic of changing the duration once in TIMER_CHANGE state
      this->timer_button_->press(); // this will enter the TIMER_CHANGE state
      break;
    case DC_STATE_MEMORY:
    case DC_STATE_HEIGHT:
    case DC_STATE_MOVING:
    case DC_STATE_TIMER_MOVING:
      // wait, as these states will eventually change to one above and till lead into TIMER_CHANGE state
      ESP_LOGD(TAG, "Waiting for TIMER_CHANGE state to set timer to %d minutes", duration);
      timer_target_duration_ = duration; // this will instruct the start the logic of changing the duration once in TIMER_CHANGE state
      break;
    case DC_STATE_TIMER_CHANGE: 
      {
        timer_target_duration_ = duration; // in case we changed the duration again before it reached the old target
        auto current_duration = state_machine.timer_duration();
        if (duration > current_duration) {
          ESP_LOGD(TAG, "Timer is %d (target: %d)", current_duration, duration);
          this->up_button_->press();
        } else if (duration < current_duration) {
          ESP_LOGD(TAG, "Timer is %d (target: %d)", current_duration, duration);
          this->down_button_->press();
        } else {
          // correct duration is already set
          timer_target_duration_ = 0;
          ESP_LOGI(TAG, "Timer was set to %d minutes", duration);
          this->timer_button_->press(); // start timer
          return;
        }
      }
      break;
    default:
      break;
  }
}

}  // namespace loctekmotion_desk
}  // namespace esphome
