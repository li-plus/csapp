#include "cachelab.h"
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

void usage(const char *name) {
  printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n"
         "Options:\n"
         "  -h         Print this help message.\n"
         "  -v         Optional verbose flag.\n"
         "  -s <num>   Number of set index bits.\n"
         "  -E <num>   Number of lines per set.\n"
         "  -b <num>   Number of block offset bits.\n"
         "  -t <file>  Trace file.\n"
         "\n"
         "Examples:\n"
         "  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n"
         "  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n",
         name, name, name);
}

typedef struct {
  int s;
  int E;
  int b;
  char *t;
  int v;
} args_t;

args_t parse_args(int argc, char *argv[]) {
  args_t args;
  args.s = args.E = args.b = -1;
  args.t = NULL;
  args.v = 0;

  int opt;
  while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
    switch (opt) {
    case 'h':
      usage(argv[0]);
      exit(EXIT_SUCCESS);
    case 's':
      args.s = atoi(optarg);
      break;
    case 'E':
      args.E = atoi(optarg);
      break;
    case 'b':
      args.b = atoi(optarg);
      break;
    case 't':
      args.t = optarg;
      break;
    case 'v':
      args.v = 1;
      break;
    default:
      usage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }
  if (args.s < 0 || args.E <= 0 || args.b < 0 || args.t == NULL) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }
  return args;
}

typedef struct {
  int valid;
  int clk; // lru timestamp
  uint64_t tag;
} line_t;

typedef struct { line_t *lines; } set_t;

typedef struct {
  int s;
  int E;
  int b;
  set_t *sets;
} cache_t;

void cache_init(cache_t *cache, int s, int E, int b) {
  cache->s = s;
  cache->E = E;
  cache->b = b;
  int S = 1 << s;

  cache->sets = malloc(S * sizeof(set_t));
  if (cache->sets == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < S; i++) {
    set_t *set = &cache->sets[i];
    set->lines = malloc(E * sizeof(line_t));
    if (set->lines == NULL) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    for (int j = 0; j < E; j++) {
      line_t *line = &set->lines[j];
      line->valid = 0;
    }
  }
}

void cache_destroy(cache_t *cache) {
  for (int i = 0; i < (1 << cache->s); i++) {
    free(cache->sets[i].lines);
  }
  free(cache->sets);
}

#define CACHE_MISS 1
#define CACHE_EVICT 2

int cache_access(cache_t *cache, uint64_t addr, int clk) {
  uint64_t index = (addr >> cache->b) & ((1ull << cache->s) - 1);
  uint64_t tag = addr >> (cache->b + cache->s);
  set_t *set = &cache->sets[index];

  for (int i = 0; i < cache->E; i++) {
    line_t *line = &set->lines[i];
    if (line->valid && line->tag == tag) {
      // cache hit
      line->clk = clk;
      return 0;
    }
  }

  // cache miss: search for an empty line
  for (int i = 0; i < cache->E; i++) {
    line_t *line = &set->lines[i];
    if (!line->valid) {
      line->valid = 1;
      line->clk = clk;
      line->tag = tag;
      return CACHE_MISS;
    }
  }

  // do eviction
  line_t *victim_line = &set->lines[0];
  for (int i = 1; i < cache->E; i++) {
    line_t *line = &set->lines[i];
    if (line->clk < victim_line->clk) {
      victim_line = line;
    }
  }
  victim_line->tag = tag;
  victim_line->clk = clk;
  return CACHE_MISS | CACHE_EVICT;
}

int main(int argc, char *argv[]) {
  args_t args = parse_args(argc, argv);

#define printf_v(...)                                                          \
  do {                                                                         \
    if (args.v) {                                                              \
      printf(__VA_ARGS__);                                                     \
    }                                                                          \
  } while (0)

  FILE *fp = fopen(args.t, "r");
  if (fp == NULL) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }

  int hits = 0, misses = 0, evictions = 0;

  cache_t cache;
  cache_init(&cache, args.s, args.E, args.b);

  char line[32];
  int clk = 0;
  while (fgets(line, sizeof(line), fp)) {
    char type;
    uint64_t addr;
    int size;
    if (sscanf(line, " %c %lx,%d", &type, &addr, &size) != 3) {
      continue;
    }
    if (type != 'L' && type != 'S' && type != 'M') {
      continue;
    }
    printf_v("%c %lx,%d", type, addr, size);
    int num_accesses = (type == 'M') ? 2 : 1;
    for (int i = 0; i < num_accesses; i++) {
      int ret = cache_access(&cache, addr, clk++);
      if (ret & CACHE_MISS) {
        misses++;
        printf_v(" miss");
        if (ret & CACHE_EVICT) {
          evictions++;
          printf_v(" eviction");
        }
      } else {
        hits++;
        printf_v(" hit");
      }
    }
    printf_v("\n");
  }

  cache_destroy(&cache);
  if (fclose(fp) != 0) {
    perror("fclose");
    exit(EXIT_FAILURE);
  }

  printSummary(hits, misses, evictions);
  return 0;
}
