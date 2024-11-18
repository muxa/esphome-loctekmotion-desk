import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import uart, binary_sensor, text_sensor, sensor, button
from esphome.components.uart import button as uart_button

from esphome.const import (
    CONF_DATA,
    CONF_DURATION,
    CONF_ID,
    CONF_TRIGGER_ID,
    CONF_UART_ID,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_MOVING,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_ARROW_EXPAND_VERTICAL,
    ICON_COUNTER,
    ICON_TIMER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CENTIMETER,
    UNIT_SECOND,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@muxa"]
DEPENDENCIES = ["uart", "button"]
AUTO_LOAD = ["binary_sensor", "text_sensor", "sensor"]

loctekmotion_desk_ns = cg.esphome_ns.namespace("loctekmotion_desk")

LoctekMotionOnTimerDoneTrigger = loctekmotion_desk_ns.class_(
    "LoctekMotionOnTimerDoneTrigger", automation.Trigger.template()
)

LoctekMotionSetTimerAction = loctekmotion_desk_ns.class_("LoctekMotionSetTimerAction", automation.Action)

LoctekMotionComponent = loctekmotion_desk_ns.class_(
    "LoctekMotionComponent", cg.PollingComponent
)

MicronPressAction = loctekmotion_desk_ns.class_("MicronPressAction", automation.Action)

CONF_CONNECTED = "connected"
CONF_MOVING = "moving"
CONF_TIMER_ACTIVE = "timer_active"
CONF_HEIGHT = "height"
CONF_TIMER = "timer"
CONF_CONTROL_STATUS = "control_status"

CONF_ON_TIMER_DONE_ACTION = "on_timer_done"

CONF_UP_BUTTON = "up_button"
CONF_DOWN_BUTTON = "down_button"
CONF_PRESET1_BUTTON = "preset1_button"
CONF_PRESET2_BUTTON = "preset2_button"
CONF_PRESET3_BUTTON = "preset3_button"
CONF_MEMORY_BUTTON = "memory_button"
CONF_TIMER_BUTTON = "timer_button"
CONF_TIMER_SET_ACTION = "timer_set"

ICON_STATE_MACHINE = "mdi:state-machine"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LoctekMotionComponent),
            cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_TIMER_ACTIVE): binary_sensor.binary_sensor_schema(
                icon=ICON_TIMER,
            ),
            cv.Optional(CONF_HEIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_CENTIMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_DISTANCE,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_ARROW_EXPAND_VERTICAL,
            ),
            cv.Optional(CONF_TIMER): sensor.sensor_schema(
                unit_of_measurement=UNIT_SECOND,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_TIMER,
            ),
            cv.Optional(CONF_MOVING): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_MOVING,
            ),
            cv.Optional(CONF_CONTROL_STATUS): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon=ICON_STATE_MACHINE
            ),
            cv.Optional(CONF_UP_BUTTON): uart_button.CONFIG_SCHEMA,
            cv.Optional(CONF_DOWN_BUTTON): uart_button.CONFIG_SCHEMA,
            cv.Optional(CONF_PRESET1_BUTTON): uart_button.CONFIG_SCHEMA,
            cv.Optional(CONF_PRESET2_BUTTON): uart_button.CONFIG_SCHEMA,
            cv.Optional(CONF_PRESET3_BUTTON): uart_button.CONFIG_SCHEMA,
            cv.Optional(CONF_MEMORY_BUTTON): uart_button.CONFIG_SCHEMA,
            cv.Optional(CONF_TIMER_BUTTON): uart_button.CONFIG_SCHEMA,
            cv.Optional(CONF_ON_TIMER_DONE_ACTION): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LoctekMotionOnTimerDoneTrigger),
                }
            ),
        }
    ).extend(uart.UART_DEVICE_SCHEMA)
)

def validate_uart(config):
    uart.final_validate_device_schema(
        "loctekmotion_desk", baud_rate=9600, require_rx=True, require_tx=True
    )(config)


FINAL_VALIDATE_SCHEMA = validate_uart

async def to_code(config):

    cg.add_global(loctekmotion_desk_ns.using)

    uart_component = await cg.get_variable(config[CONF_UART_ID])

    var = cg.new_Pvariable(config[CONF_ID], uart_component)
    await cg.register_component(var, config)

    if  connected_conf := config.get(CONF_CONNECTED):
        sens = await binary_sensor.new_binary_sensor(connected_conf)
        cg.add(var.set_connected_binary_sensor(sens))

    if moving_conf := config.get(CONF_MOVING):
        sens = await binary_sensor.new_binary_sensor(moving_conf)
        cg.add(var.set_moving_binary_sensor(sens))

    if timer_conf := config.get(CONF_TIMER_ACTIVE):
        sens = await binary_sensor.new_binary_sensor(timer_conf)
        cg.add(var.set_timer_active_binary_sensor(sens))

    if control_status_conf := config.get(CONF_CONTROL_STATUS):
        sens = await text_sensor.new_text_sensor(control_status_conf)
        cg.add(var.set_control_status_text_sensor(sens))

    if height_conf := config.get(CONF_HEIGHT):
        sens = await sensor.new_sensor(height_conf)
        cg.add(var.set_height_sensor(sens))

    if timer_conf := config.get(CONF_TIMER):
        sens = await sensor.new_sensor(timer_conf)
        cg.add(var.set_timer_sensor(sens))

    up_button = await new_uart_button(config, CONF_UP_BUTTON)
    if up_button:
        cg.add(var.set_up_button(up_button))

    down_button = await new_uart_button(config, CONF_DOWN_BUTTON)
    if down_button:
        cg.add(var.set_down_button(down_button))

    preset1_button = await new_uart_button(config, CONF_PRESET1_BUTTON)
    if preset1_button:
        cg.add(var.set_preset1_button(preset1_button))

    preset2_button = await new_uart_button(config, CONF_PRESET2_BUTTON)
    if preset2_button:
        cg.add(var.set_preset2_button(preset2_button))

    preset3_button = await new_uart_button(config, CONF_PRESET3_BUTTON)
    if preset3_button:
        cg.add(var.set_preset3_button(preset3_button))

    memory_button = await new_uart_button(config, CONF_MEMORY_BUTTON)
    if memory_button:
        cg.add(var.set_memory_button(memory_button))

    timer_button = await new_uart_button(config, CONF_TIMER_BUTTON)
    if timer_button:
        cg.add(var.set_timer_button(timer_button))

    if actions := config.get(CONF_ON_TIMER_DONE_ACTION, []):
        for action in actions:
            trigger = cg.new_Pvariable(action[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, [], action)

    cg.add(var.dump_config())

async def new_uart_button(config, config_name, *args):
    if button_conf := config.get(config_name):
        var = await button.new_button(button_conf)        
        await uart.register_uart_device(var, config)

        data = button_conf[CONF_DATA]
        if isinstance(data, bytes):
            data = [HexInt(x) for x in data]
        cg.add(var.set_data(data))

        return var
    return None

@automation.register_action(
    "loctekmotion_desk.timer_set",
    LoctekMotionSetTimerAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(LoctekMotionComponent),
            cv.Required(CONF_DURATION): cv.templatable(cv.int_range(1, 99)),
        },
        key=CONF_DURATION,
    ),    
)
async def timer_set_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    duration = await cg.templatable(config[CONF_DURATION], args, int)
    cg.add(var.set_duration(duration))
    return var