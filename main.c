#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* Show optional timings (as well as required ones) */
#define CONFIG_OPT_TIMINGS
/* All timings should be specified in file */
#define CONFIG_FILE_ALL_TIM
/* Multiply timings by 2 if double latency bit is set */
//#define CONFIG_MUL_DOUBLE_LATENCY

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))
/* Generate bit mask: bits from "a" to "b" are set to "1" (b > a) */
#define BIT_MASK(a, b)	(((unsigned)-1 >> (31 - (b))) & ~((1U << (a)) - 1))

enum config_idx {
	CONFIG1 = 0,
	CONFIG2 = 1,
	CONFIG3 = 2,
	CONFIG4 = 3,
	CONFIG5 = 4,
	CONFIG6 = 5,
	CONFIG_NUM
};

struct parse_info {
	uint32_t val;		/* field value */
	const char desc[50];	/* field description */
};

struct timing {
	char name[30];
	enum config_idx config_reg;
	uint8_t left_bit;
	uint8_t right_bit;
	bool is_timing;		/* this field is actual timing (in ticks) */
	bool is_paragran;	/* timings multiplies by 2 if paragran is set */
	size_t pi_len;		/* size of "pi" array */
	struct parse_info *pi;	/* data for parsing this field; NULL if none */
	uint32_t value;		/* ticks or some value (if not timing) */
};

