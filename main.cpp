#include "mbed.h"

#define COLUMNS 32
#define SPEED 1
#define BASE 10

DigitalOut myled(LED_RED);

DigitalOut r1(PTD6);
DigitalOut g1(PTD5);
DigitalOut b1(PTD3);
//DigitalOut gnd1(PTC2);
DigitalOut r2(PTD2);
DigitalOut g2(PTC9);
DigitalOut b2(PTC8);
//DigitalOut gnd2(PTB23);

DigitalOut sel_a(PTC0);
DigitalOut sel_b(PTC7);
DigitalOut sel_c(PTC5);
//DigitalOut gnd3(PTB9);

DigitalOut clk(PTD1);
DigitalOut lat(PTB19);
DigitalOut oe(PTC1);
//DigitalOut gnd4(PTC6);

static void
select_row(int n)
{
  sel_a = n & 1;
  sel_b = (n >> 1) & 1;
  sel_c = (n >> 2) & 1;
}

int main()
{
  int i;
  int row;
  int x;
  int match1;
  int match2;
  int delay;

  //gnd1 = 0;
  myled = 0;
  oe = 1;
  clk = 0;
  lat = 0;
  r1 = 0;
  r2 = 0;
  g1 = 0;
  g2 = 0;
  b1 = 0;
  b2 = 0;

  row = 0;
  select_row(row);
  while(1) {
      r1 = 0;
      match1 = (x / ((row + BASE) * SPEED)) % COLUMNS;
      match2 = (x / ((row + BASE + 8) * SPEED)) % COLUMNS;
      for (i = 0; i < COLUMNS; i++) {
	  r1 = (i == match1);
	  r2 = (i == match2);
	  g1 = (i == match1);
	  g2 = (i == match2);
	  b1 = (i == match1);
	  b2 = (i == match2);
	  clk = 1;
	  r1 = 0;
	  r2 = 0;
	  clk = 0;
      }
      lat = 1;
      select_row(row);
      lat = 0;
      oe = 0;
      row++;
      if (row == 8) {
	  row = 0;
	  delay++;
	  x++;
      }
      wait_ms(1);
      oe = 1;
  }
}
