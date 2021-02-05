#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/io.h>

#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>


void uart_wait() {
  while(!(UCSR0A & (1 << UDRE0)));
}

void uart_init(uint16_t baud) {
  cli();
  int brr = F_CPU/16/baud - 1;
  UBRR0L = (uint8_t)(brr & 0xFF);
  UBRR0H = (uint8_t)(brr >> 8);
  UCSR0B = (1<<RXEN0)|(1<<TXEN0); //enable receiver and transmitter

  UCSR0C = (0<<USBS0) // 1 stop bit
    |(1<<UPM01)|(0<<UPM00) // even parity
    |(0<<UCSZ02)|(1<<UCSZ01)|(1<<UCSZ00) // 8 data, 2stop bit
    ;
  sei();
}

void uart_putc(uint8_t data) {
  uart_wait(); //while(!(UCSR0A & (1 << UDRE0)));
  UDR0 = data;
}

volatile uint64_t seconds = 0;
//volatile uint8_t minutes = 0;
//volatile uint8_t hours = 0;
ISR(WDT_vect) {
  seconds += 4;
  /* while(seconds > 60) { */
  /*   minutes++; */
  /*   seconds -= 60; */
  /*   while(minutes > 60) { */
  /*     hours++; */
  /*     minutes -= 60; */
  /*   } */
  /* } */
}

void print_pind() {
  //uint8_t in = PINB;
  //char alphabet [] = "0123456789ABCDEF";
  //uart_putc('w');
  //uart_putc(alphabet[((in>>4)&0xF)]);
  uart_putc(((PINB & 1<<PB1) == 0) + '0');//alphabet[((in>>0)&0xF)]);
  //uart_wait();
  //_delay_ms(10);
}

/* // https://www.sciencetronics.com/greenphotons/?p=1521 */
/* #define ADMUX_ADCMASK  ((1 << MUX3)|(1 << MUX2)|(1 << MUX1)|(1 << MUX0)) */
/* #define ADMUX_REFMASK  ((1 << REFS1)|(1 << REFS0)) */
/* #define ADMUX_REF_AREF ((0 << REFS1)|(0 << REFS0)) */
/* #define ADMUX_REF_AVCC ((0 << REFS1)|(1 << REFS0)) */
/* #define ADMUX_REF_RESV ((1 << REFS1)|(0 << REFS0)) */
/* #define ADMUX_REF_VBG  ((1 << REFS1)|(1 << REFS0)) */
/* #define ADMUX_ADC_VBG  ((1 << MUX3)|(1 << MUX2)|(1 << MUX1)|(0 << MUX0)) */
/* /\* // measure supply voltage in mV *\/ */
/* uint16_t ivr_measure(void) { */
/*   ADMUX &= ~(ADMUX_REFMASK | ADMUX_ADCMASK); */
/*   ADMUX |= ADMUX_REF_AVCC;      // select AVCC as reference */
/*   ADMUX |= ADMUX_ADC_VBG;       // measure bandgap reference voltage */
 
/*   _delay_us(500);               // a delay rather than a dummy measurement is needed to give a stable reading! */
/*   ADCSRA |= (1 << ADSC);        // start conversion */
/*   while (ADCSRA & (1 << ADSC)); // wait to finish */
 
/*   return (1100UL*1023/ADC);     // AVcc = Vbg/ADC*1023 = 1.1V*1023/ADC */
/* } */

uint16_t ivr_measure() {
  // initialize the ADC
  ADMUX = (0 << REFS1) | (1 << REFS0)
    | (0 << ADLAR)
    | (1 << MUX3) | (1 << MUX2) | (1 << MUX1) | (0 << MUX0);
  ADCSRA = (1 << ADEN)
    | (0 << ADSC)
    | (0 << ADATE)
    | (1 << ADPS2)|(0 << ADPS1)|(1 << ADPS0);
  _delay_us(500); // let things settle for more stable voltage reading
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC));
 
  uint16_t v = (1100UL*1023/ADC);
  ADCSRA &= ~(1 << ADEN); // turn off ADC to save power
  return v;
}

