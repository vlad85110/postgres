#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <getopt.h>

static char* PYTHON_PATH_REL = "/PATH/TO/python3";
static char* STAND_CLIENT_SCRIPT_REL = "/PATH/TO/stand_client.py";
static char* STAND_CLIENT_SQL_FILE_REL = "/PATH/TO/sql-tests/test_wal_load.sql";
static char* LIVE_GRAPH_SCRIPT_REL = "/PATH/TO/live_write_delay.py";
static char* LIVE_DELAY_WINDOW = "100";
static char* LIVE_DELAY_INTERVAL = "300";

static char* SIGNAL_PID = NULL;
static char* SIGNAL_NUM = "12";
static char* FIXED_DELAY_START_QUERY = NULL;
static char* SIGNAL_REPEAT_EVERY = NULL;

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
	if (relative[0] == '/') {
		if (snprintf(out, out_size, "%s", relative) >= (int) out_size) {
			fprintf(stderr, "Error: path too long: %s\n", relative);
			return -1;
		}
		return 0;
	}

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

static void print_usage(const char *prog_name) {
	fprintf(stderr,
		"Usage: %s [OPTIONS]\n"
		"\n"
		"Paths:\n"
		"  --python-path PATH            path to python3 interpreter\n"
		"  --stand-client-script PATH    path to stand_client.py\n"
		"  --sql-file PATH               path to the SQL file with queries\n"
		"  --live-graph-script PATH      path to live_write_delay.py\n"
		"\n"
		"live_write_delay.py options:\n"
		"  --window N                    number of most recent queries to display\n"
		"  --interval N                  chart refresh interval in milliseconds\n"
		"\n"
		"stand_client.py options:\n"
		"  --signal-pid PID              PID of the process to signal during the run\n"
		"  --signal-num N                signal number to send (default: 12 / SIGUSR2)\n"
		"  --fixed-delay-start-query N   query number at which the first SIGUSR2 is sent\n"
		"  --signal-repeat-every N       resend SIGUSR2 every N queries after the first send\n"
		"\n"
		"  -h, --help                    show this help message\n",
		prog_name
	);
}

int main(int argc, char *argv[]) {
	enum {
		OPT_PYTHON_PATH = 1000,
		OPT_STAND_CLIENT_SCRIPT,
		OPT_SQL_FILE,
		OPT_LIVE_GRAPH_SCRIPT,
		OPT_WINDOW,
		OPT_INTERVAL,
		OPT_SIGNAL_PID,
		OPT_SIGNAL_NUM,
		OPT_FIXED_DELAY_START_QUERY,
		OPT_SIGNAL_REPEAT_EVERY
	};

	static struct option long_options[] = {
		{"python-path",             required_argument, 0, OPT_PYTHON_PATH},
		{"stand-client-script",     required_argument, 0, OPT_STAND_CLIENT_SCRIPT},
		{"sql-file",                required_argument, 0, OPT_SQL_FILE},
		{"live-graph-script",       required_argument, 0, OPT_LIVE_GRAPH_SCRIPT},
		{"window",                  required_argument, 0, OPT_WINDOW},
		{"interval",                required_argument, 0, OPT_INTERVAL},
		{"signal-pid",              required_argument, 0, OPT_SIGNAL_PID},
		{"signal-num",              required_argument, 0, OPT_SIGNAL_NUM},
		{"fixed-delay-start-query", required_argument, 0, OPT_FIXED_DELAY_START_QUERY},
		{"signal-repeat-every",     required_argument, 0, OPT_SIGNAL_REPEAT_EVERY},
		{"help",                    no_argument,       0, 'h'},
		{0, 0, 0, 0}
	};

	int opt;
	int option_index = 0;

	while ((opt = getopt_long(argc, argv, "h", long_options, &option_index)) != -1) {
		switch (opt) {
			case OPT_PYTHON_PATH:
				PYTHON_PATH_REL = optarg;
				break;
			case OPT_STAND_CLIENT_SCRIPT:
				STAND_CLIENT_SCRIPT_REL = optarg;
				break;
			case OPT_SQL_FILE:
				STAND_CLIENT_SQL_FILE_REL = optarg;
				break;
			case OPT_LIVE_GRAPH_SCRIPT:
				LIVE_GRAPH_SCRIPT_REL = optarg;
				break;
			case OPT_WINDOW:
				LIVE_DELAY_WINDOW = optarg;
				break;
			case OPT_INTERVAL:
				LIVE_DELAY_INTERVAL = optarg;
				break;
			case OPT_SIGNAL_PID:
				SIGNAL_PID = optarg;
				break;
			case OPT_SIGNAL_NUM:
				SIGNAL_NUM = optarg;
				break;
			case OPT_FIXED_DELAY_START_QUERY:
				FIXED_DELAY_START_QUERY = optarg;
				break;
			case OPT_SIGNAL_REPEAT_EVERY:
				SIGNAL_REPEAT_EVERY = optarg;
				break;
			case 'h':
				print_usage(argv[0]);
				return 0;
			default:
				print_usage(argv[0]);
				return 1;
		}
	}

	if (SIGNAL_PID == NULL) {
		fprintf(stderr, "Error: --signal-pid is required\n");
		print_usage(argv[0]);
		return 1;
	}

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

		if (FIXED_DELAY_START_QUERY != NULL && SIGNAL_REPEAT_EVERY != NULL) {
			execl(
				PYTHON_PATH,
				"python3",
				STAND_CLIENT_SCRIPT,
				STAND_CLIENT_SQL_FILE,
				"--signal-pid",
				SIGNAL_PID,
				"--signal-num",
				SIGNAL_NUM,
				"--fixed-delay-start-query",
				FIXED_DELAY_START_QUERY,
				"--signal-repeat-every",
				SIGNAL_REPEAT_EVERY,
				(char *) NULL
			);
		} else if (FIXED_DELAY_START_QUERY != NULL) {
			execl(
				PYTHON_PATH,
				"python3",
				STAND_CLIENT_SCRIPT,
				STAND_CLIENT_SQL_FILE,
				"--signal-pid",
				SIGNAL_PID,
				"--signal-num",
				SIGNAL_NUM,
				"--fixed-delay-start-query",
				FIXED_DELAY_START_QUERY,
				(char *) NULL
			);
		} else {
			execl(
				PYTHON_PATH,
				"python3",
				STAND_CLIENT_SCRIPT,
				STAND_CLIENT_SQL_FILE,
				"--signal-pid",
				SIGNAL_PID,
				"--signal-num",
				SIGNAL_NUM,
				(char *) NULL
			);
		}

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
			LIVE_DELAY_WINDOW,
			"--interval",
			LIVE_DELAY_INTERVAL,
			(char *) NULL
		);

		printf("Error: execl live_write_delay.py");
		return 1;
	}
}