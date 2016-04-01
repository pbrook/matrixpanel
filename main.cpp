#include "mbed.h"
#include "EthernetInterface.h"

#define COMPILE_ASSERT(x) extern int _compile_assert_fail[-((x) == 0)];

#define COLUMNS (32 * 8)
#define NUM_SUBFRAMES 8
#define ROW_SIZE (COLUMNS * NUM_SUBFRAMES)
#define ROWS 8
#define FRAME_SIZE (ROW_SIZE * ROWS)
#define NUM_FRAMES 2
#define SPEED 1
#define BASE 8

uint8_t framebuffer[FRAME_SIZE * NUM_FRAMES];
static uint8_t *write_framebuffer = &framebuffer[0];
static const uint8_t * volatile read_framebuffer = NULL;

#define FRAGMENT_SIZE 512
#define MAX_PACKET_SIZE (FRAGMENT_SIZE + 8)
uint8_t packet_buffer[MAX_PACKET_SIZE];
#define NUM_FRAGMENTS (FRAME_SIZE / FRAGMENT_SIZE)

COMPILE_ASSERT((FRAME_SIZE % FRAGMENT_SIZE) == 0)
#define FRAGMENTS_PER_ROW (NUM_FRAGMENTS / ROWS)
COMPILE_ASSERT((NUM_FRAGMENTS % ROWS) == 0)

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
DigitalOut oe(PTC1, 1);

Timer t;

static int extra_food = 1000;

static void
watchdog_feed()
{
  WDOG_REFRESH = 0xa602;
  WDOG_REFRESH = 0xb480;
}

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
  DMA_TCD0_CSR = DMA_CSR_INTMAJOR_MASK;
  DMA_TCD0_BITER_ELINKNO = 1;

  NVIC_EnableIRQ(DMA0_IRQn);
}

static void
trigger_dma(const void *src)
{
  DMA_TCD0_SADDR = (uint32_t)src;
  DMA_SSRT = 0;
}

EthernetInterface eth;

class NetRec {
  public:
    NetRec() {};
    void run();
  private:
    UDPSocket s;
    Endpoint client;
    uint8_t current_frame;
    uint32_t frame_mask;

    void send_frame_status();
    void flip();
};

void
NetRec::send_frame_status()
{
  packet_buffer[0] = 4; /* Frame ACK */
  packet_buffer[1] = current_frame;
  packet_buffer[2] = 0;
  packet_buffer[3] = 0;

  packet_buffer[4] = frame_mask & 0xff;
  packet_buffer[5] = (frame_mask >> 8) & 0xff;
  packet_buffer[6] = (frame_mask >> 16) & 0xff;
  packet_buffer[7] = (frame_mask >> 24) & 0xff;
  s.sendTo(client, (char *)packet_buffer, 8);
}


void
NetRec::flip()
{
  current_frame++;
  if (current_frame & 1) {
      read_framebuffer = &framebuffer[0];
      write_framebuffer = &framebuffer[FRAME_SIZE];
  } else {
      read_framebuffer = &framebuffer[FRAME_SIZE];
      write_framebuffer = &framebuffer[0];
  }
  frame_mask = 0;
  while (read_framebuffer)
      Thread::wait(1);
}

static void
ethernet_thread(const void *arg)
{
  NetRec netobj;

  netobj.run();
}

static void
decode_pixels(int fragment, uint8_t *pixels)
{
  uint32_t *src = (uint32_t *)pixels;
  uint32_t color1;
  uint32_t color2;
  uint8_t *base;
  uint8_t *dest;
  uint8_t bits;
  int partial;
  int row;
  int x;
  int n;
  int delta_base;
  bool flip;
  int color2_offset;

  partial = fragment % FRAGMENTS_PER_ROW;
  row = fragment / FRAGMENTS_PER_ROW;
  flip = (partial & 1) == 0;
  if (flip)
    row = ROWS - (row + 1);
  base = &write_framebuffer[partial * (FRAGMENT_SIZE / 8) + row * ROW_SIZE];
  if (flip) {
      base += (FRAGMENT_SIZE / 8) - 1;
      color2_offset = -(FRAGMENT_SIZE / 8);
      src += FRAGMENT_SIZE / 8;
      delta_base = -1;
  } else {
      color2_offset = FRAGMENT_SIZE / 8;
      delta_base = 1;
  }
  for (x = 0; x < FRAGMENT_SIZE / 8; x++) {
      color1 = src[0];
      color2 = src[color2_offset];
      src++;
      dest = base;
      for (n = 0; n < NUM_SUBFRAMES; n++) {
	  bits = (color1 & 1) | ((color1 & 0x100) >> 7) | ((color1 & 0x10000) >> 13);
	  bits |= ((color2 & 1) << 4) | ((color2 & 0x100) >> 2) | ((color2 & 0x10000) >> 9);
	  *dest = bits;
	  dest += COLUMNS;
	  color1 >>= 1;
	  color2 >>= 1;
      }
      base += delta_base;
  }
}

