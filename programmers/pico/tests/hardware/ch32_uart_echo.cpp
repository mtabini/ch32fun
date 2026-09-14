// Temporary qualification image only. Never installed in Hardware Core sources.
#include "ch32x035.h"
#include "api.hpp"
#include "wait.hpp"
volatile unsigned uartReceived = 0;
volatile unsigned uartTransmitted = 0;
int main() {
    using namespace t76::ch32x035;
    if (bootApi().magic != 0x31423754 || bootApi().version != 1) for (;;) {}
    waitInit();
    bootApi().disconnect(); // Prove the data path works with CH32 USB disabled.
    RCC->APB2PCENR |= RCC_IOPBEN | RCC_AFIOEN;
    RCC->APB1PCENR |= RCC_USART4EN;
    RCC->APB1PRSTR |= RCC_USART4RST;
    RCC->APB1PRSTR &= ~RCC_USART4RST;
    AFIO->PCFR1 &= ~AFIO_PCFR1_USART4_REMAP; // RM p72: TX/PB0, RX/PB1.
    GPIOB->CFGLR = (GPIOB->CFGLR & ~0xffu) | 0x8bu; // PB0 AF push-pull, PB1 pull-up input.
    GPIOB->BSHR = 1u << 1;
    USART4->CTLR1 = 0;
    USART4->CTLR2 = 0;
    USART4->CTLR3 = 0;
    USART4->BRR = (48000000u + 115200u/2) / 115200u;
    USART4->CTLR1 = USART_CTLR1_UE | USART_CTLR1_TE | USART_CTLR1_RE;
    for (;;) {
        bootApi().housekeeping();
        if ((USART4->STATR & (USART_STATR_RXNE | USART_STATR_TXE)) ==
            (USART_STATR_RXNE | USART_STATR_TXE)) {
            auto byte = USART4->DATAR;
            ++uartReceived;
            USART4->DATAR = byte;
            ++uartTransmitted;
        }
    }
}
