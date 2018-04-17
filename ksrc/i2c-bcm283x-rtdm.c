/**
 * Copyright (C) 2017 Sergio J. Munoz Lopez <semulopez@gmail.com>
 * 
 *    Tutor: Corrado Guarino Lo Bianco <guarino@ce.unipr.it>
 * 
 *     Inspired in 'Real-Time SPI driver':
 *         Github: https://github.com/nicolas-schurando/spi-bcm283x-rtdm
 *         Author: Nicolas Schurando <schurann@ext.essilor.com>
 *         Version: 0.1-untracked
 *    
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* Self header */
#include "../include/i2c-bcm283x-rtdm.h"

/* Linux headers */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/errno.h>

/* RTDM headers */
#include <rtdm/rtdm.h>
#include <rtdm/driver.h>

/* BCM2835 library header */
#include "bcm2835.h"

/**
 * Buffer type.
 */
typedef struct buffer_s {
	size_t size;
	char data[BCM283X_I2C_BUFFER_SIZE_MAX];
} buffer_t;

/**
 * Device config structure stored inside each context.
 */
typedef struct config_s {
	uint8_t slave_address;
	char register_address;
	char* cmds;
	uint8_t cmds_size;
	int baudrate;
	int clock_divider;
	uint8_t flags; // bit [0] -> READ REPEATED START | bit [1] -> WRITE REPEATED START | bit [2] -> DEBUG MODE | bit [3] -> RECONFIGURE DEVICE EACH WRITE/READ
} config_t;

/**
 * Device context, associated with every open device instance.
 */
typedef struct i2c_bcm283x_context_s {
	config_t config;
	buffer_t transmit_buffer;
	buffer_t receive_buffer;
} i2c_bcm283x_context_t;

/**
 * This structure contain the RTDM device created for I2C/BSC1 (position [0]).
 */
static struct rtdm_device i2c_bcm283x_devices[1];

/**
 * Open handler. Note: opening a named device instance always happens from secondary mode.
 * @param[in] fd File descriptor associated with opened device instance.
 * @param[in] oflags Open flags as passed by the user.
 * @return 0 on success. On failure, a negative error code is returned.
 */
static int bcm283x_i2c_rtdm_open(struct rtdm_fd *fd, int oflags) {

	i2c_bcm283x_context_t *context;

	/* Retrieve context */
	context = (i2c_bcm283x_context_t *) rtdm_fd_to_private(fd);

	/* Set default clock config */
	context->config.clock_divider = BCM2835_I2C_CLOCK_DIVIDER_626;
	
	/* Set flags */
	context->config.flags = oflags;
	
	return 0;

}

/**
 * Close handler. Note: closing a device instance always happens from secondary mode.
 * @param[in] fd File descriptor associated with opened device instance.
 * @return 0 on success. On failure return a negative error code.
 */
static void bcm283x_i2c_rtdm_close(struct rtdm_fd *fd) {

	return;

}

/**
 * Read from the device. If the bit [0] of flags is activated repeated start is enabled.
 * @param[in] fd File descriptor.
 * @param[out] buf Input buffer as passed by the user.
 * @param[in] size Number of bytes the user requests to read.
 * @return On success, the number of bytes read. On failure return either -ENOSYS, to request that this handler be called again from the opposite realtime/non-realtime context, or another negative error code.
 */
