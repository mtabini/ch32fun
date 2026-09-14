#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef unsigned uint;
typedef struct { uint8_t data[8192]; unsigned head, count, size, capacity; } queue_t;
static void queue_init(queue_t *q, unsigned size, unsigned capacity) { *q=(queue_t){.size=size,.capacity=capacity}; }
static bool queue_is_full(queue_t *q) { return q->count == q->capacity; }
static bool queue_try_add(queue_t *q, const void *v) { if(queue_is_full(q))return false; memcpy(q->data+((q->head+q->count)%q->capacity)*q->size,v,q->size); ++q->count; return true; }
static bool queue_try_remove(queue_t *q, void *v) { if(!q->count)return false; memcpy(v,q->data+q->head*q->size,q->size); q->head=(q->head+1)%q->capacity; --q->count; return true; }
typedef struct { uint32_t bit_rate; uint8_t stop_bits, parity, data_bits; } cdc_line_coding_t;
static queue_t usb_in[2], usb_out[2], wire_in, wire_out;
static unsigned usb_space[2]={512,512};
static bool connected[2]={true,true};
static unsigned baud, data_bits, stop_bits, parity, boot_calls, write_calls, init_calls;
static uint8_t last_reg;
static uint32_t last_value;
static unsigned tud_cdc_n_available(unsigned i) { return usb_in[i].count; }
static unsigned tud_cdc_n_read(unsigned i, void *p, unsigned n) { assert(n==1); return queue_try_remove(&usb_in[i],p); }
static unsigned tud_cdc_n_write_available(unsigned i) { return usb_space[i]; }
static unsigned tud_cdc_n_write(unsigned i,const void *p,unsigned n) { assert(usb_space[i]>=n); for(unsigned j=0;j<n;j++)assert(queue_try_add(&usb_out[i],(uint8_t*)p+j)); usb_space[i]-=n; return n; }
static void tud_cdc_n_write_flush(unsigned i) { (void)i; }
static bool tud_cdc_n_connected(unsigned i) { return connected[i]; }
static void tud_task(void) {}
static void tusb_init(void) {}
static void reset_usb_boot(unsigned a,unsigned b) { (void)a;(void)b; ++boot_calls; }
#define WCH_UART_TX_PIN 0
#define WCH_UART_RX_PIN 1
#define WCH_RVSWD_DIO_PIN 2
#define WCH_RVSWD_DCK_PIN 3
#define GPIO_FUNC_UART 2
#define UART_PARITY_NONE 0
#define UART_PARITY_ODD 1
#define UART_PARITY_EVEN 2
#define UART_UARTFR_BUSY_BITS 8
#define uart0 0
static struct { uint32_t fr; } uart_hw;
#define uart_get_hw(x) (&uart_hw)
static void uart_init(int u,unsigned b) { (void)u;baud=b; }
static void gpio_set_function(unsigned p,unsigned f) { (void)p;(void)f; }
static void gpio_pull_up(unsigned p) { (void)p; }
static void uart_set_hw_flow(int u,bool a,bool b) { (void)u;(void)a;(void)b; }
static void uart_set_format(int u,unsigned d,unsigned s,unsigned p) { (void)u;data_bits=d;stop_bits=s;parity=p; }
static void uart_set_fifo_enabled(int u,bool b) { (void)u;(void)b; }
static void uart_set_baudrate(int u,unsigned b) { (void)u;baud=b; }
static bool uart_is_readable(int u) { (void)u;return wire_in.count>0; }
static bool uart_is_writable(int u) { (void)u;return !queue_is_full(&wire_out); }
static int uart_getc(int u) { (void)u; uint8_t b;assert(queue_try_remove(&wire_in,&b));return b; }
static void uart_putc_raw(int u,uint8_t b) { (void)u;assert(queue_try_add(&wire_out,&b)); }
static void tight_loop_contents(void) {}
static void multicore_launch_core1(void (*f)(void)) { (void)f; }
