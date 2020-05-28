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
#include "udev_helper.h"

int find_udev_devices(find_callback *cb, void *ptr, struct udev *ud, struct udev_device *parent, const char *subsystem, 
        ... /* const char *attr, const char *value, */)
{
    va_list av;
    const char *attr, *value;
    int res = 0;

    struct udev_enumerate *ude = udev_enumerate_new(ud);
    if (parent)
        udev_enumerate_add_match_parent(ude, parent);
    if (subsystem)
        udev_enumerate_add_match_subsystem(ude, subsystem);

    va_start(av, subsystem);
    for (;;)
    {
        attr = va_arg(av, const char *);
        if (!attr)
            break;
        value = va_arg(av, const char *);
        udev_enumerate_add_match_sysattr(ude, attr, value);
    }
    va_end(av);

    udev_enumerate_scan_devices(ude);
    struct udev_list_entry *le;
    for (le = udev_enumerate_get_list_entry(ude); le; le = udev_list_entry_get_next(le))
    {
        const char *sys_name = udev_list_entry_get_name(le);
        res = cb(ptr, ud, sys_name);
        if (res)
            break;
    }
    udev_enumerate_unref(ude);
    return res;
}