static ssize_t bcm283x_i2c_rtdm_read_rt(struct rtdm_fd *fd, void __user *buf, size_t size) {

	i2c_bcm283x_context_t *context;
	int res, i;

	/* Retrieve context */
	context = (i2c_bcm283x_context_t *) rtdm_fd_to_private(fd);

	/* Limit size */
	context->receive_buffer.size = (size > BCM283X_I2C_BUFFER_SIZE_MAX) ? BCM283X_I2C_BUFFER_SIZE_MAX : size;
	
	//DEBUG OUTPUT 
	if(context->config.flags&4)
		printk(KERN_DEBUG "%s: READ_SIZE (%d).\r\n", __FUNCTION__, size);
	
	/*  Reconfigure device  */
	if(context->config.flags&8){
		
		/* Set slave address */
		bcm2835_i2c_setSlaveAddress(context->config.slave_address);
		
		/*  Set bus speed  */
		if (context->config.clock_divider == 0)
			bcm2835_i2c_set_baudrate((uint32_t)context->config.baudrate);
		else
			bcm2835_i2c_setClockDivider((uint16_t)context->config.clock_divider);
	
	}
	
	/* Select between normal read or with repeated start */
	if(!(context->config.flags&1))
		res = bcm2835_i2c_read(context->receive_buffer.data, context->receive_buffer.size);
	else if(context->config.register_address > 0)
		res = bcm2835_i2c_read_register_rs(&context->config.register_address, context->receive_buffer.data, context->receive_buffer.size);	

	//DEBUG OUTPUT
	if(context->config.flags&4)
		printk(KERN_DEBUG "%s: READ_RETURN_CODE (0x%02x).\r\n", __FUNCTION__, res);

	//DEBUG OUTPUT
	if(context->config.flags&4)
		for (i=0; i < context->receive_buffer.size; i++)
			printk(KERN_DEBUG "%s: <<READ (0x%02x).\r\n", __FUNCTION__, context->receive_buffer.data[i]);

	/* Copy data to user space */
	res = rtdm_safe_copy_to_user(fd, buf, (const void *)context->receive_buffer.data, context->receive_buffer.size);
	if (res) {
		printk(KERN_ERR "%s: Can't copy data from driver to user space (%d)!\r\n", __FUNCTION__, res);
		return (res < 0) ? res : -res;
	}

	/* Return read bytes */
	return (ssize_t)context->receive_buffer.size;

}

/**
 * Write to the device.
 * @param[in] fd File descriptor.
 * @param[in,out] buf Output buffer as passed by the user.
 * @param[in] size Number of bytes the user requests to write.
 * @return On success, the I2C return code. On failure return either -ENOSYS, to request that this handler be called again from the opposite realtime/non-realtime context, or another negative error code.
 */
static int bcm283x_i2c_rtdm_write_rt(struct rtdm_fd *fd, const void __user *buf, size_t size) {

	i2c_bcm283x_context_t *context;
	int res, i;

	/* Retrieve context */
	context = (i2c_bcm283x_context_t *) rtdm_fd_to_private(fd);
	
	/* Ensure that there will be enough space in the buffer */
	if (size > BCM283X_I2C_BUFFER_SIZE_MAX) {
		printk(KERN_ERR "%s: Trying to transmit data larger than buffer size !", __FUNCTION__);
		return -EINVAL;
	}
	
	context->transmit_buffer.size = size;
	
	//DEBUG OUTPUT
	if(context->config.flags&4)
		printk(KERN_DEBUG "%s: WRITE_SIZE (%d).\r\n", __FUNCTION__, context->transmit_buffer.size);
	
	/*  Reconfigure device  */
	if(context->config.flags&8){
		
		/* Set slave address */
		bcm2835_i2c_setSlaveAddress(context->config.slave_address);
		
		/*  Set bus speed  */
		if (context->config.clock_divider == 0)
			bcm2835_i2c_set_baudrate((uint32_t)context->config.baudrate);
		else
			bcm2835_i2c_setClockDivider((uint16_t)context->config.clock_divider);
	
	}
	
	/* Save data in kernel space buffer */
	res = rtdm_safe_copy_from_user(fd, (void *)context->transmit_buffer.data, (const void *)buf, context->transmit_buffer.size);
	if (res) {
		printk(KERN_ERR "%s: Can't copy data from user space to driver (%d)!\r\n", __FUNCTION__, res);
		return (res < 0) ? res : -res;
	}
	
	//DEBUG OUTPUT
	if(context->config.flags&4)
		for (i=0; i < context->transmit_buffer.size; i++)
			printk(KERN_DEBUG "%s: >>WRITE (0x%02x).\r\n", __FUNCTION__, context->transmit_buffer.data[i]);

	/* Select between normal write or with repeated start */
	if(!(context->config.flags&2)){
		bcm2835_i2c_write(context->transmit_buffer.data, context->transmit_buffer.size);
		
		//DEBUG OUTPUT
		if(context->config.flags&4)
			printk(KERN_DEBUG "%s: WRITE_RETURN_CODE (0x%02x).\r\n", __FUNCTION__, res);

	}else if(context->config.cmds_size > 0){
	
		res = bcm2835_i2c_write_read_rs(context->config.cmds, (uint32_t)context->config.cmds_size, context->transmit_buffer.data, (uint32_t)context->transmit_buffer.size);

		//DEBUG OUTPUT
		if(context->config.flags&4)
			printk(KERN_DEBUG "%s: WRITE_RETURN_CODE (0x%02x).\r\n", __FUNCTION__, res);
		
		/* Copy data to user space */
		res = rtdm_safe_copy_to_user(fd, (void *)context->config.cmds, (const void *)context->transmit_buffer.data, context->transmit_buffer.size);
		if (res) {
			printk(KERN_ERR "%s: Can't copy data from driver to user space (%d)!\r\n", __FUNCTION__, res);
			return (res < 0) ? res : -res;
		}
	}
		
	
	/* Return bytes written */
	return res;
}

