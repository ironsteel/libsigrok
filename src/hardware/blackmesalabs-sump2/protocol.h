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

#define trig_and_ris           0x00// Bits AND Rising
#define trig_and_fal           0x01// Bits AND Falling
#define trig_or_ris            0x02// Bits OR  Rising
#define trig_or_fal            0x03// Bits OR  Falling
#define trig_pat_ris           0x04// Pattern Match Rising
#define trig_pat_fal           0x05// Pattern Match Falling
#define trig_in_ris            0x06// External Input Trigger Rising
#define trig_in_fal            0x07// External Input Trigger Falling
#define trig_watchdog          0x08// Watchdog trigger

#define cmd_wr_trig_type       0x04
#define cmd_wr_trig_field      0x05//  Correspond to Event Bits
#define cmd_wr_trig_dly_nth    0x06// Trigger Delay and Nth
#define cmd_wr_trig_position   0x07// Samples post Trigger to Capture
#define cmd_wr_rle_event_en    0x08// Enables events for RLE detection
#define cmd_wr_ram_ptr         0x09// Load specific pointer.
#define cmd_wr_ram_page        0x0a//  Load DWORD Page.
    
#define cmd_wr_user_ctrl       0x10
#define cmd_wr_user_pattern0   0x11// Also Mask    for Pattern Matching
#define cmd_wr_user_pattern1   0x12// Also Pattern for Pattern Matching
#define cmd_wr_user_data_en    0x13// Special Data Enable Capture Mode 
#define cmd_wr_watchdog_time   0x14// Watchdog Timeout
    
#define status_ram_post        0x04// Engine has filled post-trig RAM 
struct dev_context {
	int hw_rev;
	int data_en;
	int trigger_wd_en;
	int nonrle_disabled;
	int rle_enabled;
	int pattern_en;
	int trigger_nth_en;
	int trigger_delay_en;
	uint32_t samplerate;

	int ram_len;
	int ram_dwords;
	int ram_event_bytes;
	int ram_rle;

        uint32_t trigger_field;
        uint32_t trigger_type;

        uint32_t acq_len;
};

SR_PRIV struct dev_context *sump2_dev_new(void);

SR_PRIV int blackmesalabs_sump2_receive_data(int fd, int revents, void *cb_data);

SR_PRIV int mesabus_write(struct sr_serial_dev_inst *serial, uint32_t addr, uint32_t command);

SR_PRIV int mesabus_read(struct sr_serial_dev_inst *serial, uint32_t addr, uint32_t num_dwords);

SR_PRIV int sump2_autobaud(struct sr_serial_dev_inst *serial);

SR_PRIV int sump2_read(struct sr_serial_dev_inst *serial, uint32_t addr);

SR_PRIV int sump2_init(struct sr_dev_inst *sr);
#endif
