/*
 * steamctrl: a utility to setup Valve Steam Controller
 *
 * It is intended as a user-space complement to the linux hid-steam driver.
 *
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

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include "udev_helper.h"
#include "steam.h"

int verbose = 0;
struct steam_find_info
{
    int fd;
    int wireless;
    char serial[11];
    char *dev_name;
};

static int steam_hidraw_cb(void *ptr, struct udev *ud, const char *sys_hidraw)
{
    int res, fd = -1;
    char serial[11] = {};
    struct udev_device *hid = udev_device_new_from_syspath(ud, sys_hidraw);
    const char *dev = udev_device_get_devnode(hid);

    if (verbose)
        fprintf(stderr, "Probing %s...\n", dev);
    struct steam_find_info *info = ptr;

    fd = open(dev, O_RDWR);
    if (fd >= 0)
    {
        /* If it is a wireless adaptor, we send a get_serial to see if
         * there is a connected controller.
         * If we filter by serial, we also need it. */
        if (info->serial[0] || info->wireless)
        {
            res = steam_get_serial(fd, serial);
            if (verbose)
            {
                if (res > 0)
                    fprintf(stderr, "Found device '%s' at %s\n", serial, dev);
                else if (res == 0)
                    fprintf(stderr, "Wireless device %s not connected\n", dev);
                else
                    perror(dev);
            }
            res = res > 0;
        }
        else
        {
            res = 1;
        }
    }
    else
    {
        res = 0;
        if (verbose)
            perror(dev);
    }

    if (res && info->serial[0])
    {
        /* Filter by serial number */
        res = strcmp(serial, info->serial) == 0;
    }

    if (res)
    {
        info->dev_name = strdup(dev);
        info->fd = fd;
    }
    else
    {
        if (fd >= 0)
            close(fd);
    }
    udev_device_unref(hid);
    return res;
}

static int steam_usb_itf00_cb(void *ptr, struct udev *ud, const char *sys_itf)
{
    int res;
    struct udev_device *itf = udev_device_new_from_syspath(ud, sys_itf);
    res = find_udev_devices(steam_hidraw_cb, ptr, ud, itf, "hidraw", NULL);
    udev_device_unref(itf);
    return res;
}

static int steam_usb_cb(void *ptr, struct udev *ud, const char *sys_usb)
{
    int res;
    struct udev_device *usb = udev_device_new_from_syspath(ud, sys_usb);
    res = find_udev_devices(steam_usb_itf00_cb, ptr, ud, usb, "usb", "bInterfaceProtocol", "00", NULL);
    udev_device_unref(usb);
    return res;
}

static int parse_hex(const char *txt, uint8_t *buf, int len)
{
    int i;
    char *end;
    for (i = 0; txt[0] && txt[1] && i < len; txt += 2, ++i)
    {
        char n[3] = {txt[0], txt[1], 0};
        buf[i]  = strtol(n, &end, 16);
        if (*end)
            return -1;
    }
    return i;
}

/* commands */

struct command
{
    const char *command;
    int nargs;
    const char *help;
    void (*func)(int fd, char **);
};

static void cmd_get_serial(int fd, char **args)
{
    char serial[11];
    int res = steam_get_serial(fd, serial);
    if (res > 0)
        printf("%s\n", serial);
}
static void cmd_disable_keys_cursor(int fd, char **args)
{
    steam_send_report_byte(fd, 0x81);
}
static void cmd_enable_keys_cursor(int fd, char **args)
{
    steam_send_report_byte(fd, 0x85);
}
static void cmd_disable_mouse(int fd, char **args)
{
    steam_write_register(fd, 0x08, 0x07);
}
static void cmd_enable_mouse(int fd, char **args)
{
    steam_send_report_byte(fd, 0x8e);
}
static void cmd_disable_cursor(int fd, char **args)
{
    steam_write_register(fd, 0x07, 0x07);
}
static void cmd_disable_rmargin(int fd, char **args)
{
    steam_write_register(fd, 0x18, 0);
}
static void cmd_enable_rmargin(int fd, char **args)
{
    steam_write_register(fd, 0x18, 1);
}
static void cmd_register(int fd, char **args)
{
    char *end;
    int reg, val;
    reg = strtol(args[0], &end, 0);
    if (!*end)
        val = strtol(args[1], &end, 0);
    if (*end)
    {
        fprintf(stderr, "bad argument for 'register'\n");
        return;
    }
    steam_write_register(fd, reg, val);
}
static void cmd_send(int fd, char **args)
{
    uint8_t buf[64];
    int len = parse_hex(args[0], buf, sizeof(buf));
    if (len <= 0)
    {
        fprintf(stderr, "bad argument for 'send'\n");
        return;
    }
    steam_send_report(fd, buf, len);
}
static void cmd_recv(int fd, char **args)
{
    uint8_t reply[64] = {};
    int res = steam_recv_report(fd, reply);
    if (res > 0)
    {
        for (int i = 0; i < 64 && i < res; ++i)
            printf("%02x", reply[i]);
        printf("\n");
    }
}