/**
 * Changes the baudrate.
 * @param context The context associated with the device.
 * @param value An integer representing the baudrate.
 * @return 0 on success, -EINVAL if the specified value is invalid.
 */
static int bcm283x_i2c_change_baudrate(i2c_bcm283x_context_t *context, const int value) {

	if(value > 0){
		context->config.baudrate = value;
		context->config.clock_divider = 0;
		if(context->config.flags&4)
			printk(KERN_DEBUG "%s: Changing baudrate to %d.\r\n", __FUNCTION__, value);
		bcm2835_i2c_set_baudrate((uint32_t)context->config.baudrate);
		return 0;
	}
	printk(KERN_ERR "%s: Unexpected value!\r\n", __FUNCTION__);
	return -EINVAL;

}

/**
 * Changes the clock divider.
 * @param context The context associated with the device.
 * @param value An integer representing a clock divider preference.
 * @return 0 on success, -EINVAL if the specified value is invalid.
 */
static int bcm283x_i2c_change_clock_divider(i2c_bcm283x_context_t *context, const int value) {

	/*  Check if the value is valid  */
	switch (value) {
		case BCM2835_I2C_CLOCK_DIVIDER_2500:
		case BCM2835_I2C_CLOCK_DIVIDER_626:
		case BCM2835_I2C_CLOCK_DIVIDER_150:
		case BCM2835_I2C_CLOCK_DIVIDER_148:
			
			//DEBUG OUTPUT
			if(context->config.flags&4)
				printk(KERN_DEBUG "%s: Changing clock divider to %d.\r\n", __FUNCTION__, value);
				
			context->config.clock_divider = value;
			context->config.baudrate = 0;
			
			bcm2835_i2c_setClockDivider((uint16_t)context->config.clock_divider);
			return 0;
	}

	printk(KERN_ERR "%s: Unexpected value!\r\n", __FUNCTION__);
	return -EINVAL;
}

/**
 * Changes the slave address.
 * @param context The context associated with the device.
 * @param value An 'uint8-t' with the value of the address.
 * @return 0 on success, -EINVAL if the specified value is invalid.
 */
static int bcm283x_i2c_change_slave_address(i2c_bcm283x_context_t *context, const uint8_t value) {

	/*  Check if the value is valid  */
	if(value > 0){
				
		//DEBUG OUTPUT
		if(context->config.flags&4)
			printk(KERN_DEBUG "%s: Changing slave address to %x.\r\n", __FUNCTION__, value);
		
		context->config.slave_address = value;	
			
		bcm2835_i2c_setSlaveAddress(context->config.slave_address);
		return 0;
	}
	printk(KERN_ERR "%s: Unexpected value!\r\n", __FUNCTION__);
	return -EINVAL;
}