void
NetRec::run()
{
  int n;
  uint8_t cmd;
  uint8_t *dest;
  uint8_t fragment;

  eth.init();
  eth.connect();
  s.bind(3001);
  current_frame = 0;
  frame_mask = 0;

  while(1) {
      myled = 1;
      n = s.receiveFrom(client, (char *)packet_buffer, sizeof(packet_buffer));
      myled = 0;
      if (n < 4)
	  continue;
      cmd = packet_buffer[0];
      switch (cmd) {
      case 0: /* Frame data */
	  //if (frame_num != current_frame)
	  //    break;
	  if (n != FRAGMENT_SIZE + 4)
	      break;
	  fragment = packet_buffer[2];
	  frame_mask |= 1u << fragment;
	  send_frame_status();
	  if (fragment < NUM_FRAGMENTS) {
	      dest = &write_framebuffer[fragment * FRAGMENT_SIZE];
	      memcpy(dest, packet_buffer + 4, FRAGMENT_SIZE);
	  }
	  break;
      case 1: /* EOF */
	  if (n != 4)
	      break;
	  frame_mask = 0;
	  send_frame_status();
	  flip();
	  break;
      case 2: /* Query */
	  frame_mask = 0;
	  send_frame_status();
	  break;
      case 3: /* Pixel Data */
	  if (n != FRAGMENT_SIZE + 8)
	      break;
	  fragment = packet_buffer[2];
	  frame_mask |= 1u << fragment;
	  send_frame_status();
	  if (fragment < NUM_FRAGMENTS) {
	      decode_pixels(fragment, &packet_buffer[8]);
	  }
	  break;
      }
  }
}

static void scan_tick(void);
Timeout scan_timeout;

static bool output_active;
static bool dma_active;

#define BASE_SCAN_US 2

static void
maybe_rearm(void)
{
  static int current_row;
  static int current_subframe;
  static const uint8_t *ptr;
  static const uint8_t *base;

  if (output_active || dma_active)
    return;

  output_active = true;
  lat = 1;
  if (current_subframe == 0) {
      select_row(current_row);
  }
  scan_timeout.attach_us(scan_tick, BASE_SCAN_US << current_subframe);
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
	  if (extra_food > 0) {
	      watchdog_feed();
	      extra_food--;
	  }
	      
	  if (read_framebuffer) {
	      base = read_framebuffer;
	      read_framebuffer = NULL;
	      watchdog_feed();
	  }
	  ptr = base;
      }
  }
  if (ptr) {
      dma_active = true;
      trigger_dma(ptr);
      ptr += COLUMNS;
  }
}

extern "C" void
DMA0_IRQHandler(void)
{
  dma_active = false;
  DMA_CINT = 0;
  maybe_rearm();
}

static void
scan_tick(void)
{
  oe = 1;
  output_active = false;
  maybe_rearm();
}

#if 0
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
#endif

static void
watchdog_init()
{
  __disable_irq();
  watchdog_feed();
  WDOG_UNLOCK = 0xc520;
  WDOG_UNLOCK = 0xd928;
  
  WDOG_STCTRLH; // dummy read so at least one memory cycle passes
  WDOG_TOVALL = 10000;
  WDOG_TOVALH = 0;
  WDOG_STCTRLH = WDOG_STCTRLH_WAITEN_MASK | WDOG_STCTRLH_STOPEN_MASK | WDOG_STCTRLH_WDOGEN_MASK;
  watchdog_feed();
  __enable_irq();
}

int main()
{
  Thread thread(ethernet_thread);

  watchdog_init();
  green_led = 1;
  setup_flexbus();
  setup_dma();
  oe = 1;
  lat = 0;
  myled = 1;

  maybe_rearm();
  Thread::wait(osWaitForever);
}
