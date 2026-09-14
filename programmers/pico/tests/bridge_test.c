#include "fakes/fake_api.c"
#include <stdio.h>
#define main firmware_main
#include "../src/main.c"
#undef main
void wch_rvswd_init(uint d,uint c) { assert(d==2 && c==3); ++init_calls; }
void wch_rvswd_write_reg(uint8_t r,uint32_t v) { last_reg=r;last_value=v;++write_calls; }
bool wch_rvswd_read_reg(uint8_t r,uint32_t *v) { *v=0x12345678;return r==7; }
static void put(queue_t *q,uint8_t b) { assert(queue_try_add(q,&b)); }
static uint8_t take(queue_t *q) { uint8_t b;assert(queue_try_remove(q,&b));return b; }
int main(void) {
    queue_init(&uart_rx,1,4096);queue_init(&uart_tx,1,512);queue_init(&uart_settings,sizeof(cdc_line_coding_t),1);
    for(unsigned i=0;i<2;i++){queue_init(&usb_in[i],1,4096);queue_init(&usb_out[i],1,4096);}
    queue_init(&wire_in,1,4096);queue_init(&wire_out,1,4096);
    // A fragmented programmer write must not stop the independent UART bridge.
    put(&usb_in[0],'w');programmer_task();assert(request_size==1 && !write_calls);
    put(&usb_in[1],'?');put(&wire_in,'A');bridge_task();uart_task();bridge_task();
    assert(take(&wire_out)=='?' && take(&usb_out[1])=='A' && !usb_out[0].count);
    uint8_t tail[]={5,0x78,0x56,0x34,0x12};
    for(unsigned i=0;i<5;i++){put(&usb_in[0],tail[i]);programmer_task();}
    assert(write_calls==1 && last_reg==5 && last_value==0x12345678 && take(&usb_out[0])=='+');
    put(&usb_in[0],'r');put(&usb_in[0],7);programmer_task();
    for(unsigned i=0;i<4;i++)assert(take(&usb_out[0])==(uint8_t)(0x12345678u>>(8*i)));
    put(&usb_in[0],'r');put(&usb_in[0],8);programmer_task();
    for(unsigned i=0;i<4;i++)assert(take(&usb_out[0])==255);
    // No command consumption without enough reply capacity.
    usb_space[0]=0;put(&usb_in[0],'?');programmer_task();assert(usb_in[0].count==1);
    usb_space[0]=512;programmer_task();assert(take(&usb_out[0])=='+');
    // Full UART TX queue backpressures only CDC1, without losing its byte.
    for(unsigned i=0;i<512;i++)put(&uart_tx,42);
    put(&usb_in[1],13);bridge_task();assert(usb_in[1].count==1);
    uart_task();bridge_task();assert(!usb_in[1].count);
    // Full USB TX preserves received UART bytes until capacity returns.
    put(&uart_rx,99);usb_space[1]=0;bridge_task();assert(uart_rx.count==1);
    usb_space[1]=512;bridge_task();assert(take(&usb_out[1])==99);
    connected[1]=false;put(&uart_rx,88);bridge_task();assert(!uart_rx.count && !usb_out[1].count);
    // UART coding affects only UART; settings wait until transmitter is idle.
    cdc_line_coding_t c={9600,2,2,7};tud_cdc_line_coding_cb(1,&c);
    uart_hw.fr=8;uart_task();assert(baud==0);uart_hw.fr=0;uart_task();
    assert(baud==9600 && stop_bits==2 && parity==2 && data_bits==7);
    c.bit_rate=0;tud_cdc_line_coding_cb(1,&c);uart_task();assert(baud==9600);
    c.bit_rate=1200;tud_cdc_line_coding_cb(1,&c);tud_cdc_line_state_cb(1,false,false);assert(!boot_calls);
    tud_cdc_line_coding_cb(0,&c);tud_cdc_line_state_cb(0,false,false);assert(boot_calls==1);
    put(&usb_in[0],'r');programmer_task();assert(request_size==1);tud_umount_cb();assert(!request_size);
    puts("PASS: programmer framing, port isolation, backpressure, coding and reset routing");
}
