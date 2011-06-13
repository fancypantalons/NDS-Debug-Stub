/*
 * $Id: debug_serial.c,v 1.2 2006/10/10 12:11:05 ben Exp $
 */
/** \file
 * \brief The SPI UART bridge debug stub comms.
 */
#include <stdlib.h>
#include <stdint.h>

#include "debug_stub.h"

#include "spi_uart_bridge.h"

#ifdef DO_LOGGING
#define LOG( fmt, ...) logFn_debug( fmt, ##__VA_ARGS__)
#else
#define LOG( fmt, ...)
#endif


/** The RS232 Transmit fifo size */
#define RS232_TX_BUFFER_SIZE 1024

/** The RS232 Receive fifo size */
#define RS232_RX_BUFFER_SIZE 1024

/** The rx buffer */
uint8_t rx_buffer[RS232_RX_BUFFER_SIZE];
/** The tx buffer */
uint8_t tx_buffer[RS232_TX_BUFFER_SIZE];


static int
init_fn( void *data __attribute__((unused))) {
  LOG("Initialising Serial comms\n");

  /* Initialise the spi uart bridge with its buffers */
  init_spiUART( tx_buffer, sizeof( tx_buffer),
		rx_buffer, sizeof( rx_buffer));

  return 1;
}


static void
writeByte_fn( uint8_t byte) {
  write_spiUART( &byte, 1);
}


static void
writeData_fn( uint8_t *buffer, uint32_t count) {
  write_spiUART( buffer, count);
}


static int
readByte_fn( uint8_t *read_byte) {
  int read_good;

  read_good = read_spiUART( read_byte, 1);

  if ( read_good) {
    LOG("SERIAL DEBUG read byte\n");
  }
  else {
  }

  return read_good;
}


static void
poll_fn( void) {
  poll_spiUART();
}

static uint32_t
irqs_fn( void) {
  /* no interrupts are needed */
  return 0;
}


/** The instance of the comms function interface */
struct comms_fn_iface_debug serialCommsIf_debug = {
  .init_fn = init_fn,

  .readByte_fn = readByte_fn,

  .writeByte_fn = writeByte_fn,

  .writeData_fn = writeData_fn,

  .poll_fn = poll_fn,

  .get_IRQs = irqs_fn
};