uint16_t tmp_mV() {
  ADMUX = (1 << REFS1) | (1 << REFS0) // voltage reference
    | (0 << ADLAR) // MSB/LSB
    | (0 << MUX3) | (1 << MUX2) | (0 << MUX1) | (0 << MUX0); // ADC source
  ADCSRA = (1 << ADEN)
    | (0 << ADSC) // enable (sequenced)
    | (0 << ADATE) // auto trigger
    | (1 << ADPS2)|(0 << ADPS1)|(1 << ADPS0); // prescaler
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC));

  uint16_t adc = ADCL + (ADCH<<8); // 0 - 1023
  ADCSRA &= ~(1 << ADEN); // turn off ADC to save power
  return (adc*1.1*(1000.0/1024.0));
}

int16_t tmp_dC() {
  int16_t r = tmp_mV();
  return r - 500;
}

// setup watchdog so that the WDT_vect gets triggered every second.
// this is our poor man's clock.
void setup_watchdog() {
  // 0=16ms, 1=32ms,2=64ms,3=128ms,4=250ms,5=500ms
  // 6=1 sec,7=2 sec, 8=4 sec, 9= 8sec
  uint32_t ii = 8; // must match seconds increment in ISR!

  uint8_t bb;
  if (ii > 9 ) ii=9;
  bb=ii & 7;
  if (ii > 7) bb|= (1<<5);
  bb|= (1<<WDCE);

  MCUSR &= ~(1<<WDRF);
  // start timed sequence
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  // set new watchdog timeout value
  WDTCSR = bb;
  WDTCSR |= _BV(WDIE);
}

// system wakes up when watchdog is timed out
// obs: make sure you setup the watchdog before calling this!
void system_sleep() {
  ADCSRA = 0; // switch Analog to Digitalconverter OFF
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();           // timed sequence follows
  sleep_enable();
  // turn off brown-out enable in software
  //MCUCR = 1<<(BODS) | 1<<(BODSE);  // turn on brown-out enable select
  //MCUCR = 1<<(BODS);        // this must be done within 4 clock cycles of above
  sei();             // guarantees next instruction executed
  sleep_cpu ();              // sleep within 3 clock cycles of above
  sleep_disable(); // System continues execution here when watchdog timed out 
  ADCSRA = 1; // switch Analog to Digitalconverter ON
}
// ================================================================================
// HISTORY

// 1 recording every 10 minutes, 6 times per hour for 24 hours
#define HISTORY_SIZE (24 * 6)
int8_t history[HISTORY_SIZE];
uint16_t history_pos = 0;
void history_insert(int8_t ddeg) {
  history[history_pos++] = ddeg;
  //printf("pos %d/%d\n", history_pos, HISTORY_SIZE);
  if(history_pos >= HISTORY_SIZE) {
    history_pos = 0;
  }
}
int8_t history_get(uint16_t pos) {
  return history[pos];
}

int16_t mapping(int16_t point, int16_t amin, int16_t amax, int16_t bmin, int16_t bmax) {
  int16_t arange = amax-amin;
  int16_t brange = bmax-bmin;
  return ((brange*(point-amin))/arange)+bmin;
}


