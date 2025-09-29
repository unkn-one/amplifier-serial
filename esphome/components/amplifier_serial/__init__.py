import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, media_player, sensor, text_sensor
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL, UNIT_PERCENT

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["media_player", "sensor", "text_sensor"]
MULTI_CONF = True

amplifier_serial_ns = cg.esphome_ns.namespace("amplifier_serial")
SerialTransport = amplifier_serial_ns.class_("SerialTransport", uart.UARTDevice)
AmplifierSerial = amplifier_serial_ns.class_(
    "AmplifierSerial", SerialTransport, cg.PollingComponent, media_player.MediaPlayer
)

CONF_SOFTWARE_VERSION_SENSOR = "software_version_sensor"
CONF_MAX_VOLUME_SENSOR = "max_volume_sensor"
CONF_MAX_STREAMING_VOLUME_SENSOR = "max_streaming_volume_sensor"

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(AmplifierSerial),
        cv.Optional(CONF_UPDATE_INTERVAL, default="15s"): cv.update_interval,
        cv.Optional(CONF_SOFTWARE_VERSION_SENSOR): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_MAX_VOLUME_SENSOR): sensor.sensor_schema(),
        cv.Optional(CONF_MAX_STREAMING_VOLUME_SENSOR): sensor.sensor_schema(),
    })
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema('15s'))
    .extend(media_player.MEDIA_PLAYER_SCHEMA)
)

async def to_code(config):
    uart_component = await cg.get_variable(config[uart.CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await media_player.register_media_player(var, config)

    if CONF_UPDATE_INTERVAL in config:
        cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))

    if CONF_SOFTWARE_VERSION_SENSOR in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SOFTWARE_VERSION_SENSOR])
        cg.add(var.set_software_version_sensor(sens))

    if CONF_MAX_VOLUME_SENSOR in config:
        sens = await sensor.new_sensor(config[CONF_MAX_VOLUME_SENSOR])
        cg.add(var.set_max_volume_sensor(sens))

    if CONF_MAX_STREAMING_VOLUME_SENSOR in config:
        sens = await sensor.new_sensor(config[CONF_MAX_STREAMING_VOLUME_SENSOR])
        cg.add(var.set_max_streaming_volume_sensor(sens))