/**
 * Changes the slave register address.
 * @param context The context associated with the device.
 * @param value An 'char' with the value of the register address.
 * @return 0 on success, -EINVAL if the specified value is invalid.
 */
static int bcm283x_i2c_change_slave_register_address(i2c_bcm283x_context_t *context, char value) {

	/*  Check if the value is valid  */
	if(value > 0){
		
		//DEBUG OUTPUT
		if(context->config.flags&4)
			printk(KERN_DEBUG "%s: Changing slave register address to %x.\r\n", __FUNCTION__, value);
		
		context->config.register_address = value;
		return 0;
	}
	printk(KERN_ERR "%s: Unexpected value!\r\n", __FUNCTION__);
	return -EINVAL;
}

/**
 * Changes the commands pointer.
 * @param context The context associated with the device.
 * @param value An 'char' pointer to the commands array.
 * @return 0 on success, -EINVAL if the specified value is invalid.
 */
static int bcm283x_i2c_change_cmds(i2c_bcm283x_context_t *context, char* value) {

	/*  Check if the value is valid  */
	if(value && context->config.cmds_size > 0){
		
		//DEBUG OUTPUT
		if(context->config.flags&4){
			printk(KERN_DEBUG "%s: Changing cmds to 0x%02x (first cmds pos).\r\n", __FUNCTION__, value[0]);
		}
		
		context->config.cmds = value;
		return 0;
	}
	
	if(context->config.cmds_size < 0)
		printk(KERN_ERR "%s: Set first the commands size!\r\n", __FUNCTION__);
	else
		printk(KERN_ERR "%s: Unexpected value!\r\n", __FUNCTION__);
	
	
	return -EINVAL;
}

/**
 * Changes the commands size.
 * @param context The context associated with the device.
 * @param value An 'uint8_t' with the size of the commands array.
 * @return 0 on success, -EINVAL if the specified value is invalid.
 */
static int bcm283x_i2c_change_cmds_size(i2c_bcm283x_context_t *context, const uint8_t value) {

	/*  Check if the value is valid  */
	if(value > 0 && value < BCM283X_I2C_BUFFER_SIZE_MAX){
		
		//DEBUG OUTPUT
		if(context->config.flags&4)
			printk(KERN_DEBUG "%s: Changing cmds size to %d.\r\n", __FUNCTION__, value);
			
		context->config.cmds_size = value;
		return 0;
	}
	printk(KERN_ERR "%s: Unexpected value!\r\n", __FUNCTION__);
	return -EINVAL;
}

/**
 * Changes the flags.
 * @param context The context associated with the device.
 * @param value An 'uint8_t' with the flags preference [MAX_VALUE 7].
 * @return 0 on success, -EINVAL if the specified value is invalid.
 */
static int bcm283x_i2c_set_flags(i2c_bcm283x_context_t *context, const uint8_t value) {

	/*  Check if the value is valid  */
	if(value > 0 && value < 16){
		
		//DEBUG OUTPUT
		if(context->config.flags&4)
			printk(KERN_DEBUG "%s: Changing flags to %d.\r\n", __FUNCTION__, value);

		context->config.flags = value;
		return 0;
	}
	printk(KERN_ERR "%s: Unexpected value!\r\n", __FUNCTION__);
	return -EINVAL;
}

/**
 * IOCTL handler.
 * @param[in] fd File descriptor.
 * @param[in] request Request number as passed by the user.
 * @param[in,out] arg Request argument as passed by the user.
 * @return A positive value or 0 on success. On failure return either -ENOSYS, to request that the function be called again from the opposite realtime/non-realtime context, or another negative error code.
 */
