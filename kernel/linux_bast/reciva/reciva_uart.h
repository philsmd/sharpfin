#ifndef LINUX_UART_H
#define LINUX_UART_H

/* Reciva Uart */

#include <linux/ioctl.h>


/* Receive data from UART */
typedef struct 
{
  void (*uart_rx)(char *);

} reciva_uart_rx_driver_t;
extern int reciva_uart_register_rx_driver(const reciva_uart_rx_driver_t *driver);

/* Write data to UART */
extern int reciva_uart_write_internal(const char *buf, size_t count);

/* Indicates if the tx buffer is empty */
extern int reciva_uart_tx_buffer_empty(void);


#define UART_IOCTL_BASE  'U'
#define IOC_UART_SET_BAUD               _IOW(UART_IOCTL_BASE, 0, int)
#define IOC_UART_GET_TX_BUFFER_EMPTY    _IOR(UART_IOCTL_BASE, 1, int)
#define IOC_UART_CLEAR_RX_BUFFER        _IOW(UART_IOCTL_BASE, 2, int)

#endif
