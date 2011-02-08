/*
 * Copyright (C) 2010-2011 Canonical
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
#include <getopt.h>

#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>


static int  s3_multiple;	/* number of s3 multiple tests to run */
static int  s3_min_delay;	/* min time between resume and next suspend */
static int  s3_max_delay;	/* max time between resume and next suspend */
static float s3_delay_delta;	/* amount to add to delay between each S3 tests */

static char *s3_headline(void)
{
	return "S3 suspend/resume test.";
}

static int s3_init(fwts_framework *fw)
{
	int ret;

	/* Pre-init - make sure wakealarm works so that we can wake up after suspend */
	if (fwts_klog_clear()) {
		fwts_log_error(fw, "Cannot clear kernel log.");
		return FWTS_ERROR;
	}
	if ((ret = fwts_wakealarm_test_firing(fw, 1))) {
		fwts_log_error(fw, "Cannot automatically wake machine up - aborting S3 test.");
		fwts_failed(fw, "Check if wakealarm works reliably for S3 tests.");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

static int s3_deinit(fwts_framework *fw)
{
	return FWTS_OK;
}

static void s3_do_suspend_resume(fwts_framework *fw, int *errors, int delay, int *duration)
{
	fwts_list *output;
	int status;
	time_t t_start;
	time_t t_end;

	fwts_klog_clear();

	fwts_wakealarm_trigger(fw, delay);

	time(&t_start);

	/* Do S3 here */
	status = fwts_pipe_exec("pm-suspend", &output);

	time(&t_end);
	if (output)
		fwts_text_list_free(output);

	*duration = (int)(t_end - t_start);

	fwts_log_info(fw, "pm-suspend returned %d after %d seconds.", status, *duration);

	if ((t_end - t_start) < delay) {
		(*errors)++;
		fwts_failed_medium(fw, "Unexpected: S3 slept for less than %d seconds.", delay);
		fwts_tag_failed(fw, FWTS_TAG_POWER_MANAGEMENT);
	}
	if ((t_end - t_start) > delay*3) {
		int s3_C1E_enabled;
		(*errors)++;
		fwts_failed_high(fw, "Unexpected: S3 much longer than expected (%d seconds).", *duration);

		s3_C1E_enabled = fwts_cpu_has_c1e();
		if (s3_C1E_enabled == -1)
			fwts_log_error(fw, "Cannot read C1E bit\n");
		else if (s3_C1E_enabled == 1)
			fwts_advice(fw, "Detected AMD with C1E enabled. The AMD C1E idle wait can sometimes "
					"produce long delays on resume.  This is a known issue with the "
					"failed delivery of intettupts while in deep C states. "
					"If you have a BIOS option to disable C1E please disable this and retry. "
					"Alternatively, re-test with the kernel parameter \"idle=mwait\". ");
	}

	fwts_log_info(fw, "pm-suspend returned status: %d.", status);

	/* Add in error check for pm-suspend status */
	if ((status > 0) && (status < 128)) {
		(*errors)++;
		fwts_failed_medium(fw, "pm-action failed before trying to put the system "
				     "in the requested power saving state.");
		fwts_tag_failed(fw, FWTS_TAG_POWER_MANAGEMENT);
	} else if (status == 128) {
		(*errors)++;
		fwts_failed_medium(fw, "pm-action tried to put the machine in the requested "
       				     "power state but failed.");
		fwts_tag_failed(fw, FWTS_TAG_POWER_MANAGEMENT);
	} else if (status > 128) {
		(*errors)++;
		fwts_failed_medium(fw, "pm-action encountered an error and also failed to "
				     "enter the requested power saving state.");
		fwts_tag_failed(fw, FWTS_TAG_POWER_MANAGEMENT);
	}
}

static int s3_check_log(fwts_framework *fw)
{
	fwts_list *klog;
	int errors = 0;
	int oopses = 0;

	if ((klog = fwts_klog_read()) == NULL) {
		fwts_log_error(fw, "Cannot read kernel log.");
		fwts_failed(fw, "Unable to check kernel log for S3 suspend/resume test.");
		return FWTS_ERROR;
	}

	if (fwts_klog_pm_check(fw, NULL, klog, &errors))
		fwts_log_error(fw, "Error parsing kernel log.");

	if (fwts_klog_firmware_check(fw, NULL, klog, &errors))
		fwts_log_error(fw, "Error parsing kernel log.");

	if (fwts_klog_common_check(fw, NULL, klog, &errors))
		fwts_log_error(fw, "Error parsing kernel log.");

	if (fwts_oops_check(fw, klog, &oopses))
		fwts_log_error(fw, "Error parsing kernel log.");

	fwts_klog_free(klog);

	if (errors + oopses > 0)
		fwts_log_info(fw, "Found %d errors and %d oopses in kernel log.", errors, oopses);
	else
		fwts_passed(fw, "Found no errors in kernel log.");

	return FWTS_OK;
}

static int s3_test_single(fwts_framework *fw)
{	
	int errors = 0;
	int duration;

	s3_do_suspend_resume(fw, &errors, 30, &duration);
	if (errors > 0)
		fwts_log_info(fw, "Found %d errors doing suspend/resume.", errors);
	else
		fwts_passed(fw, "Found no errors doing suspend/resume.");

	return s3_check_log(fw);
}

static int s3_test_multiple(fwts_framework *fw)
{	
	int errors = 0;
	int delay = 30;
	int duration = 0;
	int i;
	int awake_delay = s3_min_delay * 1000;
	int delta = (int)(s3_delay_delta * 1000.0);

	if (s3_multiple == 0) {
		s3_multiple = 2;
		fwts_log_info(fw, "Defaulted to run 2 multiple tests, run --s3-multiple=N to run more S3 cycles\n");
	}

	for (i=0; i<s3_multiple; i++) {
		int timetaken;
		struct timeval tv;

		fwts_log_info(fw, "S3 cycle %d of %d\n",i+1,s3_multiple);
		fwts_progress(fw, ((i+1) * 100) / s3_multiple);
		s3_do_suspend_resume(fw, &errors, delay, &duration);

		timetaken = duration - delay;
		delay = timetaken + 10;		/* Shorten test time, plus some slack */

		tv.tv_sec  = awake_delay / 1000;
		tv.tv_usec = (awake_delay % 1000)*1000;

		select(0, NULL, NULL, NULL, &tv);
		
		awake_delay += delta;
		if (awake_delay > (s3_max_delay * 1000))
			awake_delay = s3_min_delay * 1000;

	}

	if (errors > 0)
		fwts_log_info(fw, "Found %d errors doing suspend/resume.", errors);
	else
		fwts_passed(fw, "Found no errors doing suspend/resume.");

	return s3_check_log(fw);
}

static int s3_options_handler(fwts_framework *fw, int argc, char * const argv[], int option_char, int long_index)
{
	s3_multiple = 0;
	s3_min_delay = 0;
        s3_max_delay = 30;
        s3_delay_delta = 0.5;

	printf("In S3 handler\n");

        switch (option_char) {
        case 0:
                switch (long_index) {
		case 0:
			s3_multiple = atoi(optarg);	
			printf("HERE! multiple = %d\n", s3_multiple);
			break;
		case 1:
			s3_min_delay = atoi(optarg);
			break;
		case 2:
			s3_max_delay = atoi(optarg);
			break;
		case 3:
			s3_delay_delta = atof(optarg);
			break;
		}
	}

	if ((s3_min_delay < 0) || (s3_min_delay > 3600)) {
		fprintf(stderr, "--s3-min-delay cannot be less than zero or more than 1 hour!\n");
		return FWTS_ERROR;
	}
	if (s3_max_delay < s3_min_delay || s3_max_delay > 3600)  {
		fprintf(stderr, "--s3-max-delay cannot be less than --s3-min-delay or more than 1 hour!\n");
		return FWTS_ERROR;
	}
	if (s3_delay_delta <= 0.001) {
		fprintf(stderr, "--s3-delay-delta cannot be less than 0.001\n");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

static fwts_option s3_options[] = {
	{ "s3-multiple", 	"", 1, "Time to be added to delay between S3 iterations." },
	{ "s3-min-delay", 	"", 1, "Minimum time between S3 iterations." },
	{ "s3-max-delay", 	"", 1, "Maximum time between S3 iterations." },
	{ "s3-delay-delta", 	"", 1, "Run S3 tests N times." },
	{ NULL, NULL, 0, NULL }
};

static fwts_framework_minor_test s3_tests[] = {
	{ s3_test_single,   "S3 suspend/resume test (single run)." },
	{ s3_test_multiple, "S3 suspend/resume test (multiple runs)." },
	{ NULL, NULL }
};

static fwts_framework_ops s3_ops = {
	.headline    = s3_headline,
	.init        = s3_init,	
	.deinit      = s3_deinit,
	.minor_tests = s3_tests,
	.options     = s3_options,
	.options_handler = s3_options_handler,
};

FWTS_REGISTER(s3, &s3_ops, FWTS_TEST_LATE, FWTS_POWER_STATES);

#endif
