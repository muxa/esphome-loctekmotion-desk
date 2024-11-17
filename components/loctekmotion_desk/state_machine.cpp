#include "state_machine.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace loctekmotion_desk {

static const char *const TAG = "loctekmotion_desk.state_machine";

const LogString* segment_display_state_to_string(SegmentDisplayState state) {
  switch (state) {
    case SD_STATE_OFF:
      return LOG_STR("OFF");
    case SD_STATE_HEIGHT:
      return LOG_STR("HEIGHT");
    case SD_STATE_MEMORY:
      return LOG_STR("MEMORY");
    case SD_STATE_TIMER_ON:
      return LOG_STR("TIMER_ON");
    case SD_STATE_TIMER_DURATION_ON:
      return LOG_STR("TIMER_DURATION_ON");
    case SD_STATE_TIMER_DURATION_OFF:
      return LOG_STR("TIMER_DURATION_OFF");
    case SD_STATE_TIMER_DURATION_ONLY:
      return LOG_STR("TIMER_DURATION_ONLY");
    case SD_STATE_TIMER_OFF:
      return LOG_STR("TIMER_OFF");
    default:
      return LOG_STR("UNKNOWN");
  }
}

const LogString* desk_control_state_to_string(DeskControlState state) {
    switch (state) {
        case DC_STATE_OFF: return LOG_STR("OFF");
        case DC_STATE_MEMORY: return LOG_STR("MEMORY");
        case DC_STATE_HEIGHT: return LOG_STR("HEIGHT");
        case DC_STATE_MOVING: return LOG_STR("MOVING");
        case DC_STATE_TIMER_STARTING: return LOG_STR("TIMER_STARTING");
        case DC_STATE_TIMER_CHANGE: return LOG_STR("TIMER_CHANGE");
        case DC_STATE_TIMER_ON: return LOG_STR("TIMER_ON");
        case DC_STATE_TIMER_OFF: return LOG_STR("TIMER_OFF");
        case DC_STATE_TIMER_MOVING: return LOG_STR("TIMER_MOVING");
        case DC_STATE_TIMER_DONE: return LOG_STR("TIMER_DONE");
        default: return LOG_STR("UNKNOWN");
    }
}

// ---------------------------------

DeskStateMachine::DeskStateMachine() {}

