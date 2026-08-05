#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>

char* PYTHON_PATH_REL = "/PATH/TO/python3";
char* STAND_CLIENT_SCRIPT_REL = "/PATH/TO/stand_client.py";
char* STAND_CLIENT_SQL_FILE_REL = "/PATH/TO/sql-tests/test_wal_load.sql";
char* LIVE_GRAPH_SCRIPT_REL = "/PATH/TO/live_write_delay.py";
char* LIVE_DELAY_WINDOW = "100";
char* LIVE_DELAY_INTERVAL = "300";

char base_dir[PATH_MAX];
char PYTHON_PATH[PATH_MAX];
char STAND_CLIENT_SCRIPT[PATH_MAX];
char STAND_CLIENT_SQL_FILE[PATH_MAX];
char LIVE_GRAPH_SCRIPT[PATH_MAX];

void change_input_output(int is_parent, int pipefds[]) {
	if (is_parent) {
		dup2(pipefds[1], 1);
	} else {
		dup2(pipefds[0], 0);
	}
}

int get_base_dir(char *out, size_t out_size) {
	char exe_path[PATH_MAX];
	ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);

	if (len == -1) {
		perror("readlink /proc/self/exe");
		return -1;
	}

	exe_path[len] = '\0';

	char *dir = dirname(exe_path);
	if (snprintf(out, out_size, "%s", dir) >= (int) out_size) {
		fprintf(stderr, "Error: base_dir path too long\n");
		return -1;
	}

	return 0;
}

int build_path(char *out, size_t out_size, const char *base, const char *relative) {
	if (snprintf(out, out_size, "%s/%s", base, relative) >= (int) out_size) {
		fprintf(stderr, "Error: path too long: %s/%s\n", base, relative);
		return -1;
	}
	return 0;
}

int check_file_exists(const char *path) {
	if (access(path, F_OK) != 0) {
		fprintf(stderr, "Error: file not found: %s\n", path);
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[]) {
	if (get_base_dir(base_dir, sizeof(base_dir)) == -1) {
		return 1;
	}

	if (build_path(PYTHON_PATH, sizeof(PYTHON_PATH), base_dir, PYTHON_PATH_REL) == -1
		|| build_path(STAND_CLIENT_SCRIPT, sizeof(STAND_CLIENT_SCRIPT), base_dir, STAND_CLIENT_SCRIPT_REL) == -1
		|| build_path(STAND_CLIENT_SQL_FILE, sizeof(STAND_CLIENT_SQL_FILE), base_dir, STAND_CLIENT_SQL_FILE_REL) == -1
		|| build_path(LIVE_GRAPH_SCRIPT, sizeof(LIVE_GRAPH_SCRIPT), base_dir, LIVE_GRAPH_SCRIPT_REL) == -1) {
		return 1;
	}

	if (check_file_exists(PYTHON_PATH) == -1
		|| check_file_exists(STAND_CLIENT_SCRIPT) == -1
		|| check_file_exists(STAND_CLIENT_SQL_FILE) == -1
		|| check_file_exists(LIVE_GRAPH_SCRIPT) == -1) {
		return 1;
	}

	int pipefds[2];
	if (pipe(pipefds) == -1) {
		perror("pipe");
		return 1;
	}

	int pid = fork();
	if (pid == -1) {
		perror("fork");
		return 1;
	}

	if (pid > 0) {
		/* --- РОДИТЕЛЬ (Писатель) --- */
		close(pipefds[0]);
		change_input_output(1, pipefds);
		close(pipefds[1]);

		execl(
			PYTHON_PATH,
			"python3",
			STAND_CLIENT_SCRIPT,
			STAND_CLIENT_SQL_FILE,
			(char *) NULL
		);

		printf("Error: execl stand_client.py");
		return 1;
	} else {
		close(pipefds[1]);
		change_input_output(0, pipefds);
		close(pipefds[0]);

		execl(
			PYTHON_PATH,
			"python3",
			LIVE_GRAPH_SCRIPT,
			"--window",
			strtol(LIVE_DELAY_WINDOW),
			"--interval",
			LIVE_DELAY_INTERVAL,
			(char *) NULL
		);

		printf("Error: execl live_write_delay.py");
		return 1;
	}
}