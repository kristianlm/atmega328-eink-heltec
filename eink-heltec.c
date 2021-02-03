#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/io.h>

#define SPI_SET_SS			PORTB |=  (1 << PORTB2)
#define SPI_LOWER_SS			PORTB &= ~(1 << PORTB2)

#define _ON(name,pin)   PORT##name |=  (1<<P##name##pin); DDR##name |=  (1<<P##name##pin);
#define _OFF(name,pin)  PORT##name &= ~(1<<P##name##pin); DDR##name &= ~(1<<P##name##pin);

void spi_init(void) {
  //         enable      master               speed
  SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);

  // outputs:    SS      MOSI        SCK
  DDRB =      (1<<DDB2)|(1<<DDB3)|(1<<DDB5);

  SPI_SET_SS;
}

void spi_send(uint8_t data) {
  SPI_LOWER_SS; // indicate start of SPI byte transfer to SPI slave
  _ON(C, 1); // my indicator led
  {
    SPDR = data;
    while(!(SPSR & (1 << SPIF)));
  }
  _OFF(C, 1); // my indicator led
  SPI_SET_SS;
}

void epdRefreshWait() {
  _delay_ms(100);
  DDRD &= ~(1<<DDD7); // PD7 as input
  while(PIND & (1<<PD7) == 0) { // pulled low by eink module => busy
    _ON(C, 0);  _delay_ms(1); _OFF(C, 0); _delay_ms(50);
  }
  _delay_ms(1000);
}

void sendCommand(unsigned char command) {
  _OFF(B, 0); // D/C
  spi_send(command);
}

void sendData(unsigned char data) {
  _ON(B, 0); // D/C
  spi_send(data);
}

#define PA1 _delay_ms(120);
#define PA3 PA1; PA1; PA1;
#define PAL PA3; PA3;
#define DIH _ON(C,0);  PA1; _OFF(C,0); PA1;
#define DAH _ON(C,0);  PA3; _OFF(C,0); PA1;

#define mI DIH; DIH; PAL;
#define mK DAH; DIH; DAH; PAL;

void epdRefresh() {
  _delay_ms(100);
  sendCommand(0x12); // display refresh
  _delay_ms(100);
  epdRefreshWait();
}

void epdInit() {
  // based on the flowchart in the data sheet
  sendCommand(0x06); // booster soft start
  sendData(0x17);
  sendData(0x17);
  sendData(0x17);

  /* sendCommand(0x01); */ // power setting
  /* sendData(0x03); */
  /* sendData(0x00); */
  /* sendData(0x26); */
  /* sendData(0x26); */
  /* sendData(0x03); */

  sendCommand(0x04); // power on

  epdRefreshWait();

  sendCommand(0X00); // panel setting
  sendData(0x0f); // someone online suggests 0x8F here

  sendCommand(0x61); // resolution TRES
  sendData(0x68); // 104 HRES
  sendData(0x00);
  sendData(0xd4); // 212 VRES

  sendCommand(0X50); // vcom and data interval
  sendData(0x77); // data sheet says default is: 0xd7, stm32 sample code says 0x77
}

// what does that even mean on an EInk??
void epdOff() {
  sendCommand(0x02);
}

extern const uint8_t black[] PROGMEM;
extern const uint8_t red[] PROGMEM;

void main() {
  DIH; DIH; DIH; // indicate something is happening

  spi_init();

  epdInit();
  sendCommand(0x10); // start data transmission 1 (black/white)
  for(int i = 0 ; i < 212 * 13 ; i++) {
    sendData(pgm_read_byte(black + i));
  }
  sendCommand(0x13); // start data transmission 2 (red)
  for(int i = 0 ; i < 212 * 13 ; i++) {
    sendData(pgm_read_byte(red + i));
  }
  epdRefresh(); // issue display update (this should take BUSY high for about 5s)
  epdOff();

  while(1) { // my "idle" LED indication
    _ON(C, 0); _delay_ms(10); _OFF(C, 0); _delay_ms(1000);
  }
}