bool DeskStateMachine::transition(DeskControlTrigger trigger) {
    DeskControlState new_state = DC_STATE_UNKNOWN;

    switch (current_state_) {
        case DC_STATE_UNKNOWN:
            if (trigger == SD_STATE_OFF) {
                new_state = DC_STATE_OFF;
            } else if (trigger == SD_STATE_TIMER_DURATION_ON || trigger == SD_STATE_TIMER_DURATION_ONLY) {
                new_state = DC_STATE_TIMER_ON;
            }
            break;
        case DC_STATE_OFF:
            if (trigger == SD_STATE_OFF) {
                new_state = current_state_;
            } else if (trigger == SD_STATE_MEMORY) {
                new_state = DC_STATE_MEMORY;
            } else if (trigger == SD_STATE_HEIGHT) {
                new_state = DC_STATE_HEIGHT;
            } else if (trigger == SD_STATE_TIMER_ON) {
                new_state = DC_STATE_TIMER_STARTING;
            }
            break;

        case DC_STATE_MEMORY:
            if (trigger == SD_STATE_MEMORY) {
                new_state = current_state_;
            } else if (trigger == SD_STATE_HEIGHT) {
                new_state = DC_STATE_HEIGHT;
            }
            break;

        case DC_STATE_HEIGHT:
            if (trigger == SD_STATE_OFF) {
                new_state = DC_STATE_OFF;
            } else if (trigger == SD_STATE_MEMORY) {
                new_state = DC_STATE_MEMORY;
            } else if (trigger == SD_STATE_HEIGHT) {
                if (has_height_changed()) {
                    new_state = DC_STATE_MOVING;
                } else {
                  new_state = current_state_;
                }
            } else if (trigger == SD_STATE_TIMER_ON) {
                new_state = DC_STATE_TIMER_STARTING;
            } else if (trigger == SD_STATE_TIMER_DURATION_ON) {
                new_state = DC_STATE_TIMER_ON;
            } else if (trigger == SD_STATE_TIMER_DURATION_ONLY) {
                new_state = DC_STATE_TIMER_ON;
            } else if (trigger == SD_STATE_MEMORY) {
                new_state = DC_STATE_MEMORY;
            }
            break;

        case DC_STATE_MOVING:
            if (trigger == SD_STATE_HEIGHT) {
              if (!has_height_changed()) {
                new_state = DC_STATE_HEIGHT;
              } else {
                new_state = current_state_;
              }
            }
            break;

        case DC_STATE_TIMER_STARTING:
            if (trigger == SD_STATE_TIMER_ON) {
                new_state = current_state_;
            } else if (trigger == SD_STATE_TIMER_DURATION_ON || trigger == SD_STATE_TIMER_DURATION_OFF) {
                new_state = DC_STATE_TIMER_CHANGE;
            }
            break;

        case DC_STATE_TIMER_CHANGE:
            if (trigger == SD_STATE_TIMER_DURATION_ON || trigger == SD_STATE_TIMER_DURATION_OFF) {
                new_state = current_state_;
            } else if (trigger == SD_STATE_TIMER_DURATION_ONLY) {
                new_state = DC_STATE_TIMER_ON;
            } else if (trigger == SD_STATE_TIMER_OFF) {
                new_state = DC_STATE_TIMER_OFF;
            }
            break;

        case DC_STATE_TIMER_ON:
            if ((trigger == SD_STATE_TIMER_DURATION_ON || trigger == SD_STATE_TIMER_DURATION_ONLY) && is_timer_done()) {
                new_state = DC_STATE_TIMER_DONE;
            } else if (trigger == SD_STATE_TIMER_DURATION_ON || trigger == SD_STATE_TIMER_DURATION_ONLY) {
                new_state = current_state_;
            } else if (trigger == SD_STATE_TIMER_DURATION_OFF) {
                new_state = DC_STATE_TIMER_CHANGE;
            } else if (trigger == SD_STATE_HEIGHT) {
                new_state = DC_STATE_TIMER_MOVING;
            } else if (trigger == SD_STATE_TIMER_OFF) {
                new_state = DC_STATE_TIMER_OFF;
            } else if (trigger == SD_STATE_MEMORY) {
                new_state = DC_STATE_MEMORY;
            }
            break;

        case DC_STATE_TIMER_MOVING:
            if (trigger == SD_STATE_HEIGHT) {
                new_state = current_state_;
            } else if (trigger == SD_STATE_TIMER_DURATION_ON || trigger == SD_STATE_TIMER_DURATION_ONLY) {
                new_state = DC_STATE_TIMER_ON;
            }
            break;

        case DC_STATE_TIMER_DONE:
            if (trigger == SD_STATE_TIMER_DURATION_ON || trigger == SD_STATE_TIMER_DURATION_ONLY) {
                new_state = current_state_;
            } else if (trigger == SD_STATE_HEIGHT) {
                new_state = DC_STATE_HEIGHT;
            } else if (trigger == SD_STATE_TIMER_DURATION_OFF) {
                new_state = DC_STATE_TIMER_CHANGE;
            } else if (trigger == SD_STATE_TIMER_OFF) {
                new_state = DC_STATE_TIMER_OFF;
            }
            break;

        case DC_STATE_TIMER_OFF:
            if (trigger == SD_STATE_TIMER_OFF) {
                new_state = current_state_;
            } else if (trigger == SD_STATE_HEIGHT) {
                new_state = DC_STATE_HEIGHT;
            }
            break;
    }

    if (trigger == SD_STATE_HEIGHT) {
      height_previous_ = height_current_;
    }

    if (new_state == DC_STATE_UNKNOWN) {
      ESP_LOGW(TAG, "No transition available from %s on %s", LOG_STR_ARG(desk_control_state_to_string(current_state_)),
                LOG_STR_ARG(segment_display_state_to_string(trigger)));
    } else if (new_state != current_state_) {      
      ESP_LOGI(TAG, "Control state changed from %s to %s on trigger %s", LOG_STR_ARG(desk_control_state_to_string(current_state_)),
                LOG_STR_ARG(desk_control_state_to_string(new_state)), LOG_STR_ARG(segment_display_state_to_string(trigger)));
      current_state_ = new_state;
      return true;
    }

    return false;
}

} // namespace loctekmotion_desk
} // namespace esphome