static struct command commands[] =
{
    {
        "get_serial", 0,
        ": gets the serial number of the controller",
        cmd_get_serial,
    },
    {
        "disable_keys_cursor", 0,
        ": disables the emulation of ENTER, ESC, LBUTTON, RBUTTON and cursor keys by the joystick",
        cmd_disable_keys_cursor,
    },
    {
        "enable_keys_cursor", 0,
        ": enables the emulation of ENTER, ESC, LBUTTON, RBUTTON and cursor keys by the joystick",
        cmd_enable_keys_cursor,
    },
    {
        "disable_mouse", 0,
        ": disables the emulation of the mouse by the right pad",
        cmd_disable_mouse,
    },
    {
        "enable_mouse", 0,
        ": enables the emulation of the mouse by the right pad",
        cmd_enable_mouse,
    },
    {
        "disable_cursor", 0,
        ": disables the emulation of the cursor keys by the joystick",
        cmd_disable_cursor,
    },
    {
        "disable_rmargin", 0,
        ": disables the dead margin on the right pad",
        cmd_disable_rmargin,
    },
    {
        "enable_rmargin", 0,
        ": enables the dead margin on the right pad",
        cmd_enable_rmargin,
    },
    /* WARNING: The following commands are undocumented because they can be used to do evil.
     * Use them at your own responsibility.
     */
    {
        "register", 2,
        NULL, //"R X: writes value X (16 bits) into register R (8 bits)",
        cmd_register,
    },
    {
        "send", 1,
        NULL, //"xxxx: sends the given data as a feature report, hex values without separators"
        cmd_send,
    },
    {
        "recv", 0,
        NULL, //": receives a feature report and prints it on stdout"
        cmd_recv,
    },
    {}
};

static const struct option long_options[] =
{
    { "help", no_argument, NULL, 'h' },
    { "serial", required_argument, NULL, 's' },
    { "device", required_argument, NULL, 'd' },
    {},
};

static void usage(void)
{
    printf("USAGE:\n");
    printf("   %s [OPTION]* [COMMAND]*\n", program_invocation_short_name);

    printf("\n");
    printf("where OPTION can be:\n");
    printf("   -h or --help: print this message\n");
    printf("   -s SN or --serial SN: looks for the controller with serial number SN.\n");
    printf("   -d DEV or --device DEV: Uses DEV as the hidraw device node, such as /dev/hidraw0.\n");
    printf("If no --serial nor --device option is specified, it will use the first device found.\n");
    printf("\n");
    printf("where COMMAND can be:\n");
    for (struct command *cmds = commands; cmds->command; ++cmds)
        if (cmds->help)
            printf("   %s%s.\n", cmds->command, cmds->help);
    printf("\n");
    printf("If you want to run a configuration upon connection you can install a udev rule such as:\n");
    printf("ACTION==\"add\", SUBSYSTEM==\"input\", ATTR{uniq}==\"?*\", ATTRS{idVendor}==\"28de\", RUN+=\"/usr/bin/steamctrl -s $attr{uniq} disable_keys_cursor disable_mouse\"\n");
}

int main(int argc, char **argv)
{
    struct steam_find_info info =
    {
        .fd = -1,
        .dev_name = NULL,
    };

    for (;;)
    {
        int option_index = 0;
        int c = getopt_long(argc, argv, "hvs:d:", long_options, &option_index);
        if (c == -1)
            break;
        switch (c)
        {
        case 'v':
            verbose = 1;
            break;
        case 's':
            strncpy(info.serial, optarg, sizeof(info.serial));
            info.serial[sizeof(info.serial)-1] = 0;
            break;
        case 'd':
            info.dev_name = strdup(optarg);
            break;
        default:
            usage();
            return EXIT_FAILURE;
        }
    }

    if (!info.dev_name)
    {
        struct udev *ud = udev_new();
        info.wireless = 0;
        if (!find_udev_devices(steam_usb_cb, &info, ud, NULL, "usb", "idVendor", "28de", "idProduct", "1102", NULL)) //wired
        {
            info.wireless = 1;
            find_udev_devices(steam_usb_cb, &info, ud, NULL, "usb", "idVendor", "28de", "idProduct", "1142", NULL); //wireless
        }
        udev_unref(ud);
    }

    if (!info.dev_name)
    {
        fprintf(stderr, "Steam Controller not found\n");
        return EXIT_FAILURE;
    }
    if (verbose)
        fprintf(stderr, "Device '%s'\n", info.dev_name);

    /* If -d is used, the device is not opened yet. */
    int fd;
    if (info.fd != -1)
    {
        fd = info.fd;
    }
    else
    {
        fd = open(info.dev_name, O_RDWR);
        if (fd < 0)
        {
            perror(info.dev_name);
            return EXIT_FAILURE;
        }
    }
    free(info.dev_name);
    info.dev_name = NULL;

    for (int i = optind; i < argc; ++i)
    {
        const char *cmd = argv[i];

        struct command *cmds;
        for (cmds = commands; cmds->command; ++cmds)
        {
            if (strcmp(cmds->command, cmd) == 0)
                break;
        }
        if (!cmds->command)
        {
            fprintf(stderr, "unknown command '%s'\n", cmd);
            continue;
        }
        if (i >= argc - cmds->nargs)
        {
            fprintf(stderr, "missing argument for '%s'\n", cmd);
            break;
        }

        cmds->func(fd, cmds->nargs ? &argv[i + 1] : NULL);
        i += cmds->nargs;
    }

    close(fd);
    return EXIT_SUCCESS;
}
