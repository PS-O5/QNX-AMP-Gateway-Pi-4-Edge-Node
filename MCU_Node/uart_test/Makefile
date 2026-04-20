CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
CFLAGS = -mcpu=cortex-m4 -mthumb -Wall -g -O0
LDFLAGS = -T stm32_flash.ld -nostdlib

all: main.bin

main.elf: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) main.c -o main.elf

main.bin: main.elf
	$(OBJCOPY) -O binary main.elf main.bin

flash: main.bin
	st-flash write main.bin 0x08000000

clean:
	rm -f *.o *.elf *.bin
