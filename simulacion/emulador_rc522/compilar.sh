#!/bin/bash
# Compila el emulador del RC522 para el ATmega328P del esquema de Proteus.
# F_CPU debe coincidir con la propiedad "Clock Frequency" del ATmega328P en Proteus.
F_CPU=64000000
avr-gcc -mmcu=atmega328p -DF_CPU=${F_CPU}UL -O2 -Wall -Wextra -std=gnu99 main.c -o emu.elf
avr-objcopy -O ihex emu.elf emulador_rc522.hex
avr-size emu.elf
