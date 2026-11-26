/*
 * \brief  Implementation of driver specific Linux functions
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \date   2026-10-28
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <lx_emul.h>

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/sysfs.h>


void lx_emul_usb_client_device_unregister_callback(struct usb_device *udev) { }


#include <linux/uaccess.h>

#ifndef INLINE_COPY_TO_USER
unsigned long _copy_from_user(void * to,const void __user * from,unsigned long n)
{
        memcpy(to, from, n);
        return 0;
}


unsigned long raw_copy_from_user(void *to, const void * from, unsigned long n)
{
        memcpy(to, from, n);
        return 0;
}


#else
unsigned long __must_check __arch_copy_from_user(void *to, const void __user *from, unsigned long n);
unsigned long __must_check __arch_copy_from_user(void *to, const void __user *from, unsigned long n)
{
        memcpy(to, from, n);
        return 0;
}
#endif


#ifndef INLINE_COPY_FROM_USER
unsigned long _copy_to_user(void __user * to,const void * from,unsigned long n)
{
        memcpy(to, from, n);
        return 0;
}


unsigned long raw_copy_to_user(void *to, const void *from, unsigned long n)
{
        memcpy(to, from, n);
        return 0;
}
#else
unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n);
unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n)
{
        memcpy(to, from, n);
        return 0;
}
#endif


unsigned long arm_copy_from_user(void *to, const void *from, unsigned long n)
{
        memcpy(to, from, n);
        return 0;
}