uint8_t history_getpixel(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  // we have 104 pixels, and want to show degrees from -15 to 35.
  // we can store 0.5 increments, so we double our Celcius scale. We store 35 degrees as 70.
  // this means our temp range goes from -30 to +70.
  // that fits very nicely with our 104 pixels: 100 resolution in degrees is 100 and
  // we have 104 pixels to show it on:
  int8_t yd =   ((104 -   y)-30); // degrees for current y value
  int8_t y0 =   ((104 - (  0+30))); // find the 0C pixel
  int8_t yn10 = ((104 - (-20+30))); /// and then the 20°C pixel position
  int8_t y10 =  ((104 - ( 20+30))); /// and then the 10°C pixel position
  int8_t y20 =  ((104 - ( 40+30))); /// and then the 20°C pixel position
  int8_t y30 =  ((104 - ( 60+30))); /// and then the 20°C pixel position

  int16_t xx = mapping(x, 0, w,  0, HISTORY_SIZE); // history position of current x value
  xx = history_pos - (HISTORY_SIZE - xx);
  while(xx < 0) xx += HISTORY_SIZE;
  int ddeg = history_get(xx);
  int lo = 0<ddeg?0:ddeg;
  int hi = 0<ddeg?ddeg:0;
  uint8_t result = !(yd >= lo && yd <= hi);
  if(y0 == y  &&  (x%2==0)) return !result; // make the 0C line solid
  if(y30 == y &&  (x%20==0)) return !result;
  if(y20 == y &&  (x%10==0)) return !result;
  if(y10 == y &&  (x%20==0)) return !result;
  if(yn10 == y && (x%20==0)) return !result;
  return result;
}

// ================================================================================

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
    while(!(SPSR & (1 << SPIF))) ;
  }
  _OFF(C, 1); // my indicator led
  SPI_SET_SS;
}

void epdRefreshWait() {
  DDRB &= ~(1<<DDB1); // PD7 as input
  PORTB &= ~(1<<PB1);
  
  for(int i = 0 ; i < 10 ; i++) {
    uart_putc('_');
    _delay_ms(100);
  }
  uart_putc('t'); print_pind();
  
  while((PINB & 1<<PB1) == 0) { //(PINB & (1<<PB1) != 0) { // pulled low by eink module => busy
    uart_putc('!');
    _ON(C, 0);  _delay_ms(5); _OFF(C, 0); _delay_ms(50);
    _ON(C, 0);  _delay_ms(5); _OFF(C, 0); _delay_ms(50);
    _ON(C, 0);  _delay_ms(5); _OFF(C, 0); _delay_ms(50);
    system_sleep();
  }
  _delay_ms(100);
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
  uart_putc('(');
  uart_putc('R');
  uart_putc(' ');
  _delay_ms(100);
  sendCommand(0x12); // display refresh
  _delay_ms(100);
  epdRefreshWait();
  uart_putc(')');
}

void epdInit() {
  uart_putc('(');
  uart_putc('I');
  uart_putc(' ');
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
  uart_putc('p');
  uart_putc('o');
  epdRefreshWait();

  sendCommand(0X00); // panel setting
  sendData(0x0f); // someone online suggests 0x8F here

  sendCommand(0x61); // resolution TRES
  sendData(0x68); // 104 HRES
  sendData(0x00);
  sendData(0xd4); // 212 VRES

  sendCommand(0X50); // vcom and data interval
  sendData(0x77); // data sheet says default is: 0xd7, stm32 sample code says 0x77
  uart_putc(')');
}

// what does that even mean on an EInk??
void epdOff() {
  uart_putc('(');
  uart_putc('o');
  uart_putc('f');
  uart_putc('f');
  uart_putc(' ');
  
  sendCommand(0X50); // vcom and data interval
  sendData(0x07); // data sheet says default is: 0xd7, stm32 sample code says 0x77

  sendCommand(0x02); // power off
  //sendCommand(0x07); // deep sleep (requires hardware reset to power back up?)
  //sendData(0xA5);
  uart_putc(')');
}

extern const uint8_t Font_Table[] PROGMEM;
uint8_t charmap[13][35] = {};

void timer_get(uint8_t *hr, uint8_t *min, uint8_t *sec) {
  uint64_t t = seconds;
  if(sec != 0) *sec = t % 60;
  t /= 60;
  if(min != 0) *min = t % 60;
  t /= 60;
  if( hr != 0) *hr = t;
}