static int bcm283x_i2c_rtdm_ioctl_rt(struct rtdm_fd *fd, unsigned int request, void __user *arg) {

	i2c_bcm283x_context_t *context;
	int interger;
	uint8_t uChar;
	char* charPointer;
	char character;
	int res;

	/* Retrieve context */
	context = (i2c_bcm283x_context_t *) rtdm_fd_to_private(fd);

	/* Analyze request */
	switch (request) {

		case BCM283X_I2C_SET_SLAVE_ADDRESS: /* Set the slave address */
			res = rtdm_safe_copy_from_user(fd, &uChar, arg, sizeof(uint8_t));
			if (res) {
				printk(KERN_ERR "%s: Can't retrieve argument from user space (%d)!\r\n", __FUNCTION__, res);
				return (res < 0) ? res : -res;
			}
			return bcm283x_i2c_change_slave_address(context, uChar);
			
		case BCM283X_I2C_SET_SLAVE_REGISTER_ADDRESS: /* Set the slave register address*/
			res = rtdm_safe_copy_from_user(fd, &character, arg, sizeof(char)); 
			if (res) {
				printk(KERN_ERR "%s: Can't retrieve argument from user space (%d)!\r\n", __FUNCTION__, res);
				return (res < 0) ? res : -res;
			}
			return bcm283x_i2c_change_slave_register_address(context, character);

		case BCM283X_I2C_SET_BAUDRATE: /* Change the baudrate */
			res = rtdm_safe_copy_from_user(fd, &interger, arg, sizeof(int));
			if (res) {
				printk(KERN_ERR "%s: Can't retrieve argument from user space (%d)!\r\n", __FUNCTION__, res);
				return (res < 0) ? res : -res;
			}
			return bcm283x_i2c_change_baudrate(context, interger);

		case BCM283X_I2C_SET_CLOCK_DIVIDER: /* Change the clock divider */
			res = rtdm_safe_copy_from_user(fd, &interger, arg, sizeof(int));
			if (res) {
				printk(KERN_ERR "%s: Can't retrieve argument from user space (%d)!\r\n", __FUNCTION__, res);
				return (res < 0) ? res : -res;
			}
			return bcm283x_i2c_change_clock_divider(context, interger);

		case BCM283X_I2C_SET_CMDS: /* Change the cmd pointer */
			res = rtdm_safe_copy_from_user(fd, &charPointer, arg, sizeof(char *));
			if (res) {
				printk(KERN_ERR "%s: Can't retrieve argument from user space (%d)!\r\n", __FUNCTION__, res);
				return (res < 0) ? res : -res;
			}
			return bcm283x_i2c_change_cmds(context, charPointer);

		case BCM283X_I2C_SET_CMDS_SIZE: /* Change the cmds size */
			res = rtdm_safe_copy_from_user(fd, &uChar, arg, sizeof(uint8_t));
			if (res) {
				printk(KERN_ERR "%s: Can't retrieve argument from user space (%d)!\r\n", __FUNCTION__, res);
				return (res < 0) ? res : -res;
			}
			return bcm283x_i2c_change_cmds_size(context, uChar);

		case BCM283X_I2C_SET_FLAGS: /* Change the flags */
			res = rtdm_safe_copy_from_user(fd, &uChar, arg, sizeof(uint8_t));
			if (res) {
				printk(KERN_ERR "%s: Can't retrieve argument from user space (%d)!\r\n", __FUNCTION__, res);
				return (res < 0) ? res : -res;
			}
			return bcm283x_i2c_set_flags(context, uChar);

		default: /* Unexpected case */
			printk(KERN_ERR "%s: Unexpected request : %d!\r\n", __FUNCTION__, request);
			return -EINVAL;

	}

}

/**
 * This structure describes the RTDM driver.
 */
static struct rtdm_driver i2c_bcm283x_driver = {
	.profile_info = RTDM_PROFILE_INFO(foo, RTDM_CLASS_EXPERIMENTAL, RTDM_SUBCLASS_GENERIC, 42),
	.device_flags = RTDM_NAMED_DEVICE | RTDM_EXCLUSIVE | RTDM_FIXED_MINOR,
	.device_count = 1,
	.context_size = sizeof(struct i2c_bcm283x_context_s),
	.ops = {
		.open = bcm283x_i2c_rtdm_open,
		.read_rt = bcm283x_i2c_rtdm_read_rt,
		.write_rt = bcm283x_i2c_rtdm_write_rt,
		.ioctl_rt = bcm283x_i2c_rtdm_ioctl_rt,
		.close = bcm283x_i2c_rtdm_close
	}
};

