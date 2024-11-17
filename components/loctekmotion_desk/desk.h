#pragma once

#include "state_machine.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"
namespace esphome {
namespace loctekmotion_desk {
class LoctekMotionComponent : public Component, public uart::UARTDevice {
 public:
  LoctekMotionComponent(uart::UARTComponent *uart);

  void set_connected_binary_sensor(binary_sensor::BinarySensor *connected_binary_sensor) {
    connected_binary_sensor_ = connected_binary_sensor;
  }

  void set_moving_binary_sensor(binary_sensor::BinarySensor *moving_binary_sensor) {
    moving_binary_sensor_ = moving_binary_sensor;
  }

  void set_timer_active_binary_sensor(binary_sensor::BinarySensor *timer_active_binary_sensor) {
    timer_active_binary_sensor_ = timer_active_binary_sensor;
  }

  void set_control_status_text_sensor(text_sensor::TextSensor *control_status_text_sensor) {
    control_status_text_sensor_ = control_status_text_sensor;
  }

  void set_up_button(button::Button *up_button) {
    up_button_ = up_button;
  }

  void set_down_button(button::Button *down_button) {
    down_button_ = down_button;
  }

  void set_preset1_button(button::Button *preset1_button) {
    preset1_button_ = preset1_button;
  }

  void set_preset2_button(button::Button *preset2_button) {
    preset2_button_ = preset2_button;
  }

  void set_preset3_button(button::Button *preset3_button) {
    preset3_button_ = preset3_button;
  }

  void set_memory_button(button::Button *memory_button) {
    memory_button_ = memory_button;
  }

  void set_timer_button(button::Button *timer_button) {
    timer_button_ = timer_button;
  }

  void set_height_sensor(sensor::Sensor *height_sensor) {
    height_sensor_ = height_sensor;
  }

  void set_timer_sensor(sensor::Sensor *timer_sensor) {
    timer_sensor_ = timer_sensor;
  }

  void add_on_timer_done_callback(std::function<void()> &&callback) { this->timer_done_callback_.add(std::move(callback)); }
  void set_timer_duration(uint8_t duration);

  DeskControlState current_state() {
    return state_machine.current_state();
  }

  void dump_config() override;

  void setup() override;
  void loop() override;

 protected:

  binary_sensor::BinarySensor *connected_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *moving_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *timer_active_binary_sensor_{nullptr};
  button::Button *up_button_{nullptr};
  button::Button *down_button_{nullptr};
  button::Button *preset1_button_{nullptr};
  button::Button *preset2_button_{nullptr};
  button::Button *preset3_button_{nullptr};
  button::Button *memory_button_{nullptr};
  button::Button *timer_button_{nullptr};
  text_sensor::TextSensor *control_status_text_sensor_{nullptr};
  sensor::Sensor *height_sensor_{nullptr};
  sensor::Sensor *timer_sensor_{nullptr};  

  CallbackManager<void()> timer_done_callback_{};

 private:
  void update_connected_binary_sensor_();
  void update_moving_binary_sensor_();
  void update_control_status_text_sensor_();

  void start_calculated_timer_duration_();
  void update_calculated_timer_duration_();

  uint32_t last_packet_time_;
  uint32_t timer_target_duration_; // remember the target timer duration while setting it. 0 = not setting
  uint32_t timer_start_time_; // store time of timer start to calculate elapsed time
  uint32_t timer_total_seconds_; // store total timer duration at the start of the timer to calculate elapsed time
  bool is_timer_active_;
  uint32_t desk_control_trigger_timestamps[SD_STATE_TIMER_OFF+1] = {0};

  DataFrameReader data_reader;
  SegmentDisplay display;
  SegmentDisplayState last_display_state;
  DeskStateMachine state_machine;
};

}  // namespace loctekmotion_desk
}  // namespace esphome