static struct timing timings[] = {
	{ "wrap-burst", 		CONFIG1, 31, 31, false, false,
		2, (struct parse_info []) {
			{ 0x0, "Sync. burst not supported" },
			{ 0x1, "Sync. burst supported" },
		},
	},
	{ "read-multiple",		CONFIG1, 30, 30, false, false,
		2, (struct parse_info []) {
			{ 0x0, "Single access" },
			{ 0x1, "Multiple access" },
		},
	},
	{ "read-type",			CONFIG1, 29, 29, false, false,
		2, (struct parse_info []) {
			{ 0x0, "Read async." },
			{ 0x1, "Read sync." }
		},
	},
	{ "write-multiple",		CONFIG1, 28, 28, false, false,
		2, (struct parse_info []) {
			{ 0x0, "Single access" },
			{ 0x1, "Multiple access" },
		},
	},
	{ "write-type",			CONFIG1, 27, 27, false, false,
		2, (struct parse_info []) {
			{ 0x0, "Write async." },
			{ 0x1, "Write sync." },
		},
	},
	{ "clk-activation",		CONFIG1, 26, 25, false, false,
		3, (struct parse_info []) {
			{ 0x0, "1st edge at start access time" },
			{ 0x1, "... +one cycle after"  },
			{ 0x2, "... +two cycles after" },
		},
	},
	{ "attached-dev-page-len",	CONFIG1, 24, 23, false, false,
		3, (struct parse_info []) {
			{ 0x0, "4 words" },
			{ 0x1, "8 words" },
			{ 0x2, "16 words" },
		},
	},
	{ "wait-read-mon",		CONFIG1, 22, 22, false, false,
		2, (struct parse_info []) {
			{ 0x0, "Wait pin is not monitored for read" },
			{ 0x1, "Wait pin is monitored for read" },
		},
	},
	{ "wait-write-mon",		CONFIG1, 21, 21, false, false,
		2, (struct parse_info []) {
			{ 0x0, "Wait pin is not monitored for write" },
			{ 0x1, "Wait pin is monitored for write" },
		},
	},
	{ "wait-mon",			CONFIG1, 19, 18, false, false,
		3, (struct parse_info []) {
			{ 0x0, "Wait pin is monitored with valid data" },
			{ 0x1, "Wait pin is monitored one cycle" },
			{ 0x2, "Wait pin is monitored two cycles" },
		},
	},
	{ "wait-pin-select",		CONFIG1, 17, 16, false, false,
		2, (struct parse_info []) {
			{ 0x0, "Wait input pin is WAIT0" },
			{ 0x1, "Wait input pin is WAIT1" },
		},
	},
	{ "device-size",		CONFIG1, 13, 12, false, false,
		2, (struct parse_info[]) {
			{ 0x0, "8 bit" },
			{ 0x1, "16 bit" },
		},
	},
	{ "device-type",		CONFIG1, 11, 10, false, false,
		2, (struct parse_info[]) {
			{ 0x0, "NOR flash-like" },
			{ 0x2, "NAND flash-like" },
		},
	},
	{ "mux-add-data",		CONFIG1, 9,  8,  false, false,
		3, (struct parse_info[]) {
			{ 0x0, "Nonmultiplexed device" },
			{ 0x1, "AAD-multiplexed protocol device" },
			{ 0x2, "Address and data multiplexed device" },
		},
	},
	{ "time-para-gran",		CONFIG1, 4,  4,  false, false,
		2, (struct parse_info[]) {
			{ 0x0, "x1 latencies" },
			{ 0x1, "x2 latencies" },
		},
	},
	{ "gpmc-fclk-divider",		CONFIG1, 1,  0,  false, false,
		4, (struct parse_info[]) {
			{ 0x0, "GPMC_CLK = GPMC_FCLK" },
			{ 0x1, "GPMC_CLK = GPMC_FCLK / 2" },
			{ 0x2, "GPMC_CLK = GPMC_FCLK / 3" },
			{ 0x3, "GPMC_CLK = GPMC_FCLK / 4" },
		},
	},
	{ "cs-wr-off",			CONFIG2, 20, 16, true,  true,  },
	{ "cs-rd-off",			CONFIG2, 12, 8,  true,  true,  },
	{ "cs-extra-delay",		CONFIG2, 7,  7,  false, false,
		2, (struct parse_info[]) {
			{ 0x0, "CS i timing control signal is not delayed" },
			{ 0x1, "CS i timing control signal is delayed" },
		},
	},
	{ "cs-on",			CONFIG2, 3,  0,  true,  true,  },
	{ "adv-aad-mux-wr-off",		CONFIG3, 30, 28, true,  false, },
	{ "adv-aad-mux-rd-off",		CONFIG3, 26, 24, true,  false, },
	{ "adv-wr-off",			CONFIG3, 20, 16, true,  true,  },
	{ "adv-rd-off",			CONFIG3, 12, 8,  true,  true,  },
	{ "adv-extra-delay",		CONFIG3, 7,  7,  false, false,
		2, (struct parse_info[]) {
			{ 0x0, "nADV timing control signal is not delayed" },
			{ 0x1, "nADC timing control signal is delayed" },
		},
	},
	{ "adv-aad-mux-on",		CONFIG3, 6,  4,  true,  false, },
	{ "adv-on",			CONFIG3, 3,  0,  true,  true,  },
	{ "we-off",			CONFIG4, 28, 24, true,  true,  },
	{ "we-extra-delay",		CONFIG4, 23, 23, false, false,
		2, (struct parse_info[]) {
			{ 0x0, "nWE timing control signal is not delayed" },
			{ 0x1, "nWE timing control signal is delayed" },
		},
	},
	{ "we-on",			CONFIG4, 19, 16, true,  true,  },
	{ "oe-aad-mux-off",		CONFIG4, 15, 13, true,  false, },
	{ "oe-off",			CONFIG4, 12, 8,  true,  true,  },
	{ "oe-extra-delay",		CONFIG4, 7,  7,  false, false,
		2, (struct parse_info[]) {
			{ 0x0, "nOE timing control signal is not delayed" },
			{ 0x1, "nOE timing control signal is delayed" },
		},
	},
	{ "oe-aad-mux-on",		CONFIG4, 6,  4,  true,  false, },
	{ "oe-on",			CONFIG4, 3,  0,  true,  true,  },
	{ "page-burst-access",		CONFIG5, 27, 24, true,  true,  },
	{ "rd-access",			CONFIG5, 20, 16, true,  true,  },
	{ "wr-cycle",			CONFIG5, 12, 8,  true,  true,  },
	{ "rd-cycle",			CONFIG5, 4,  0,  true,  true,  },
	{ "wr-access",			CONFIG6, 28, 24, true,  true,  },
	{ "wr-data-on-ad-mux-bus",	CONFIG6, 19, 16, false, true,  },
	{ "cycle2cycle-delay",		CONFIG6, 11, 8,  true,  true,  },
	{ "cycle2cycle-same-cs-en",	CONFIG6, 7,  7,  false, false,
		2, (struct parse_info[]) {
			{ 0x0, "No delay between the two accesses" },
			{ 0x1, "Add CYCLE2CYCLEDELAY" },
		},
	},
	{ "cycle2cycle-diff-cs-en",	CONFIG6, 6,  6,  false, false,
		2, (struct parse_info[]) {
			{ 0x0, "No delay between the two accesses" },
			{ 0x1, "Add CYCLE2CYCLEDELAY" },
		},
	},
	{ "bus-turnaround",		CONFIG6, 3,  0,  true,  true,  },
};

