/*
 * Copyright (C) 2010 Canonical
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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>

#include "framework.h"
#include "iasl.h"

text_list *iasl_disassemble(log *log, framework *fw, char *table, int which)
{
	char tmpbuf[PATH_MAX+128];
	char tmpname[PATH_MAX];
	char cwd[PATH_MAX];
	int len;
	int fd;
	char *iasl = fw->iasl ? fw->iasl : IASL;
	char *data;
	pid_t pid;
	text_list *output;

	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		log_error(log, "Cannot get current working directory");
		return NULL;
	}

	if (chdir("/tmp") < 0) {
		log_error(log, "Cannot change directory to /tmp");
		return NULL;
	}

	snprintf(tmpbuf, sizeof(tmpbuf), "acpidump -b -t %s -s %d", table, which);
	fd = pipe_open(tmpbuf, &pid);
	if (fd < 0) {
		log_error(log, "exec of %s failed", tmpbuf);
		return NULL;
	}
	data = pipe_read(fd, &len);
	pipe_close(fd, pid);

	snprintf(tmpname, sizeof(tmpname), "tmp_iasl_%d_%s", getpid(), table);
	if ((fd = open(tmpname, O_WRONLY | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR)) < 0) {
		log_error(log, "Cannot create temporary file %s", tmpname);
		free(data);
		return NULL;
	}
	if (write(fd, data, len) != len) {
		log_error(log, "Cannot write all data to temporary file");
		free(data);
		close(fd);
		return NULL;
	}	
	free(data);
	close(fd);

	snprintf(tmpbuf, sizeof(tmpbuf), "%s -d %s", iasl, tmpname);
	fd = pipe_open(tmpbuf, &pid);
	if (fd < 0) {
		log_warning(log, "exec of %s -d %s (disassemble) failed\n", iasl, tmpname);
	}
	pipe_close(fd, pid);

	snprintf(tmpbuf, sizeof(tmpbuf), "%s.dsl", tmpname);
	output = file_open_and_read(tmpbuf);

	snprintf(tmpbuf,sizeof(tmpbuf),"%s.dsl", tmpname);
	unlink(tmpname);
	unlink(tmpbuf);

	if (chdir(cwd) < 0) {
		log_error(log, "Cannot change directory to %s", cwd);
	}

	return output;
}

text_list* iasl_reassemble(log *log, framework *fw, char *table, int which)
{
	char tmpbuf[PATH_MAX+128];
	char tmpname[PATH_MAX];
	char cwd[PATH_MAX];
	text_list *output;
	int ret;
	int len;
	int fd;
	char *iasl = fw->iasl ? fw->iasl : IASL;
	char *data;
	pid_t pid;

	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		log_error(log, "Cannot get current working directory");
		return NULL;
	}

	if (chdir("/tmp") < 0) {
		log_error(log, "Cannot change directory to /tmp");
		return NULL;
	}

	snprintf(tmpbuf, sizeof(tmpbuf), "acpidump -b -t %s -s %d", table, which);
	fd = pipe_open(tmpbuf, &pid);
	if (fd < 0) {
		log_error(log, "exec of %s failed", tmpbuf);
		return NULL;
	}
	data = pipe_read(fd, &len);
	pipe_close(fd, pid);

	snprintf(tmpname, sizeof(tmpname), "tmp_iasl_%d_%s", getpid(), table);
	if ((fd = open(tmpname, O_WRONLY | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR)) < 0) {
		log_error(log, "Cannot create temporary file");
		free(data);
		return NULL;
	}
	if (write(fd, data, len) != len) {
		log_error(log, "Cannot write all data to temporary file");
		free(data);
		close(fd);
		return NULL;
	}	
	free(data);
	close(fd);

	snprintf(tmpbuf, sizeof(tmpbuf), "%s -d %s", iasl, tmpname);
	ret = pipe_exec(tmpbuf, &output);
	if (ret)
		log_warning(log, "exec of %s -d %s (disassemble) returned %d\n", iasl, tmpname, ret);
	if (output)
		text_list_free(output);

	snprintf(tmpbuf, sizeof(tmpbuf), "%s %s.dsl", iasl, tmpname);
	ret = pipe_exec(tmpbuf, &output);
	if (ret)
		log_error(log, "exec of %s (assemble) returned %d\n", iasl, ret);

	snprintf(tmpbuf,sizeof(tmpbuf),"%s.dsl", tmpname);
	unlink(tmpname);
	unlink(tmpbuf);

	if (chdir(cwd) < 0) {
		log_error(log, "Cannot change directory to %s", cwd);
	}

	return output;
}
