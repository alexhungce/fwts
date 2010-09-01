/*
 * Copyright (C) 2010 Canonical
 *
 * Portions of this code original from the Linux-ready Firmware Developer Kit
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/io.h> 
#include <unistd.h>
#include <string.h>

uint8 *fadt_table;
int    fadt_size;

/*
 *  From ACPI Spec, section 5.2.9 Fixed ACPI Description Field
 */
typedef struct {
	fwts_acpi_table_header	header;	
	uint32			firmware_control;
	uint32			dsdt;
	uint8			reserved;
	uint8			preferred_pm_profile;
	uint16			sci_int;
	uint16			smi_cmd;
	uint8			acpi_enable;
	uint8			acpi_disable;
	uint8			s4bios_req;
	uint8			pstate_cnt;
	uint32			pm1a_evt_blk;
	uint32			pm1b_evt_blk;
	uint32			pm1a_cnt_blk;
	uint32			pm1b_cnt_blk;
	uint32			pm2_cnt_blk;
	uint32			pm_tmr_blk;
	uint32			gpe0_blk;
	uint32			gpe1_blk;
	uint8			pm1_evt_len;
	uint8			pm1_cnt_len;
	uint8			pm2_cnt_len;
	uint8			pm_tmr_len;
	uint8			gpe0_blk_len;
	uint8			gpe1_blk_len;
	uint8			cst_cnt;
	uint16			p_lvl2_lat;
	uint16			p_lvl3_lat;
	uint16			flush_size;
	uint16			flush_stride;
	uint8			duty_offset;
	uint8			duty_width;
	uint8			day_alrm;
	uint8			mon_alrm;
	uint8			century;
	uint16			iapc_boot_arch;
	uint8			reserved1;
	uint32			flags;
	fwts_gas		reset_reg;
	uint8			reset_value;
	uint8			reserved2;
	uint8			reserved3;
	uint8			reserved4;
	uint64			x_firmware_control;
	uint64			x_dsdt;
	fwts_gas		x_pm1a_evt_blk;
	fwts_gas		x_pm1b_evt_blk;
	fwts_gas		x_pm1a_cnt_blk;
	fwts_gas		x_pm1b_cnt_blk;
	fwts_gas		x_pm2_cnt_blk;
	fwts_gas		x_pm_tmr_blk;
	fwts_gas		x_gpe0_blk;
	fwts_gas		x_gpe1_blk;
} fwts_fadt_version_2;


static void fadt_get_header(unsigned char *fadt_data, int size, fwts_fadt_version_2 *hdr)
{
	uint8 data[244];

	/* Copy to a V2 sized buffer incase we have read in a V1 smaller sized one */
	memset(data, 0, sizeof(data));
	memcpy(data, fadt_data, size);

	memcpy(&hdr->header, data, 36);
	FWTS_GET_UINT32(hdr->firmware_control, data, 36);
	FWTS_GET_UINT32(hdr->dsdt, data, 40);
	FWTS_GET_UINT8 (hdr->reserved, data, 44);
	FWTS_GET_UINT8 (hdr->preferred_pm_profile, data, 45);
	FWTS_GET_UINT16(hdr->sci_int, data, 46);
	FWTS_GET_UINT32(hdr->smi_cmd, data, 48);
	FWTS_GET_UINT8 (hdr->acpi_enable, data, 52);
	FWTS_GET_UINT8 (hdr->acpi_disable, data, 53);
	FWTS_GET_UINT8 (hdr->s4bios_req, data, 54);
	FWTS_GET_UINT8 (hdr->pstate_cnt, data, 55);
	FWTS_GET_UINT32(hdr->pm1a_evt_blk, data, 56);
	FWTS_GET_UINT32(hdr->pm1b_evt_blk, data, 60);
	FWTS_GET_UINT32(hdr->pm1a_cnt_blk, data, 64);
	FWTS_GET_UINT32(hdr->pm1b_cnt_blk, data, 68);
	FWTS_GET_UINT32(hdr->pm2_cnt_blk, data, 72);
	FWTS_GET_UINT32(hdr->pm_tmr_blk, data, 80);
	FWTS_GET_UINT32(hdr->gpe0_blk, data, 84);
	FWTS_GET_UINT32(hdr->gpe1_blk, data, 88);
	FWTS_GET_UINT8 (hdr->pm1_evt_len, data, 89);
	FWTS_GET_UINT8 (hdr->pm1_cnt_len, data, 90);
	FWTS_GET_UINT8 (hdr->pm2_cnt_len, data, 91);
	FWTS_GET_UINT8 (hdr->pm_tmr_len, data, 92);
	FWTS_GET_UINT8 (hdr->gpe0_blk_len, data, 93);
	FWTS_GET_UINT8 (hdr->gpe1_blk_len, data, 94);
	FWTS_GET_UINT8 (hdr->cst_cnt, data, 95);
	FWTS_GET_UINT16(hdr->p_lvl2_lat, data, 96);
	FWTS_GET_UINT16(hdr->p_lvl3_lat, data, 98);
	FWTS_GET_UINT16(hdr->flush_size, data, 100);
	FWTS_GET_UINT16(hdr->flush_stride, data, 102);
	FWTS_GET_UINT8 (hdr->duty_offset, data, 104);
	FWTS_GET_UINT8 (hdr->duty_width, data, 105);
	FWTS_GET_UINT8 (hdr->day_alrm, data, 106);
	FWTS_GET_UINT8 (hdr->mon_alrm, data, 107);
	FWTS_GET_UINT8 (hdr->century, data, 108);
	FWTS_GET_UINT16(hdr->iapc_boot_arch, data, 109);
	FWTS_GET_UINT8 (hdr->reserved1, data, 111);
	FWTS_GET_UINT32(hdr->flags, data, 112);
	FWTS_GET_GAS   (hdr->reset_reg, data, 116);
	FWTS_GET_UINT8 (hdr->reset_value, data, 128);
	FWTS_GET_UINT8 (hdr->reserved2, data, 129);
	FWTS_GET_UINT8 (hdr->reserved3, data, 130);
	FWTS_GET_UINT8 (hdr->reserved4, data, 131);
	FWTS_GET_UINT64(hdr->x_firmware_control, data, 132);
	FWTS_GET_UINT64(hdr->x_dsdt, data, 140);
	FWTS_GET_GAS   (hdr->x_pm1a_evt_blk, data, 148);
	FWTS_GET_GAS   (hdr->x_pm1b_evt_blk, data, 160);
	FWTS_GET_GAS   (hdr->x_pm1a_cnt_blk, data, 172);
	FWTS_GET_GAS   (hdr->x_pm1b_cnt_blk, data, 184);
	FWTS_GET_GAS   (hdr->x_pm2_cnt_blk, data, 196);
	FWTS_GET_GAS   (hdr->x_pm_tmr_blk, data, 208);
	FWTS_GET_GAS   (hdr->x_gpe0_blk, data, 220);
	FWTS_GET_GAS   (hdr->x_gpe1_blk, data, 232);
}

