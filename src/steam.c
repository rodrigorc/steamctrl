/*
 * Copyright (c) 2018 Rodrigo Rivas Costa <rodrigorivascosta@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/hidraw.h>
#include "steam.h"

int steam_recv_report(int fd, uint8_t reply[64])
{
    uint8_t feat[65] = {0};
    int res;
    res = ioctl(fd, HIDIOCGFEATURE(sizeof(feat)), feat);
    if (res < 0)
        return res;
    if (reply)
        memcpy(reply, feat + 1, 64);
    return res;
}

int steam_send_report(int fd, const uint8_t *cmd, int size)
{
    int res;
    uint8_t feat[65] = {0};
    memcpy(feat + 1, cmd, size);

    //This command sometimes fails with EPIPE, particularly with the wireless device
    //so retry a few times before giving up.
    for (int i = 0; i < 10; ++i) // up to 500 ms
    {
        res = ioctl(fd, HIDIOCSFEATURE(sizeof(feat)), feat);
        if (res >= 0 || errno != EPIPE)
            return res;

        steam_recv_report(fd, NULL);
        usleep(50000);
    }
    return res;
}

int steam_send_report_byte(int fd, uint8_t cmd)
{
    return steam_send_report(fd, &cmd, 1);
}

int steam_write_register(int fd, uint8_t reg, uint16_t value)
{
    uint8_t cmd[] = {0x87, 0x03, reg, value & 0xFF, value >> 8};
    return steam_send_report(fd, cmd, sizeof(cmd));
}

int steam_read_register(int fd, uint8_t reg, uint16_t *value)
{
    uint8_t cmd[] = {0x89, 0x03, reg};
    uint8_t reply[64];
    int res = steam_send_report(fd, cmd, sizeof(cmd));
    if (res < 0)
        return res;
    res = steam_recv_report(fd, reply);
    if (res < 0)
        return res;
    *value = reply[3] | (reply[4] << 8);
    return res;
}

int steam_get_serial(int fd, char serial[11])
{
    int res;
    uint8_t reply[64] = {};
    uint8_t cmd[] = {0xAE, 0x15, 0x01};
    res = steam_send_report(fd, cmd, sizeof(cmd));
    if (res < 0)
        return res;
    res = steam_recv_report(fd, reply);
    if (res < 0)
        return res;
    reply[13] = 0;
    strcpy(serial, (char*)reply + 3);
    return serial[0] ? 1 : 0;
}

