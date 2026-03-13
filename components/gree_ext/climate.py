import esphome.codegen as cg
from esphome.components import climate_ir, remote_base, switch
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.const import (
    CONF_MODEL,
    CONF_IDLE,
    CONF_LIGHT,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_LIGHTBULB,
    ICON_HEATING_COIL,
)

from . import gree_ext_ns

CODEOWNERS = ["@ryanh7"]

AUTO_LOAD = ["climate_ir", "switch"]
GreeClimate = gree_ext_ns.class_("GreeClimate", climate_ir.ClimateIR)
GreeModeBitSwitch = gree_ext_ns.class_("GreeModeBitSwitch", switch.Switch, cg.Component)

Model = gree_ext_ns.enum("Model")
MODELS = {
    'yap0f': Model.GREE_YAP0F,
}

CONF_SUPPORTS_HORIZONTAL_SWING = "supports_horizontal_swing"
CONF_SUPPORTS_VERTICAL_SWING = "supports_vertical_swing"
CONF_XFAN = "xfan"

SWITCH_CONFIGS = (
    (CONF_LIGHT, "Gree Light Switch", 0x20, ICON_LIGHTBULB, "RESTORE_DEFAULT_ON"),
    (CONF_XFAN, "Gree X-FAN Switch", 0x80, ICON_HEATING_COIL, "RESTORE_DEFAULT_OFF"),
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(GreeClimate).extend(
    {
        cv.Required(CONF_MODEL): cv.enum(MODELS, lower=True),
        cv.Optional(CONF_SUPPORTS_HORIZONTAL_SWING, default=True): cv.boolean,
        cv.Optional(CONF_SUPPORTS_VERTICAL_SWING, default=True): cv.boolean,
        **{
            cv.Optional(key): switch.switch_schema(
                GreeModeBitSwitch,
                icon=icon,
                default_restore_mode=default,
                device_class=DEVICE_CLASS_SWITCH,
                entity_category=ENTITY_CATEGORY_CONFIG,
            )
            for key, _, _, icon, default in SWITCH_CONFIGS
        },
    }
)

def _validate_idle(config):
    if remote_base.CONF_RECEIVER_ID not in config:
        return

    full_config = fv.full_config.get()
    receiver_path = full_config.get_path_for_id(config[remote_base.CONF_RECEIVER_ID])[:-1]
    receiver_conf = full_config.get_config_for_path(receiver_path)
    idle = receiver_conf[CONF_IDLE]
    min_idle = cv.TimePeriodMicroseconds(milliseconds=20)
    best_idle = cv.TimePeriodMicroseconds(milliseconds=40)
    if not idle or idle < min_idle:
        raise cv.Invalid(
            f"IR receiver idle time ({idle}) is too low. "
            f"Minimum {min_idle} required, {best_idle} recommended."
        )


FINAL_VALIDATE_SCHEMA = _validate_idle


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_supports_horizontal_swing(config[CONF_SUPPORTS_HORIZONTAL_SWING]))
    cg.add(var.set_supports_vertical_swing(config[CONF_SUPPORTS_VERTICAL_SWING]))
    cg.add(var.set_model(config[CONF_MODEL]))

    for conf_key, name, bit_mask, _, _ in SWITCH_CONFIGS:
        if switch_conf := config.get(conf_key):
            sw = cg.new_Pvariable(switch_conf[cv.CONF_ID], name, bit_mask)
            await switch.register_switch(sw, switch_conf)
            await cg.register_component(sw, switch_conf)
            await cg.register_parented(sw, var)
            cg.add(var.register_listener(sw))
