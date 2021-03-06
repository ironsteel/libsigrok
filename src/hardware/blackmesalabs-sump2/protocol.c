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

#include <config.h>
#include <stdio.h>
#include <errno.h>
#include "protocol.h"

char *bytes_len = "08";

#define MESA_BUS_PREAMBLE "F0"
#define SLOT "00"
#define SUBSLOT "0"

#define MESA_BURST_WRITE "0"

#define MESA_READ_REPEAT "3"

#define MESABUS_CMD_LEN 25

SR_PRIV struct dev_context *sump2_dev_new(void)
{
	struct dev_context *devc;

	devc = g_malloc0(sizeof(struct dev_context));

	return devc;
}

SR_PRIV int sump2_init(struct sr_dev_inst *sr)
{
	struct sr_serial_dev_inst* serial;
	struct dev_context *devc;

	uint32_t hw_info;
	uint32_t ram_data;
	uint32_t samplerate_data;
	float freq;

	serial = sr->conn;
	devc = sr->priv;

	hw_info = sump2_read(serial, SUMP2_READ_HWID);

	devc->hw_rev = (hw_info & 0x0000FF00) >> 8;
	devc->data_en = (hw_info & 0x00000040) >> 6;
	// TODO: implement support for this
	devc->trigger_wd_en = (hw_info & 0x00000020) >> 5;
	devc->nonrle_disabled = (hw_info & 0x00000010) >> 4;
	devc->rle_enabled = (hw_info & 0x00000008) >> 3;
	// TODO: implement support for these
	devc->pattern_en = (hw_info & 0x00000004) >> 2;
	devc->trigger_nth_en = (hw_info & 0x00000002) >> 1;
	devc->trigger_delay_en = (hw_info & 0x00000001) >> 0;

	ram_data = sump2_read(serial, SUMP2_READ_RAM_WIDTH);

	devc->ram_len = (ram_data & 0x0000FFFF) >> 0;
	devc->ram_dwords = (ram_data & 0x00FF0000) >> 14;
	devc->ram_event_bytes = (ram_data & 0x0F000000) >> 24;
	devc->ram_rle = ( ram_data  & 0xF0000000) >> 28;

	samplerate_data = sump2_read(serial, SUMP2_READ_SAMPLE_FREQ);
	devc->samplerate = SR_MHZ(samplerate_data / 65536);

	// TODO: handle trigger position
	devc->acq_len = 44;

	sump2_init_signal_list(sr);

	sump2_write(serial, cmd_wr_user_ctrl, 0x00000000);
	sump2_write(serial, cmd_wr_watchdog_time, 0x00001000);
	sump2_write(serial, cmd_wr_user_pattern0, 0x0000FFFF);
	sump2_write(serial, cmd_wr_user_pattern1, 0x000055FF);
	sump2_write(serial, cmd_wr_trig_type, trig_pat_ris);
	sump2_write(serial, cmd_wr_trig_field, 0x00000000);
	sump2_write(serial, cmd_wr_trig_dly_nth, 0x00000001);
	sump2_write(serial, cmd_wr_trig_position, devc->ram_len/2); 
	sump2_write(serial, cmd_wr_rle_event_en, 0xFFFFFFFF); 

	sump2_write(serial,SUMP2_CMD_STATE_RESET , 0x00000000); 


	return SR_OK;
}

SR_PRIV int sump2_init_signal_list(struct sr_dev_inst *sr)
{
	struct sr_serial_dev_inst* serial;
	struct dev_context *devc;
	uint32_t num_channels;
	int i;
	char chan_name[3];

	serial = sr->conn;
	devc = sr->priv;

	num_channels = devc->ram_event_bytes * 8;

	for (i = 0; i < num_channels; i++) {
		sprintf(chan_name, "%d", i);
		sr_channel_new(sr, i, SR_CHANNEL_LOGIC, TRUE, chan_name);
	}
}

SR_PRIV int sump2_autobaud(struct sr_serial_dev_inst *serial)
{
	int wrote = serial_write_blocking(serial, "\n", 1, serial_timeout(serial, 1));
	sr_dbg("Wrote %d", wrote);
	if (wrote != 1)
		return SR_ERR;

	if (serial_drain(serial) != 0)
		return SR_ERR;

	return SR_OK;
}

