
flash-write: blink.hex
	avrdude -B 4Mhz -c atmelice_isp -p m328p -U flash:w:eink-heltec.hex:i

blink.hex: blink.elf
	avr-objcopy -O ihex eink-heltec.elf eink-heltec.hex

blink.elf: eink-heltec.c hello.c
	avr-gcc -O2 -DF_CPU=8000000 -mmcu=atmega328 -g eink-heltec.c hello.c -o eink-heltec.elf

hello.c: png2c.scm hello.png
	csi -s png2c.scm
