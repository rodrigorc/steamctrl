#ifndef UDEV_HELPER_H_INCLUDED
#define UDEV_HELPER_H_INCLUDED

#include <libudev.h>

/* returns 0 to keep on, 1 to stop */
typedef int find_callback(void *ptr, struct udev *ud, const char *sys_name);

/* returns 1 iff callback returned 1 */
int find_udev_devices(find_callback *cb, void *ptr, struct udev *ud, struct udev_device *parent, const char *subsystem, 
        ... /* const char *attr, const char *value, */);

#endif /* UDEV_HELPER_H_INCLUDED */