SR_PRIV int sump2_write(struct sr_serial_dev_inst *serial, uint32_t addr, uint32_t data)
{
	if (mesabus_write(serial, SUMP2_CTRL_ADDR, addr) != SR_OK) {
		return SR_ERR;
	}
	
	if (mesabus_write(serial, SUMP2_DATA_ADDR, data) != SR_OK) {
		return SR_ERR;
	}
	return SR_OK;
}

SR_PRIV int sump2_read(struct sr_serial_dev_inst *serial, uint32_t addr)
{
	char buf[18];
	char payload[9];
	int ret;
	uint32_t response;

	if (mesabus_write(serial, SUMP2_CTRL_ADDR, addr) != SR_OK) {
		return SR_ERR;
	}

	if (mesabus_read(serial, SUMP2_DATA_ADDR, 1) != SR_OK) {
		return SR_ERR;
	}

	ret = serial_read_blocking(serial, buf, 17, serial_timeout(serial, 1));
	if (ret != 17) {
		sr_err("Invalid reply (expected 17 bytes, got %d).", ret);
		return SR_ERR;
	}
	buf[17] = '\0';

	sr_dbg("READING %s", buf);

	// Strip the first 8 chars and get only the payload
	// F0FE - being the preamble and slot
	// 0004 - being the byte length of the payload
	strcpy(payload, (buf + strlen(buf)) - 9);

	// Convert hex string payload to int
	response = strtol(payload, NULL, 16);
	if (errno == ERANGE) {
		sr_err("invalid hex value %s", payload);
		return SR_ERR;
	}

	sr_dbg("RESPONSE %x", response);
	return response;
}

SR_PRIV int mesabus_write(struct sr_serial_dev_inst *serial, uint32_t addr, uint32_t command)
{
	char mesa_cmd[26];
	char cmd_buf[9];
	char address_buf[9];

	sprintf(address_buf, "%08x", addr);
	sprintf(cmd_buf, "%08x", command);

	sprintf(mesa_cmd, "%s%s%s%s%s%s%s\n", MESA_BUS_PREAMBLE, SLOT, SUBSLOT,
			MESA_BURST_WRITE, bytes_len, address_buf, cmd_buf);


	sr_dbg("Sending command %s", cmd_buf);
	sr_dbg("Sending mesa command %s", mesa_cmd);
	sr_dbg("Mesa bus command len: %ld", strlen(mesa_cmd));
	int wrote = serial_write_blocking(serial, mesa_cmd, strlen(mesa_cmd), serial_timeout(serial, 1));

	if (wrote != 25)
		return SR_ERR;

	if (serial_drain(serial) != 0)
		return SR_ERR;

	return SR_OK;
}

SR_PRIV int mesabus_read(struct sr_serial_dev_inst *serial, uint32_t addr, uint32_t num_dwords) {
	char mesa_cmd[26];
	char num_dwords_buf[9];
	char address_buf[9];

	sprintf(address_buf, "%08x", addr);
	sprintf(num_dwords_buf, "%08x", num_dwords);

	sprintf(mesa_cmd, "%s%s%s%s%s%s%s\n", MESA_BUS_PREAMBLE, SLOT, SUBSLOT,
			MESA_READ_REPEAT, bytes_len, address_buf, num_dwords_buf);
	int wrote = serial_write_blocking(serial, mesa_cmd, strlen(mesa_cmd), serial_timeout(serial, 25));

	sr_dbg("Wrote %d", wrote);
	if (wrote != 25)
		return SR_ERR;

	sr_dbg("Sending command %s", num_dwords_buf);
	sr_dbg("Sending mesa command %s", mesa_cmd);
	sr_dbg("Mesa bus command len: %ld", strlen(mesa_cmd));
	if (serial_drain(serial) != 0)
		return SR_ERR;

	return SR_OK;
}

SR_PRIV int blackmesalabs_sump2_receive_data(int fd, int revents, void *cb_data)
{
	const struct sr_dev_inst *sdi;
	struct dev_context *devc;

	struct sr_serial_dev_inst* serial;
	(void)fd;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	serial = sdi->conn;

	if (revents == G_IO_IN) {
		//sr_dbg("GOT DATA");
	} else {
		mesabus_read(serial, SUMP2_DATA_ADDR, 0);
		sr_dbg("PROBING");
	}

	return TRUE;
}
