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

#ifndef BCM283X_I2C_RTDM_H
#define BCM283X_I2C_RTDM_H

/**
 * Maximum size for transmit and receive buffers.
 */
#define BCM283X_I2C_BUFFER_SIZE_MAX 1024

/**
 * IOCTL request for changing the I2C slave address.
 */
#define BCM283X_I2C_SET_SLAVE_ADDRESS 0

/**
 * IOCTL request for changing the I2C the slave register address.
 */
#define BCM283X_I2C_SET_SLAVE_REGISTER_ADDRESS 1

/**
 * IOCTL request for changing the I2C bus speed by baudrate.
 */
#define BCM283X_I2C_SET_BAUDRATE 2

/**
 * IOCTL request for changing the I2C bus speed by clock divider.
 */
#define BCM283X_I2C_SET_CLOCK_DIVIDER 3

/**
 * IOCTL request for changing the I2C cmds.
 */
#define BCM283X_I2C_SET_CMDS 4

/**
 * IOCTL request for changing the I2C cmds size.
 */
#define BCM283X_I2C_SET_CMDS_SIZE 5

/**
 * IOCTL request for changing the I2C flags.
 */
#define BCM283X_I2C_SET_FLAGS 6

#endif /* BCM283X_I2C_RTDM_H */
