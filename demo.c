#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#define PROGMEM
#include "font8.c"

/* uint8_t charmap[13][35] = {}; */
/* int console_getpixel(int x, int y) { */
/*   if(x >= 35*6) return 0; */
/*   uint16_t letteroffset = 5 * charmap[y/8][x / 6]; */
/*   uint8_t col = (x % 6); */
/*   if(col == 5) return 0; */
/*   uint8_t* offset = ((uint8_t*)Font_Table) + (letteroffset) + col; */
/*   //    1px char spacing    ,-- 2 rightmost pixels can't hold glyph */
/*   return ((*offset) >> (y%8)) & 0x01; */
/*   //return pixel>>(y%8); */
/* } */

int glyphv_getpixel(uint8_t x, uint8_t y, uint8_t ch) {
  //if(x >= 35*6) return 0;
  if(x >= 5) return 0;
  if(y >= 8) return 0;
  uint16_t letteroffset = 5 * ch;
  uint8_t col = y;
  uint8_t row = x;
  uint8_t* offset = ((uint8_t*)Font_Table) + (letteroffset) + row;
  return ((*offset) >> (col)) & 0x01;
}

int glyphh_getpixel(uint8_t x, uint8_t y, uint8_t ch) {
  //if(x >= 35*6) return 0;
  if(x >= 5) return 0;
  if(y >= 8) return 0;
  uint16_t letteroffset = 5 * ch;//charmap[y/8][x / 6];
  uint8_t col = (x % 6);
  //if(col == 5) return 0;
  uint8_t* offset = ((uint8_t*)Font_Table) + (letteroffset) + col;
  //    1px char spacing    ,-- 2 rightmost pixels can't hold glyph
  return ((*offset) >> (y%8)) & 0x01;
  //return pixel>>(y%8);
}


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

  int16_t xx = x;//mapping(x, 0, w,  0, HISTORY_SIZE); // history position of current x value
  //printf("mapping %d\n", xx);
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

// find out which digit is shown for x,y and get the corresponding
// glyph's pixel
uint8_t numberpixel(uint8_t x, uint8_t y, int16_t val) {
  uint8_t isneg = val < 0;
  if(isneg) {
    //x -= 4; // give room for minus-sign
    val = -val;
  }
  uint8_t ch;
  uint8_t digit = x/6;
  if(digit == 0) {
    ch = isneg ? '-' : '+';
  }
  else {
    digit--;
    if(digit > 4) return 0;
    for(int i = 0 ; i < 4-digit ; i++) {
      val /= 10;
    }
    val %= 10;
    ch = val+'0';
  }
  return glyphv_getpixel(x%6, y, ch);  
}

uint8_t getpixel(uint8_t x, uint8_t y, int16_t dC, int16_t v) {
  int color = 0;
  int histwidth = HISTORY_SIZE; // 1:1 pixel for our 114 datapoints for beauty
  if(x < 12) { // number scale
    uint8_t digit = (x)/6;
    uint8_t hh = 10;
    uint8_t hi = 1;
    if(y < 18) {
      color = digit == 0
        ? glyphv_getpixel(x-1-00, y-1*hh-hi, '3')
        : glyphv_getpixel(x-1-06, y-1*hh-hi, '0');
    }
    else if(y < 38) {
      color = digit == 0
        ? glyphv_getpixel(x-1-00, y-3*hh-hi, '2')
        : glyphv_getpixel(x-1-06, y-3*hh-hi, '0');
    }
    else if(y < 58) {
      color = digit == 0
        ? glyphv_getpixel(x-1-00, y-5*hh-hi, '1')
        : glyphv_getpixel(x-1-06, y-5*hh-hi, '0');
    }
    else if(y < 78) {
      color = digit == 0
        ? glyphv_getpixel(x-1-00, y-7*hh-hi, ' ')
        : glyphv_getpixel(x-1-06, y-7*hh-hi, '0');
    }
    else if(y < 98) {
      if(x < 2) { // poor man's minus sign
        color = (y == 94) ? 1 : 0;
      }
      else {
        color = digit == 0
          ? glyphv_getpixel(x-1-00, y-9*hh-hi, '1')
          : glyphv_getpixel(x-1-06, y-9*hh-hi, '0');
      }
    }
  }
  else if(x == 12) color = 0; // margin
  else if(x < histwidth+13) color = !history_getpixel(x-13, y, histwidth, 104); // chart
  else if(x == histwidth+13) color = 0; // another margin
  else { // digits for current temperature and voltage
    uint8_t xoff = histwidth+14;
    y -= 11;
    if(x-xoff < 6*6) {
      //xoff+=9;
      if(y < 20) {
        color = numberpixel(x-xoff, y, dC/10);
      } else {
        color = numberpixel(x-xoff, y-20, v);
      }
    }
    else {
      xoff+=6*6 + 2;
      if(y < 20) {
        if(x-xoff<6) color = glyphv_getpixel(x-xoff, y-00, 'd');
        else         color = glyphv_getpixel(x-xoff-6, y-00, 'C');
      }
      else {
        if(x-xoff<6) color = glyphv_getpixel(x-xoff, y-20, 'm');
        else         color = glyphv_getpixel(x-xoff-6, y-20, 'V');
      }
    }
  }
  return !color;
}

static int kk =  0;
int render() {
  for(int i = 0 ; i < 10 ; i++) {
    kk++;
    history_insert(50.0*sin(kk/100.0)+20.0);
  }

  FILE *fd = fopen("image.ppm", "wb");
  fprintf(fd, "P1\n212 104\n");
  for(int y = 0 ; y < 104 ; y++) {
    for(int x = 0 ; x < 212 ; x++) {
      fprintf(fd, "%d ", !(getpixel(x, y, 234, 2800) & 0x01));
    }
    fprintf(fd, "\n");
  }
  fclose(fd);
  printf("image.ppm written\n");
}

int main() {
  for(int i = 0 ; i < 10 ; i++) { history_insert(-20); }
  for(int i = 0 ; i < 10 ; i++) { history_insert(  0); }
  for(int i = 0 ; i < 10 ; i++) { history_insert( 20); }
  for(int i = 0 ; i < 10 ; i++) { history_insert( 40); }
  for(int i = 0 ; i < 10 ; i++) { history_insert( 70); }
  
  for(int i = 0 ; i < 1 ; i++) {
    render();
    usleep(100000);
  }
}
