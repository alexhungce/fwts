/*
 * Copyright (C) 2017 Canonical
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

#if defined(FWTS_HAS_ACPI)

#include "fwts_acpi_object_eval.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#define FWTS_ACPI_TIME_ALARM_HID "ACPI000E"

static ACPI_HANDLE device;

static uint32_t capability;

static ACPI_STATUS get_device_handle(ACPI_HANDLE handle, uint32_t level,
					  void *context, void **ret_val)
{
	FWTS_UNUSED(level);
	FWTS_UNUSED(context);
	FWTS_UNUSED(ret_val);

	device = handle;
	return AE_CTRL_TERMINATE;
}

static int acpi_time_alarm_init(fwts_framework *fw)
{
	ACPI_STATUS status;

	if (fwts_acpica_init(fw) != FWTS_OK)
		return FWTS_ERROR;

	status = AcpiGetDevices(FWTS_ACPI_TIME_ALARM_HID, get_device_handle, NULL, NULL);
	if (ACPI_FAILURE(status)) {
		fwts_log_error(fw, "Cannot find the ACPI device");
		return FWTS_ERROR;
	}

	if (!device) {
		fwts_log_error(fw, "ACPI time and alarm device does not exist, skipping test");
		fwts_acpica_deinit();
		return FWTS_SKIP;
	} else {
		ACPI_BUFFER buffer;
		char full_name[128];

		buffer.Length = sizeof(full_name);
		buffer.Pointer = full_name;

		status = AcpiGetName(device, ACPI_FULL_PATHNAME, &buffer);
		if (ACPI_SUCCESS(status)) {
			fwts_log_info_verbatim(fw, "ACPI Time and Alarm Device: %s", full_name);
			fwts_log_nl(fw);
		}
	}

	return FWTS_OK;
}

static void method_test_GCP_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_INTEGER) != FWTS_OK)
		return;

	capability = obj->Integer.Value;

	if (obj->Integer.Value & ~0x1ff)
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_GCPReturn",
			"%s returned %" PRId64 ", should be between 0 and 0x1ff, "
			"one or more of the reserved bits 9..31 seem "
			"to be set.",
			name, (uint64_t)obj->Integer.Value);
	else
		fwts_method_passed_sane_uint64(fw, name, obj->Integer.Value);
}

static int method_test_GCP(fwts_framework *fw)
{
	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_GCP", NULL, 0, method_test_GCP_return, NULL);
}

static void method_test_GRT_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_BUFFER) != FWTS_OK)
		return;

	if (obj->Buffer.Length != 16) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_GRTBadBufferSize",
			"%s should return a buffer of 16 bytes, but "
			"instead just returned %" PRIu32,
			name, obj->Buffer.Length);
		return;
	}

	/*
	 * Should sanity check this, but we can't read the
	 * the data in this emulated mode, so ignore
	 */
	fwts_method_passed_sane(fw, name, "buffer");
}

static int method_test_GRT(fwts_framework *fw)
{
	if (capability & 0x04)
		return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
			"_GRT", NULL, 0, method_test_GRT_return, NULL);
	else
		return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
			"_GRT", NULL, 0, method_test_GRT_return, NULL);
}

static void method_test_GWS_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_INTEGER) != FWTS_OK)
		return;

	if (obj->Integer.Value & ~0x3)
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_GWSReturn",
			"%s returned %" PRIu64 ", should be between 0 and 3, "
			"one or more of the reserved bits 2..31 seem "
			"to be set.",
			name, (uint64_t)obj->Integer.Value);
	else
		fwts_method_passed_sane_uint64(fw, name, obj->Integer.Value);
}

static int method_test_GWS(fwts_framework *fw)
{
	ACPI_OBJECT arg[1];

	arg[0].Type = ACPI_TYPE_INTEGER;
	arg[0].Integer.Value = 1;	/* DC timer */

	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_GWS", arg, 1, method_test_GWS_return, NULL);
}