static int fadt_init(fwts_framework *fw)
{
	if (fwts_check_root_euid(fw))
		return FWTS_ERROR;

	if ((fadt_table = fwts_acpi_table_load(fw, "FACP", 0, &fadt_size)) == NULL) {
		fwts_log_error(fw, "Failed to read ACPI FADT");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

static int fadt_deinit(fwts_framework *fw)
{
	if (fadt_table)
		free(fadt_table);

	return FWTS_OK;
}

static char *fadt_headline(void)
{
	return "Check if FADT SCI_EN bit is enabled.";
}

static int fadt_test1(fwts_framework *fw)
{
	fwts_fadt_version_2  fadt;
	uint32 port, width, value;
	char *profile;

	/*  Not having a FADT is not a failure */
	if (fadt_size == 0) {
		fwts_log_info(fw, "FADT does not exist, this is not necessarily a failure.");
		return FWTS_OK;
	}

	fadt_get_header(fadt_table, fadt_size, &fadt);

	/* Sanity check profile */
	switch (fadt.preferred_pm_profile) {
	case 0:
		profile = "Unspecified";
		break;
	case 1:
		profile = "Desktop";
		break;
	case 2:
		profile = "Mobile";
		break;
	case 3:
		profile = "Workstation";
		break;
	case 4:
		profile = "Enterprise Server";
		break;
	case 5:
		profile = "SOHO Server";
		break;
	case 6:
		profile = "Appliance PC";
		break;
	case 7:
		profile = "Performance Server";
		break;
	default:
		profile = "Reserved";
		fwts_failed_low(fw, "FADT Preferred PM Profile is Reserved - this may be incorrect.");
		break;
	}

	fwts_log_info(fw, "FADT Preferred PM Profile: %d (%s)\n", 
		fadt.preferred_pm_profile, profile);
	
	port = fadt.pm1a_cnt_blk;
	width = fadt.pm1_cnt_len * 8;	/* In bits */

	/* Punt at 244 byte FADT is V2 */
	if (fadt.header.length == 244) {
		/*  Sanity check sizes with extended address variants */
		fwts_log_info(fw, "FADT is greater than ACPI version 1.0");
		if ((uint64)port != fadt.x_pm1a_cnt_blk.address) 
			fwts_failed_medium(fw, "32 and 64 bit versions of FADT pm1_cnt address do not match (0x%8.8x vs 0x%16.16llx).", port, fadt.x_pm1a_cnt_blk.address);
		if (width != fadt.x_pm1a_cnt_blk.register_bit_width)
			fwts_failed_medium(fw, "32 and 64 bit versions of FADT pm1_cnt size do not match (0x%x vs 0x%x).", width, fadt.x_pm1a_cnt_blk.register_bit_width);

		port = fadt.x_pm1a_cnt_blk.address;	
		width = fadt.x_pm1a_cnt_blk.register_bit_width;
	}

	ioperm(port, width/8, 1);
	switch (width) {
	case 8:
		value = inb(fadt.pm1a_cnt_blk);
		ioperm(port, width/8, 0);
		break;
	case 16:
		value = inw(fadt.pm1a_cnt_blk);
		ioperm(port, width/8, 0);
		break;
	case 32:
		value = inl(fadt.pm1a_cnt_blk);
		ioperm(port, width/8, 0);
		break;
	default:
		ioperm(port, width/8, 0);
		fwts_failed_high(fw, "FADT pm1a register has invalid bit width of %d.", width);
		return FWTS_OK;
	}

	if (value & 0x01)
		fwts_passed(fw, "SCI_EN bit in PM1a Control Register Block is enabled.");
	else
		fwts_failed_high(fw, "SCI_EN bit in PM1a Control Register Block is not enabled.");

	return FWTS_OK;
}

static fwts_framework_tests fadt_tests[] = {
	fadt_test1,
	NULL
};

static fwts_framework_ops fadt_ops = {
	fadt_headline,
	fadt_init,
	fadt_deinit,
	fadt_tests
};

FWTS_REGISTER(fadt, &fadt_ops, FWTS_TEST_ANYTIME, FWTS_BATCH);

#endif
