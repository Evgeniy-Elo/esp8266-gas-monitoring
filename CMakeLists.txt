cmake_minimum_required(VERSION 4.2)
project(esp8266_gas_monitoring)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(client_firmware)
include_directories(client_firmware_nrf24)
include_directories(server_firmware)
include_directories(server_firmware_nrf24)

add_executable(esp8266_gas_monitoring
        client_firmware/client_firmware.ino
        client_firmware_nrf24/client_firmware_nrf24.ino
        server_firmware/server_firmware.ino
        server_firmware_nrf24/server_firmware_nrf24.ino)

set_source_files_properties(
        client_firmware/client_firmware.ino
        client_firmware_nrf24/client_firmware_nrf24.ino
        server_firmware/server_firmware.ino
        server_firmware_nrf24/server_firmware_nrf24.ino
        PROPERTIES LANGUAGE CXX
)
