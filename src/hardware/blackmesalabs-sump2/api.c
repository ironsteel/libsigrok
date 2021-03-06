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
#include "protocol.h"

#define SERIALCOMM "921600/8n1"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_LIST,
	SR_CONF_RLE | SR_CONF_GET,
	SR_CONF_TRIGGER_MATCH | SR_CONF_LIST,
};

static const int32_t trigger_matches[] = {
	SR_TRIGGER_RISING,
	SR_TRIGGER_FALLING,
};


static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct sr_config *src;
	struct sr_dev_inst *sdi;
	struct sr_serial_dev_inst *serial;
	GSList *l;
	int ret;
	unsigned int i;
	const char *conn, *serialcomm;

	conn = serialcomm = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		}
	}

	if (!conn) {
		sr_info("No connection");
		return NULL;
	}

	if (!serialcomm)
		serialcomm = SERIALCOMM;

	serial = sr_serial_dev_inst_new(conn, serialcomm);
	sr_info("Probing %s.", conn);

	if (serial_open(serial, SERIAL_RDWR) != SR_OK) {
		return NULL;
	}

	// TODO: tweak the FPGA design to not require this
	if (sump2_autobaud(serial) != SR_OK) {
		serial_close(serial);
		sr_err("Could not autobaud. Quitting.");
		return NULL;
	}

	int hwid_data = sump2_read(serial, SUMP2_READ_HWID);

	int hwid = (hwid_data & 0xFFFF0000) >> 16;
	if (hwid != SUMP2_HWID) {
		sr_err("Unable to locate sump2 hardware, invalid hardware id (expected %x, got %x)", SUMP2_HWID, hwid);
		return NULL;
	}

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("Black Mesa SUMP2");
	sdi->model = g_strdup("Logic Analyzer");
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;
	sdi->priv = sump2_dev_new();
	sump2_init(sdi);

	serial_close(serial);

	return std_scan_complete(di, g_slist_append(NULL, sdi));
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->samplerate);
		break;
	case SR_CONF_RLE:
		*data = g_variant_new_boolean(devc->rle_enabled ? TRUE : FALSE);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;

	(void)sdi;
	(void)data;
	(void)cg;

	ret = SR_OK;
	switch (key) {
	/* TODO */
	default:
		ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	uint64_t samplerates[1];

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case SR_CONF_SAMPLERATE:
		samplerates[0] = devc->samplerate;
		*data = std_gvar_samplerates(ARRAY_AND_SIZE(samplerates));
		break;
	case SR_CONF_TRIGGER_MATCH:
		*data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst* serial;
	struct dev_context *devc;

	struct sr_trigger *trigger;
	struct sr_trigger_stage *stage;
	struct sr_trigger_match *match;
	const GSList *l, *m;

	int channelbit, i = 0;
	
	devc = sdi->priv;
	serial = sdi->conn;

	if (!(trigger = sr_session_trigger_get(sdi->session))) {
		sr_dbg("No session trigger found");
		return SR_ERR;
	}

	for (l = trigger->stages; l; l = l->next) {
		stage = l->data;
		for (m = stage->matches; m; m = m->next) {
			match = m->data;
			if (!match->channel->enabled)
				/* Ignore disabled channels with a trigger. */
				continue;
			channelbit = 1 << (match->channel->index);
			sr_dbg("Channel trigger match %d", match->channel->index);
			devc->trigger_field = channelbit;
			switch (match->match) {
				case SR_TRIGGER_RISING:
					devc->trigger_type = trig_or_ris;  
				break;
				case SR_TRIGGER_FALLING:
					devc->trigger_type = trig_or_fal;  
				break;
			}
		}
	}

	// TODO: Add support for these
	sump2_write(serial, cmd_wr_user_ctrl, 0x00000000);
	sump2_write(serial, cmd_wr_watchdog_time, 0x00001000);
	sump2_write(serial, cmd_wr_user_pattern0, 0x0000FFFF);
	sump2_write(serial, cmd_wr_user_pattern1, 0x000055FF);
	sump2_write(serial, cmd_wr_trig_dly_nth, 0x00000001);
	sump2_write(serial, cmd_wr_rle_event_en, 0xFFFFFFFF); 

	sump2_write(serial, cmd_wr_trig_type, devc->trigger_type);
	sump2_write(serial, cmd_wr_trig_field, devc->trigger_field);
	sump2_write(serial, cmd_wr_trig_position, devc->ram_len/2); 

	sump2_write(serial, SUMP2_CMD_STATE_RESET , 0x00000000); 
	sump2_write(serial, SUMP2_CMD_STATE_ARM , 0x00000000); 

	std_session_send_df_header(sdi);

	//mesabus_read(serial, SUMP2_DATA_ADDR, 0);

	serial_source_add(sdi->session, serial, G_IO_IN, 100,
			blackmesalabs_sump2_receive_data, (struct sr_dev_inst *)sdi);
	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{

	struct sr_serial_dev_inst *serial;

	serial = sdi->conn;
	serial_source_remove(sdi->session, serial);

	std_session_send_df_end(sdi);

	return SR_OK;
}

SR_PRIV struct sr_dev_driver blackmesalabs_sump2_driver_info = {
	.name = "blackmesalabs-sump2",
	.longname = "BlackMesaLabs SUMP2",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};

SR_REGISTER_DEV_DRIVER(blackmesalabs_sump2_driver_info);
