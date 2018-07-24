/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2018 Rangel Ivanov <rangelivanov88@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#ifndef LIBSIGROK_HARDWARE_BLACKMESALABS_SUMP2_PROTOCOL_H
#define LIBSIGROK_HARDWARE_BLACKMESALABS_SUMP2_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "blackmesalabs-sump2"

#define SUMP2_HWID 0xabba
#define SUMP2_CTRL_ADDR 0x90
#define SUMP2_DATA_ADDR 0x94

#define SUMP2_CMD_STATE_IDLE 0x00
#define SUMP2_CMD_STATE_ARM 0x01
#define SUMP2_CMD_STATE_RESET 0x02

#define SUMP2_READ_HWID 0x0b
#define SUMP2_READ_RAM_WIDTH 0xc
#define SUMP2_READ_SAMPLE_FREQ 0x0d
#define SUMP2_READ_TRIGGER_PATTERN 0x0e
#define SUMP2_READ_RAM_DATA 0x0f



struct dev_context {
};

SR_PRIV struct dev_context *sump2_dev_new(void);

SR_PRIV int blackmesalabs_sump2_receive_data(int fd, int revents, void *cb_data);

SR_PRIV int mesabus_write(struct sr_serial_dev_inst *serial, uint32_t addr, uint32_t command);

SR_PRIV int mesabus_read(struct sr_serial_dev_inst *serial, uint32_t addr, uint32_t num_dwords);

SR_PRIV int sump2_autobaud(struct sr_serial_dev_inst *serial);

SR_PRIV int sump2_read(struct sr_serial_dev_inst *serial, uint32_t addr);
#endif
