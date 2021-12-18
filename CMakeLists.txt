add_executable(cpmega88
        ./src/platform_pico.c
        ./src/machine.c
        ./src/con_base.c
        ./src/con_pico_cdc.c
        ./src/sram_pico.c
        ./src/sdcard_base.c
        ./src/sdcard_pico.c
        ./src/fat.c
        ./src/eeprom_base.c
        ./src/eeprom_pico.c
        ./src/io_pico.c
        ./src/led_pico.c
        )

# pull in common dependencies
target_link_libraries(cpmega88 pico_stdlib)

# use UART
pico_enable_stdio_uart(cpmega88 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(cpmega88)

# add url via pico_set_program_url
example_auto_set_url(cpmega88)
