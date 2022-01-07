add_executable(cpmega88
        ./src/platform_pico.c
        ./src/machine.c
        ./src/cpu_8080.c
        ./src/con_base.c
        ./src/con_pico.c
        ./src/sram_pico.c
        ./src/sdcard_base.c
        ./src/sdcard_pico.c
        ./src/fat.c
        ./src/eeprom_base.c
        ./src/eeprom_image.c
        ./src/eeprom_pico.c
        ./src/io_pico.c
        ./src/led_pico.c
        )

target_compile_options(cpmega88
        PUBLIC -DMCU_PICO
        PUBLIC -DCPU_EMU_C
        PUBLIC -DUSE_FAT
        PUBLIC -DMON_FAT
        )


# pull in common dependencies
target_link_libraries(cpmega88 pico_stdlib
        hardware_flash
        hardware_spi
        pico_stdio_uart
        )

# use UART
pico_enable_stdio_uart(cpmega88 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(cpmega88)

# add url via pico_set_program_url
example_auto_set_url(cpmega88)