void epdRedrawConsole() {
  sendCommand(0x10); // start data transmission 1 (black/white)
  for(int x = 0 ; x < 212 ; x++) {
    for(int y = 0 ; y < 13 ; y++) {
      uint16_t letteroffset = 5 * charmap[12-y][x / 6];
      uint8_t col = (x % 6);
      uint8_t* offset = ((uint8_t*)Font_Table) + (letteroffset) + col;
      //    1px char spacing    ,-- 2 rightmost pixels can't hold glyph
      uint8_t pixel = (col==5||x>=35*6) ? 0x00 : pgm_read_byte(offset);
      sendData(~pixel);
    }
  }
  sendCommand(0x13); // start data transmission 2 (red)
  for(int i = 0 ; i < 212 * 13 ; i++) {
    sendData(0xFF);//pgm_read_byte(red + i));
  }
  epdRefresh(); // issue display update (this should take BUSY high for about 5s)
}

void epdRedraw() {
  sendCommand(0x10); // start data transmission 1 (black/white)
  for(int x = 0 ; x < 212 ; x++) {
    for(int y = 0 ; y < 13 ; y++) {
      uint8_t b = 0;
      for(uint8_t bb = 0 ; bb < 8 ; bb++) {
        b = (b<<1) | history_getpixel(x, 104-((y*8)+bb), 212,104);
      }
      sendData(b);
    }
  }
  sendCommand(0x13); // start data transmission 2 (red)
  for(int i = 0 ; i < 212 * 13 ; i++) {
    sendData(0xFF);//pgm_read_byte(red + i));
  }
  epdRefresh(); // issue display update (this should take BUSY high for about 5s)
}

void charmap_putc(uint8_t c) {
  static uint8_t line = 0;
  static uint8_t col = 0;
  if(line >= 13) line = 0;
  if(col >= 35) col = 0;

  charmap[line][col] = c;

  if(c == '\n') {
    col = 0;
    line++;
  } else {
    col++;
  }
}

void peripheral_init() {
  PORTC |= 1<<PC5;
  DDRC  |= 1<<DDC5;
  PORTC &= ~(1<<PC3);
  DDRC  |= 1<<DDC3;
  //_ON(C, 5); // tmp36's Vs
  //DDRC &= ~(1<<PC4); // tmp26' Vout
  //_OFF(C, 3); // tmp36's GND
}

