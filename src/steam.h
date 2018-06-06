#ifndef STEAM_H_INCLUDED
#define STEAM_H_INCLUDED

#include <stdint.h>

int steam_recv_report(int fd, uint8_t reply[64]);
int steam_send_report(int fd, const uint8_t *cmd, int size);
int steam_send_report_byte(int fd, uint8_t cmd);
int steam_write_register(int fd, uint8_t reg, uint16_t value);
int steam_read_register(int fd, uint8_t reg, uint16_t *value);

/* returns -1 on error, 0 if unconnected wireless, 1 if connected */
int steam_get_serial(int fd, char serial[11]);

#endif /* STEAM_H_INCLUDED */