static void method_test_CWS_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_INTEGER) != FWTS_OK)
		return;

	if (obj->Integer.Value != 0 && obj->Integer.Value != 1)
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_CWSInvalidInteger",
			"%s returned %" PRIu64 ", should be 0 or 1.",
			name, (uint64_t)obj->Integer.Value);
	else
		fwts_method_passed_sane_uint64(fw, name, obj->Integer.Value);
}

static int method_test_CWS(fwts_framework *fw)
{
	ACPI_OBJECT arg[1];
	int i, ret;
	arg[0].Type = ACPI_TYPE_INTEGER;

	for (i = 0; i < 2; i++) {
		arg[0].Integer.Value = i;
		ret = fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
			"_CWS", arg, 1, method_test_CWS_return, NULL);

		if (ret != FWTS_OK)
			break;
	}
	return ret;
}

static int method_test_STP(fwts_framework *fw)
{
	ACPI_OBJECT arg[2];

	arg[0].Type = ACPI_TYPE_INTEGER;
	arg[0].Integer.Value = 1;	/* DC timer */
	arg[1].Type = ACPI_TYPE_INTEGER;
	arg[1].Integer.Value = 0;	/* wake up instantly */

	return fwts_evaluate_method(fw, METHOD_OPTIONAL, &device,
		"_STP", arg, 2, fwts_method_test_passed_failed_return, NULL);
}

static int method_test_STV(fwts_framework *fw)
{
	ACPI_OBJECT arg[2];

	arg[0].Type = ACPI_TYPE_INTEGER;
	arg[0].Integer.Value = 1;	/* DC timer */
	arg[1].Type = ACPI_TYPE_INTEGER;
	arg[1].Integer.Value = 100;	/* timer value */

	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_STV", arg, 2, fwts_method_test_passed_failed_return, NULL);
}

static int method_test_TIP(fwts_framework *fw)
{
	ACPI_OBJECT arg[1];

	arg[0].Type = ACPI_TYPE_INTEGER;
	arg[0].Integer.Value = 1;	/* DC timer */

	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_TIP", arg, 1, fwts_method_test_integer_return, NULL);
}

static int method_test_TIV(fwts_framework *fw)
{
	ACPI_OBJECT arg[1];

	arg[0].Type = ACPI_TYPE_INTEGER;
	arg[0].Integer.Value = 1;	/* DC timer */

	return fwts_evaluate_method(fw, METHOD_MANDATORY, &device,
		"_TIV", arg, 1, fwts_method_test_integer_return, NULL);
}

static fwts_framework_minor_test acpi_time_alarm_tests[] = {
	{ method_test_GCP, "Test _GCP (Get Capabilities)." },
	{ method_test_GRT, "Test _GRT (Get Real Time)." },
	/* { method_test_SRT, "Test _SRT (Set Real Time)." }, */
	{ method_test_GWS, "Test _GWS (Get Wake Status)." },
	{ method_test_CWS, "Test _CWS (Clear Wake Status)." },
	{ method_test_STP, "Test _STP (Set Expired Timer Wake Policy)." },
	{ method_test_STV, "Test _STV (Set Timer Value)." },
	{ method_test_TIP, "Test _TIP (Expired Timer Wake Policy)." },
	{ method_test_TIV, "Test _TIV (Timer Values)." },
	{ NULL, NULL }
};

static int acpi_time_alarm_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);
	fwts_acpica_deinit();

	return FWTS_OK;
}

static fwts_framework_ops acpi_time_alarm_ops = {
	.description = "Time and alarm device test",
	.init        = acpi_time_alarm_init,
	.deinit      = acpi_time_alarm_deinit,
	.minor_tests = acpi_time_alarm_tests
};

FWTS_REGISTER("acpi_time", &acpi_time_alarm_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH | FWTS_FLAG_TEST_ACPI)

#endif
