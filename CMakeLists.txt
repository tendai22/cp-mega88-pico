cmake_minimum_required(VERSION 3.16)
#set(PICO_SDK_PATH "/home/kuma/pico/pico-sdk")
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include(pico_sdk_import.cmake)

project(cpmega88 C CXX ASM)
set(CMAKE_C_STANDART 11)
set(CMAKE_CXX_STANDART 17)

pico_sdk_init()

include_directories(${PICO_SDK_PATH})

add_executable(cpmega88
        ./src/platform_pico.c
        ./src/machine.c
        ./src/platform_pico.c
        ./src/machine.c
        ./src/cpu_8080.c
        ./src/con_base.c
        ./src/con_pico.c
        ./src/sram_pico.c
        ./src/sdcard_base.c
#        ./src/sdcard_picoflash.c
        ./src/sdcard_pico.c
        ./src/fat.c
        ./src/eeprom_base.c
        ./src/eeprom_image.c
        ./src/eeprom_pico.c
        ./src/io_pico.c
        ./src/led_pico.c
#        ./src/xmodem.c
#        ./src/crc16.c
        )

target_compile_options(cpmega88
        PUBLIC -DMCU_PICO
        PUBLIC -DCPU_EMU_C
#        PUBLIC -DUSE_FLASH
#        PUBLIC -DUSE_XMODEM
        PUBLIC -DSDCARD_CMD_DUMP
        PUBLIC -DMON_MEM
#        PUBLIC -DCHK_SDC
        PUBLIC -DCHK_FAT
        PUBLIC -DUSE_EXFAT
        PUBLIC -DSDCARD_DEBUG
#        PUBLIC -DUSE_UART
        )


# pull in common dependencies
target_link_libraries(cpmega88
        pico_stdlib
        hardware_flash
        hardware_spi
#        pico_stdio_uart
        pico_stdio_usb
        )

# use UART
pico_enable_stdio_uart(cpmega88 0)
# use USB
pico_enable_stdio_usb(cpmega88 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(cpmega88)

# add url via pico_set_program_url
#example_auto_set_url(cpmega88)