static const size_t para_gran_index = 14;

/* Indexes of required timings in "timings" array (see device tree bindings) */
static const size_t req_timings[] = {
	18,	/* cs-extra-delay */
	19,	/* cs-on */
	17,	/* cs-rd-off */
	16,	/* cs-wr-off */
	34,	/* oe-on */
	31,	/* oe-off */
	29,	/* we-on */
	27,	/* we-off */
	36,	/* access (rd-access) */
	38,	/* rd-cycle */
	37,	/* wr-cycle */
	39,	/* wr-access */
};

/* -------------------------------------------------------------------------- */

static void parse_timings(uint32_t *config_regs)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(timings); ++i) {
		struct timing *t = &timings[i];
		int reg_val = config_regs[t->config_reg];
		uint8_t l = t->left_bit;
		uint8_t r = t->right_bit;

		t->value = (reg_val >> r) & BIT_MASK(0, l - r);
	}
}

#ifdef CONFIG_MUL_DOUBLE_LATENCY
static void process_paragran(void)
{
	uint32_t para_gran;
	size_t i;

	para_gran = timings[para_gran_index].value;
	if (para_gran == 0)
		return;

	for (i = 0; i < ARRAY_SIZE(timings); ++i) {
		if (timings[i].is_paragran && timings[i].is_timing)
			timings[i].value *= 2;
	}
}
#else
static void process_paragran(void)
{
}
#endif

static void print_timing(size_t ti)
{
	struct timing *t = &timings[ti];

	if (t->is_timing) {
		printf("%-25s = %d\tticks\n", t->name, t->value);
	} else if (t->pi_len > 0) {
		size_t i;

		for (i = 0; i < t->pi_len; ++i) {
			if (t->value == t->pi[i].val) {
				printf("%-25s = 0x%x (%s)\n", t->name,
						t->value, t->pi[i].desc);
				return;
			}
		}
		printf("%-25s = 0x%x (!!! NOT PARSED, WRONG VAL?)\n", t->name,
				t->value);
	} else {
		printf("%-25s = 0x%x\n", t->name, t->value);
	}
}

static void print_req_timings(void)
{
	size_t i;

	printf("*** Required timings ***\n");
	for (i = 0; i < ARRAY_SIZE(req_timings); ++i)
		print_timing(req_timings[i]);
}

#ifdef CONFIG_OPT_TIMINGS
static void print_opt_timings(void)
{
	size_t *opt_timings;		/* optional timings array */
	size_t opt_timings_count;	/* opt_timings elements count */
	size_t pos = 0;			/* position in opt_timings */
	size_t i;

	/* Calculate optional timings count */
	opt_timings_count = ARRAY_SIZE(timings) - ARRAY_SIZE(req_timings);

	/* Allocate optional timings array */
	opt_timings = calloc(opt_timings_count, sizeof(*opt_timings));
	if (opt_timings == NULL) {
		fprintf(stderr, "Unable to allocate opt_timings array\n");
		exit(EXIT_FAILURE);
	}

	/* Populate optional timings array */
	for (i = 0; i < ARRAY_SIZE(timings); ++i) {
		int j;
		bool is_opt;

		is_opt = true;
		for (j = 0; j < ARRAY_SIZE(req_timings); ++j) {
			if (i == req_timings[j]) {
				is_opt = false;
				break;
			}
		}

		if (is_opt) {
			opt_timings[pos] = i;
			++pos;
		}
	}

	/* Print optional timings array */
	printf("\n*** Optional timings ***\n");
	for (i = 0; i < opt_timings_count; ++i)
		print_timing(opt_timings[i]);

	free(opt_timings);
}
#else
static void print_opt_timings(void)
{
}
#endif

/* -------------------------------------------------------------------------- */

static int read_timings_file(const char *fname)
{
	FILE *file;
	char tim[30];
	uint32_t val;
	int ret = 0;
	size_t i;
#ifdef CONFIG_FILE_ALL_TIM
	bool *tim_read;
	bool tim_read_ok = true;
#endif

	file = fopen(fname, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: Unable to open file \"%s\"", fname);
		return -1;
	}

