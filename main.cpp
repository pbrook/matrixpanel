#include "mbed.h"
#include "EthernetInterface.h"

#define COLUMNS 32
#define SPEED 1
#define BASE 8

Serial pc(USBTX, USBRX);

DigitalOut myled(LED_RED);
DigitalOut green_led(LED_GREEN);

#define PANEL_MEM_BASE 0xa0000000
#define PANEL_MEM (*(volatile uint8_t *)PANEL_MEM_BASE)

#if 0
DigitalOut r1(PTD6);
DigitalOut g1(PTD5);
DigitalOut b1(PTD3);
DigitalOut r2(PTD2);
DigitalOut g2(PTC9);
DigitalOut b2(PTC8);
DigitalOut clk(PTD1);
#endif

DigitalOut sel_a(PTC0);
DigitalOut sel_b(PTC7);
DigitalOut sel_c(PTC5);

DigitalOut lat(PTB19);
DigitalOut oe(PTC1);

Timer t;

static void
select_row(int n)
{
  sel_a = n & 1;
  sel_b = (n >> 1) & 1;
  sel_c = (n >> 2) & 1;
}

static void
setup_flexbus(void)
{
  SIM_CLKDIV1 = (SIM_CLKDIV1 &~ SIM_CLKDIV1_OUTDIV3_MASK) | SIM_CLKDIV1_OUTDIV3(2);
  SIM_SCGC5 |= SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTD_MASK;
  SIM_SCGC7 |= SIM_SCGC7_FLEXBUS_MASK;
  FB_CSMR0 = 0;
  FB_CSAR0 = PANEL_MEM_BASE;
  FB_CSCR0 = FB_CSCR_BLS_MASK | FB_CSCR_AA_MASK | FB_CSCR_PS(1);
  FB_CSMR0 = FB_CSMR_V_MASK;
  PORTD_PCR6 = PORT_PCR_MUX(5) | PORT_PCR_DSE_MASK;
  PORTD_PCR5 = PORT_PCR_MUX(5) | PORT_PCR_DSE_MASK;
  PORTD_PCR3 = PORT_PCR_MUX(5) | PORT_PCR_DSE_MASK;
  PORTD_PCR2 = PORT_PCR_MUX(5) | PORT_PCR_DSE_MASK;
  PORTC_PCR9 = PORT_PCR_MUX(5) | PORT_PCR_DSE_MASK;
  PORTC_PCR8 = PORT_PCR_MUX(5) | PORT_PCR_DSE_MASK;
  PORTD_PCR1 = PORT_PCR_MUX(5) | PORT_PCR_DSE_MASK;
  myled = 1;
}

static void
setup_dma(void)
{
  SIM_SCGC6 |= SIM_SCGC6_DMAMUX_MASK;
  SIM_SCGC7 |= SIM_SCGC7_DMA_MASK;
  DMA_TCD0_SADDR = 0;
  DMA_TCD0_SOFF = 4;
  DMA_TCD0_ATTR = DMA_ATTR_SSIZE(2) | DMA_ATTR_DSIZE(0);
  DMA_TCD0_NBYTES_MLNO = COLUMNS;
  DMA_TCD0_SLAST = 0;
  DMA_TCD0_DADDR = PANEL_MEM_BASE;
  DMA_TCD0_DOFF = 0;
  DMA_TCD0_CITER_ELINKNO = 1;
  DMA_TCD0_DLASTSGA = 0;
  DMA_TCD0_CSR = 0; //DMA_CSR_INTMAJOR_MASK;
  DMA_TCD0_BITER_ELINKNO = 1;
}

uint8_t framebuffer[COLUMNS * 8];

static void
trigger_dma(uint32_t src)
{
  DMA_TCD0_SADDR = src;
  DMA_SSRT = 0;
}

EthernetInterface eth;

#define PACKET_SIZE (COLUMNS * 8)
uint8_t packet_buffer[PACKET_SIZE];

static void
ethernet_thread(const void *arg)
{
  UDPSocket s;
  Endpoint client;
  int n;

  eth.init();
  eth.connect();
  s.bind(3001);

  while(1) {
      n = s.receiveFrom(client, (char *)packet_buffer, sizeof(packet_buffer));
      if (n == PACKET_SIZE) {
	  memcpy(framebuffer, packet_buffer, n);
      }
  }
}

#if 0
static void scan_tick(void);
Timeout scan_timeout;

static bool output_active;
static bool dma_active;
static const uint8_t *read_framebuffer;

#define BASE_SCAN_US 4

static void
maybe_rearm(void)
{
  static int current_row;
  static int current_subframe;
  static const uint8_t *ptr;

  if (output_active || dma_active)
    return;

  output_active = true;
  lat = 1;
  if (current_subframe == 0) {
      select_row(current_row);
  }
  scan_timeout.attach(scan_tick, BASE_SCAN_US << current_subframe);
  lat = 0;
  if (ptr) {
      oe = 0;
  }
  current_subframe++;
  if (current_subframe == NUM_SUBFRAMES) {
      current_subframe = 0;
      current_row++;
      if (current_row == 8) {
	  current_row = 0;
	  ptr = read_framebuffer;
      }
  }
  if (ptr) {
      dma_active = true;
      trigger_dma(ptr);
      ptr += COLUMNS;
  }
}

static void
scan_tick(void)
{
  oe = 1;
  output_active = false;
  maybe_rearm();
}
#endif

int main()
{
  Thread thread(ethernet_thread);
  int i;
  int row;
  int x;
  int match1;
  int match2;
  uint8_t *p;
  int next_tick;
  int now;

  green_led = 1;
  setup_flexbus();
  setup_dma();
  oe = 1;
  lat = 0;
  x = 0;
  myled = 1;

  row = 0;
  t.start();
  next_tick = 0;
  while(1) {
      p = &framebuffer[COLUMNS * row];
      match1 = (x / ((row + BASE) * SPEED)) % COLUMNS;
      match2 = (x / ((row + BASE + 8) * SPEED)) % COLUMNS;
      for (i = 0; i < COLUMNS; i++) {
	  uint8_t val;
	  val = 0;
	  if (i == match1)
	    val |= 0x0a;
	  if (i == match2)
	    val |= 0xc0;
	  p[i] = val;
      }
      myled = 1;
      trigger_dma((uint32_t)p);
      now = t.read_us();
      if (now < next_tick)
	wait_us(next_tick - now);
      next_tick += 1000;
      myled = 0;
      oe = 1;
      lat = 1;
      select_row(row);
      lat = 0;
      oe = 0;
      row++;
      if (row == 8) {
	  row = 0;
	  x++;
      }
  }
}
