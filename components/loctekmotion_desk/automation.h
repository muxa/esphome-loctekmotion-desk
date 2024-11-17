#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "desk.h"

namespace esphome
{
  namespace loctekmotion_desk
  {
    class LoctekMotionOnTimerDoneTrigger : public Trigger<>
    {
    public:
      LoctekMotionOnTimerDoneTrigger(LoctekMotionComponent *desk)
      {
        desk->add_on_timer_done_callback(
            [this]()
            {
              this->stop_action(); // stop any previous running actions
              this->trigger();
            });
      }
    };

    template <typename... Ts>
    class LoctekMotionSetTimerAction : public Action<Ts...>, public Parented<LoctekMotionComponent>
    {
    public:
      LoctekMotionSetTimerAction(uint8_t duration)
      {
        this->duration_ = duration;
      }

      void play(Ts... x) override { this->parent_->set_timer_duration(this->duration_); }

    protected:
      uint8_t duration_;
    };
  } // namespace loctekmotion_desk
} // namespace esphome