#ifdef CONFIG_FILE_ALL_TIM
	tim_read = calloc(ARRAY_SIZE(timings), sizeof(*tim_read));
	if (tim_read == NULL) {
		fprintf(stderr, "Unable to allocate memory for tim_read\n");
		ret = -2;
		goto out1;
	}

	memset(tim_read, false, ARRAY_SIZE(timings) * sizeof(*tim_read));
#endif

	while (fscanf(file, "%[^= ]%*[= ]%i\n", tim, &val) != EOF) {
		bool found = false;

		for (i = 0; i < ARRAY_SIZE(timings); ++i) {
			if (strcmp(timings[i].name, tim) == 0) {
				timings[i].value = val;
				found = true;
#ifdef CONFIG_FILE_ALL_TIM
				tim_read[i] = true;
#endif
				break;
			}
		}

		if (!found) {
			fprintf(stderr, "Wrong timing: \"%s\"\n", tim);
			ret = -3;
			goto out2;
		}
	}

#ifdef CONFIG_FILE_ALL_TIM
	for (i = 0; i < ARRAY_SIZE(timings); ++i) {
		if (!tim_read[i]) {
			fprintf(stderr, "File misses timing: \"%s\"\n",
					timings[i].name);
			tim_read_ok = false;
		}
	}

	if (!tim_read_ok) {
		ret = -4;
		goto out2;
	}
#endif

out2:
#ifdef CONFIG_FILE_ALL_TIM
	free(tim_read);
#endif
out1:
	fclose(file);
	return ret;
}

static void setup_reserved_bits(uint32_t *config_regs)
{
	config_regs[CONFIG6] |= 0x1 << 31;
}

static int populate_config_regs(uint32_t *config_regs)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(timings); ++i) {
		struct timing *t = &timings[i];
		uint32_t val;
		uint32_t mask;

		val = t->value << t->right_bit;
		mask = BIT_MASK(t->right_bit, t->left_bit);
		if (val & ~mask) {
			fprintf(stderr,
				"Error: timing \"%s\" has invalid value\n",
				t->name);

			return -1;
		}

		config_regs[t->config_reg] |= val;
	}

	return 0;
}

/* -------------------------------------------------------------------------- */

static void print_usage(char *app)
{
	printf("Usage:\n");
	printf("  %s -p CONFIG1..CONFIG6\n", app);
	printf("  %s -y <file>\n\n", app);
	printf("-y file       - yield registers values from file\n");
	printf("-p registers  - parse and print registers values\n");
}

static int do_parse_registers(int argc, char *argv[])
{
	uint32_t config_regs[CONFIG_NUM] = { 0 };
	size_t i;

	if (argc != CONFIG_NUM + 2) {
		fprintf(stderr, "Wrong arguments count\n");
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	/* Parse command line */
	for (i = 0; i < CONFIG_NUM; ++i)
		sscanf(argv[i+2], "%x", &config_regs[i]);

	parse_timings(config_regs);
	process_paragran();
	print_req_timings();
	print_opt_timings();

	return EXIT_SUCCESS;
}

static int do_yield_registers(int argc, char *argv[])
{
	uint32_t config_regs[CONFIG_NUM] = { 0 };
	char *fname;
	int res;
	size_t i;

	if (argc != 1 + 2) {
		fprintf(stderr, "Wrong arguments count\n");
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	fname = argv[2];

	if (access(fname, F_OK) == -1) {
		fprintf(stderr, "File \"%s\" doesn't exist\n", fname);
		perror("Error");
		return EXIT_FAILURE;
	}

	res = read_timings_file(fname);
	if (res != 0) {
		fprintf(stderr, "Error when reading file: %d\n", res);
		return EXIT_FAILURE;
	}

	res = populate_config_regs(config_regs);
	if (res != 0) {
		fprintf(stderr, "Error when populating regs: %d\n",
				res);
		return EXIT_FAILURE;
	}

	setup_reserved_bits(config_regs);

	for (i = 0; i < CONFIG_NUM; ++i)
		printf("CONFIG%zu = 0x%08x\n", i+1, config_regs[i]);

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	if (argc < 2+1) {
		fprintf(stderr, "Wrong arguments count\n");
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "-p") == 0) {
		return do_parse_registers(argc, argv);
	} else if (strcmp(argv[1], "-y") == 0) {
		return do_yield_registers(argc, argv);
	} else {
		fprintf(stderr, "Wrong arguments\n");
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