/**
 * This function is called when the module is loaded. It initializes the
 * i2c device using the bcm2835 libary, and registers the RTDM device.
 */
static int __init bcm283x_i2c_rtdm_init(void) {

	int res;
	int device_id;

	/* Log */
	printk(KERN_INFO "%s: Starting driver ...", __FUNCTION__);

	/* Ensure cobalt is enabled */
	if (!realtime_core_enabled()) {
		printk(KERN_ERR "%s: Exiting as cobalt is not enabled!\r\n", __FUNCTION__);
		return -1;
	}

	/* Initialize the bcm2835 library */
	res = bcm2835_init();
	if (res != 1) {
		printk(KERN_ERR "%s: Error in bcm2835_init (%d).\r\n", __FUNCTION__, res);
		return -1;
	}

	/* Configure the i2c port from bcm2835 library with arbitrary settings */
	bcm2835_i2c_begin();
	bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_626);

	/* Prepare to register the device */
	for(device_id = 0; device_id < 1; device_id++){

		/* Set device parameters */
		i2c_bcm283x_devices[device_id].driver = &i2c_bcm283x_driver;
		i2c_bcm283x_devices[device_id].label = "i2cdev0.%d";
		i2c_bcm283x_devices[device_id].minor = device_id;

		/* Try to register the device */
		res = rtdm_dev_register(&i2c_bcm283x_devices[device_id]);
		if (res == 0) {
			printk(KERN_INFO "%s: Device i2cdev0.%d registered without errors.\r\n", __FUNCTION__, device_id);
		} else {
			printk(KERN_ERR "%s: Device i2cdev0.%d registration failed : ", __FUNCTION__, device_id);
			switch (res) {
				case -EINVAL:
					printk(KERN_ERR "The descriptor contains invalid entries.\r\n");
					break;

				case -EEXIST:
					printk(KERN_ERR "The specified device name of protocol ID is already in use.\r\n");
					break;

				case -ENOMEM:
					printk(KERN_ERR "A memory allocation failed in the process of registering the device.\r\n");
					break;

				default:
					printk(KERN_ERR "Unknown error code returned.\r\n");
					break;
			}
			return res;
		}
	}

	return 0;

}

/**
 * This function is called when the module is unloaded. It unregisters the RTDM device.
 */
static void __exit bcm283x_i2c_rtdm_exit(void) {

	int device_id;

	/* Log */
	printk(KERN_INFO "%s: Stopping driver ...\r\n", __FUNCTION__);

	/* Ensure cobalt is enabled */
	if (!realtime_core_enabled()) {
		printk(KERN_ERR "%s: Exiting as cobalt is not enabled!\r\n", __FUNCTION__);
		return;
	}

	/* Unregister the two devices */
	for (device_id = 0; device_id < 1; device_id++) {
		printk(KERN_INFO "%s: Unregistering device %d ...\r\n", __FUNCTION__, device_id);
		rtdm_dev_unregister(&i2c_bcm283x_devices[device_id]);
	}

	/* Release the i2c pins */
	bcm2835_i2c_end();

	/* Unmap memory */
	bcm2835_close();

	/* Log */
	printk(KERN_INFO "%s: All done!\r\n", __FUNCTION__);

}

/*
 * Link init and exit functions with driver entry and exit points.
 */
module_init(bcm283x_i2c_rtdm_init);
module_exit(bcm283x_i2c_rtdm_exit);

/*
 * Register module values
 */
#ifndef GIT_VERSION
#define GIT_VERSION 0.1-untracked
#endif /* ! GIT_VERSION */
MODULE_VERSION(GIT_VERSION);
MODULE_DESCRIPTION("Real-Time I2C driver for the Broadcom BCM283x SoC familly using the RTDM API");
MODULE_AUTHOR("Sergio J. Munoz Lopez<semulopez@gmail.com>");
MODULE_LICENSE("GPL v2");
