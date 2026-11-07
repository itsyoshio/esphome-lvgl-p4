"""Remembers why the last boot ended (task watchdog: blocked tasks, current component, app context)."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.esp32 import add_idf_sdkconfig_option, include_builtin_idf_component
from esphome.const import CONF_ID

CODEOWNERS = ["@itsyoshio"]
DEPENDENCIES = ["esp32"]

CONF_COREDUMP = "coredump"

crash_info_ns = cg.esphome_ns.namespace("crash_info")
CrashInfo = crash_info_ns.class_("CrashInfo", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CrashInfo),
        # needs a "coredump" data partition (esp32: partitions:)
        cv.Optional(CONF_COREDUMP, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if config[CONF_COREDUMP]:
        cg.add_define("USE_CRASH_INFO_COREDUMP")
        include_builtin_idf_component("espcoredump")
        add_idf_sdkconfig_option("CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH", True)
        add_idf_sdkconfig_option("CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF", True)
        add_idf_sdkconfig_option("CONFIG_ESP_COREDUMP_CHECKSUM_CRC32", True)
        add_idf_sdkconfig_option("CONFIG_ESP_COREDUMP_MAX_TASKS_NUM", 64)