void main() {

  /* history_insert(-28); */
  /* history_insert(-10); */
  /* history_insert(70); */
  /* history_insert(70); */
  /* history_insert(60); */
  /* history_insert(50); */
  /* history_insert(40); */
  /* history_insert(30); */
  /* history_insert(20); */
  /* history_insert(10); */

  DIH; DIH; DIH; // indicate something is happening
  uart_init(9600);
  
  /*   DDRB =      (1<<DDB2)|(1<<DDB3)|(1<<DDB5); */
  /* while(1) { */
  /*   SPI_SET_SS; uart_putc('k'); _delay_ms(1000); */
  /*   SPI_LOWER_SS; uart_putc('K'); _delay_ms(1000); */
  /* }; */

  peripheral_init();

  /* for(int x = 0 ; x < 80 ; x++) { */
  /*   for(int y = 0 ; y < 30 ; y++) { */
  /*     uint8_t p = history_pixel(x, y, 80, 30); */
  /*     uart_putc(p ? '.' : '#'); */
  /*   } */
  /*   uart_putc('\r'); */
  /*   uart_putc('\n'); */
  /* } */
  
  /* while(1) { */
  /*   //uart_putc((PINC>>PC4) + '0'); */
  /*   _ON(C, 0); _delay_ms(4); */
  /*   _OFF(C, 0); _delay_ms(400); */
  /* } */

  /* // fill screen with our alphabet */
  /* for(int i = 0 ; i < 256 ; i++) { */
  /*   *(charmap[5] + i) = (uint8_t)i; */
  /* } */
  
  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  setup_watchdog(); // triggers interrupt, easy (but inaccurate clock)
  sei();

  spi_init();

  uint64_t ww = 0;

  while(1) {
    ww++;
    
    peripheral_init();
    _delay_ms(50);
    
    uint8_t hr, min, sec;
    timer_get(&hr, &min, &sec);
    uint16_t volt = ivr_measure();
    int16_t dC = tmp_dC(); // deci centigrade
    history_insert(dC*2/10); // from tenth deg C => double deg C (storing C in int8_t)
    
    uart_putc('\r');
    uart_putc('\n');
    uart_putc('#');
    uart_putc(((ww/10000000)%10)+'0');
    uart_putc(((ww/ 1000000)%10)+'0');
    uart_putc(((ww/  100000)%10)+'0');
    uart_putc(((ww/   10000)%10)+'0');
    uart_putc(((ww/    1000)%10)+'0');
    uart_putc(((ww/     100)%10)+'0');
    uart_putc(((ww/      10)%10)+'0');
    uart_putc(((ww/       1)%10)+'0');
    uart_putc(' ');

    uart_putc((hr/10) + '0');
    uart_putc((hr%10) + '0');
    uart_putc('h');
    uart_putc((min/10) + '0');
    uart_putc((min%10) + '0');
    uart_putc('m');
    uart_putc((sec/10) + '0');
    uart_putc((sec%10) + '0');
    uart_putc('s');
    
    uart_putc(' ');
    uart_putc(dC < 0 ? '-' : '+');
    uart_putc(((dC /    1000)%10)+'0');
    uart_putc(((dC /     100)%10)+'0');
    uart_putc(((dC /      10)%10)+'0');
    uart_putc('.');
    uart_putc(((dC /       1)%10)+'0');
    uart_putc(0xC2); uart_putc(0xB0);  // °
    uart_putc('C');

    uart_putc(' ');
    uart_putc('v');
    uart_putc((volt/1000)%10 + '0');
    uart_putc((volt/ 100)%10 + '0');
    uart_putc((volt/  10)%10 + '0');
    uart_putc((volt/   1)%10 + '0');
    uart_wait(); // don't go to sleep or anything before we're done!      

    
    /* charmap_putc('#'); */
    /* charmap_putc(((ww/10000000)%10)+'0'); */
    /* charmap_putc(((ww/ 1000000)%10)+'0'); */
    /* charmap_putc(((ww/  100000)%10)+'0'); */
    /* charmap_putc(((ww/   10000)%10)+'0'); */
    /* charmap_putc(((ww/    1000)%10)+'0'); */
    /* charmap_putc(((ww/     100)%10)+'0'); */
    /* charmap_putc(((ww/      10)%10)+'0'); */
    /* charmap_putc(((ww/       1)%10)+'0'); */
    /* charmap_putc(' '); */
    /* charmap_putc((hr/10) + '0'); */
    /* charmap_putc((hr%10) + '0'); */
    /* charmap_putc('h'); */
    /* charmap_putc((min/10) + '0'); */
    /* charmap_putc((min%10) + '0'); */
    /* charmap_putc('m'); */
    /* charmap_putc((sec/10) + '0'); */
    /* charmap_putc((sec%10) + '0'); */
    /* charmap_putc('s'); */
    /* charmap_putc(' '); */
    /* charmap_putc('v'); */
    /* charmap_putc((volt/1000)%10 + '0'); */
    /* charmap_putc((volt/ 100)%10 + '0'); */
    /* charmap_putc((volt/  10)%10 + '0'); */
    /* charmap_putc((volt/   1)%10 + '0'); */
    /* charmap_putc(' '); */
    /* charmap_putc('\n'); */
    
    epdInit();
    epdRedraw();
    epdOff();

    uint64_t target = seconds + (60 * 10);
    //target = (target/60)*60;
    while(seconds < target) {
      _ON(C, 0); _delay_us(800); _OFF(C, 0);
      //print_pind();  uart_wait();  _delay_ms(2);
      system_sleep();
    }
  }
}
