cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico CACHE STRING "Pico SDK board")
set(WCH_RVSWD_DIO_PIN 2 CACHE STRING "Pico GPIO connected to target DIO")
set(WCH_RVSWD_DCK_PIN 3 CACHE STRING "Pico GPIO connected to target DCK")
set(WCH_UART_TX_PIN 0 CACHE STRING "UART0 TX GPIO (0, 12, or 16)")
set(WCH_UART_RX_PIN 1 CACHE STRING "UART0 RX GPIO (1, 13, or 17)")
if(NOT WCH_UART_TX_PIN MATCHES "^(0|12|16)$" OR NOT WCH_UART_RX_PIN MATCHES "^(1|13|17)$")
  message(FATAL_ERROR "Select exposed Pico UART0 TX/RX pins")
endif()
if(WCH_UART_TX_PIN STREQUAL WCH_RVSWD_DIO_PIN OR WCH_UART_TX_PIN STREQUAL WCH_RVSWD_DCK_PIN OR
   WCH_UART_RX_PIN STREQUAL WCH_RVSWD_DIO_PIN OR WCH_UART_RX_PIN STREQUAL WCH_RVSWD_DCK_PIN)
  message(FATAL_ERROR "UART and RVSWD pins must not overlap")
endif()
if(NOT DEFINED PICO_SDK_PATH)
  if(DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
  else()
    message(FATAL_ERROR "Set PICO_SDK_PATH to a Pico SDK checkout")
  endif()
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(ch32fun_pico_programmer C CXX ASM)
pico_sdk_init()

if(NOT TARGET tinyusb_device)
  message(FATAL_ERROR
    "Pico SDK TinyUSB dependency is missing. Run: "
    "git -C ${PICO_SDK_PATH} submodule update --init lib/tinyusb")
endif()

add_executable(ch32fun_pico_programmer
  src/usb_descriptors.c
  src/main.c
  src/wch_rvswd.c
)

target_include_directories(ch32fun_pico_programmer PRIVATE src)
target_compile_definitions(ch32fun_pico_programmer PRIVATE
  WCH_UART_TX_PIN=${WCH_UART_TX_PIN}
  WCH_UART_RX_PIN=${WCH_UART_RX_PIN}
  WCH_RVSWD_DIO_PIN=${WCH_RVSWD_DIO_PIN}
  WCH_RVSWD_DCK_PIN=${WCH_RVSWD_DCK_PIN}
)
target_link_libraries(ch32fun_pico_programmer pico_stdlib pico_multicore pico_unique_id tinyusb_device tinyusb_board hardware_uart hardware_gpio hardware_pio hardware_sync)

pico_generate_pio_header(ch32fun_pico_programmer
  ${CMAKE_CURRENT_LIST_DIR}/src/wch_rvswd.pio
)

pico_enable_stdio_usb(ch32fun_pico_programmer 0)
pico_enable_stdio_uart(ch32fun_pico_programmer 0)
pico_add_extra_outputs(ch32fun_pico_programmer)
