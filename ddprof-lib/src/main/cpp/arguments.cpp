/*
 * Copyright 2017 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "arguments.h"
#include "vmEntry.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// Predefined value that denotes successful operation
const Error Error::OK(NULL);

// Extra buffer space for expanding file pattern
const size_t EXTRA_BUF_SIZE = 512;

static const Multiplier NANOS[] = {
    {'n', 1}, {'u', 1000}, {'m', 1000000}, {'s', 1000000000}, {0, 0}};
static const Multiplier BYTES[] = {
    {'b', 1}, {'k', 1024}, {'m', 1048576}, {'g', 1073741824}, {0, 0}};
static const Multiplier SECONDS[] = {
    {'s', 1}, {'m', 60}, {'h', 3600}, {'d', 86400}, {0, 0}};
static const Multiplier UNIVERSAL[] = {
    {'n', 1}, {'u', 1000}, {'m', 1000000},    {'s', 1000000000},
    {'b', 1}, {'k', 1024}, {'g', 1073741824}, {0, 0}};

// Statically compute hash code of a string containing up to 12 [a-z] letters
#define HASH(s)                                                                \
  ((s[0] & 31LL) | (s[1] & 31LL) << 5 | (s[2] & 31LL) << 10 |                  \
   (s[3] & 31LL) << 15 | (s[4] & 31LL) << 20 | (s[5] & 31LL) << 25 |           \
   (s[6] & 31LL) << 30 | (s[7] & 31LL) << 35 | (s[8] & 31LL) << 40 |           \
   (s[9] & 31LL) << 45 | (s[10] & 31LL) << 50 | (s[11] & 31LL) << 55)

// Simulate switch statement over string hashes
#define SWITCH(arg)                                                            \
  long long arg_hash = hash(arg);                                              \
  if (0)

#define CASE(s)                                                                \
  }                                                                            \
  else if (arg_hash == HASH(s "            ")) {

#define DEFAULT()                                                              \
  }                                                                            \
  else {

// Parses agent arguments.
// The format of the string is:
//     arg[,arg...]
// where arg is one of the following options:
//     start              - start profiling
//     resume             - start or resume profiling without resetting
//     collected data stop               - stop profiling check              -
//     check if the specified profiling event is available status             -
//     print profiling status (inactive / running for X seconds) list - show the
//     list of available profiling events version[=full]     - display the agent
//     version event=EVENT        - which event to trace (cpu, wall,
//     cache-misses, etc.) cpu[=INTERVAL]     - enable CPU profiling with the
//     specified sampling interval wall[=INTERVAL]    - enable wall-clock
//     profiling with the specified sampling interval walltpt=THREADS    -
//     sample THREADS threads per tick in wall-clock profiling
//     memory[=BYTES[:[a|[l|L[:RATIO]]]]]
//                        - memory profiling with suggested interval in bytes
//                        - 'a'=record allocation,'l'=record liveness,'L'=record
//                        liveness and heap usage
//                        - RATIO is a floating point number between 0.01
//                        and 1.0 to subsample live samples
//                        - eg. 'memory=1048576:a:L:0.1' to sample every 1MB
//                        with allocation, liveness and heap usage tracking,
//                          and keep the liveness track of 10% of the allocation
//                          samples
//     generations        - track surviving generations
//     lightweight[=BOOL] - enable lightweight profiling - events without
//     stacktraces (default: true) jfr                - dump events in Java
//     Flight Recorder format interval=N         - sampling interval in ns
//     (default: 10'000'000, i.e. 10 ms) jstackdepth=N      - maximum Java stack
//     depth (default: 2048) safemode=BITS      - disable stack recovery
//     techniques (default: 0, i.e. everything enabled) file=FILENAME      -
//     output file name for dumping log=FILENAME       - log warnings and errors
//     to the given dedicated stream loglevel=LEVEL     - logging level: TRACE,
//     DEBUG, INFO, WARN, ERROR, or NONE filter=FILTER      - thread filter
//     cstack=MODE        - how to collect C stack frames in addition to Java
//     stack
//                          MODE is 'fp', 'dwarf', 'lbr', 'vm' or 'no'
//     allkernel          - include only kernel-mode events
//     alluser            - include only user-mode events
//

Error Arguments::parse(const char *args) {
  if (args == NULL) {
    return Error::OK;
  }

  size_t len = strlen(args);
  if (_buf != NULL) {
    free(_buf);
  }
  _buf = (char *)malloc(len + EXTRA_BUF_SIZE + 1);
  if (_buf == NULL) {
    return Error("Not enough memory to parse arguments");
  }
  char *args_copy = strcpy(_buf + EXTRA_BUF_SIZE, args);

  const char *msg = NULL;

  for (char *arg = strtok(args_copy, ","); arg != NULL;
       arg = strtok(NULL, ",")) {
    char *value = strchr(arg, '=');
    if (value != NULL)
      *value++ = 0;

    SWITCH(arg) {
      // Actions
      CASE("start")
      _action = ACTION_START;

      CASE("resume")
      _action = ACTION_RESUME;

      CASE("stop")
      _action = ACTION_STOP;

      CASE("check")
      _action = ACTION_CHECK;

      CASE("status")
      _action = ACTION_STATUS;

      CASE("list")
      _action = ACTION_LIST;

      CASE("version")
      _action = ACTION_VERSION;

      CASE("jfr")
      if (value != NULL) {
        _jfr_options = (int)strtol(value, NULL, 0);
      }

      CASE("cpu")
      _cpu = value == NULL ? 0 : parseUnits(value, NANOS);
      if (_cpu < 0) {
        msg = "cpu must be >= 0";
      }

      CASE("wall")
      if (value == NULL) {
        _wall = 0;
      } else {
        if (value[0] == '~') {
          _wall_collapsing = true;
          _wall = parseUnits(value + 1, NANOS);
        } else {
          _wall = parseUnits(value, NANOS);
        }
      }
      if (_wall < 0) {
        msg = "wall must be >= 0";
      }

      CASE("walltpt")
      if (value == NULL || (_wall_threads_per_tick = atoi(value)) <= 0) {
        msg = "walltpt must be > 0";
      }

      CASE("event")
      if (value == NULL || value[0] == 0) {
        msg = "event must not be empty";
      } else if (strcmp(value, EVENT_ALLOC) == 0) {
        if (_memory < 0)
          _memory = 0;
      } else if (_event != NULL) {
        msg = "Duplicate event argument";
      } else {
        _event = value;
      }

      CASE("memory")
      char *config = value ? strchr(value, ':') : nullptr;
      char *ratio = nullptr;
      if (config) {
        *(config++) = 0; // terminate the 'value' string and update the pointer
                         // to the 'config' section
        ratio = strchr(config, ':');
        if (ratio) {
          *(ratio++) = 0; // terminate the preceding 'config' string and update
                          // the pointer to the 'ratio' section
        }
      }
      _memory =
          value == nullptr ? DEFAULT_ALLOC_INTERVAL : parseUnits(value, BYTES);
      if (_memory >= 0) {
        if (config) {
          if (strchr(config, 'a')) {
            _record_allocations = true;
          }
          if (strchr(config, 'l')) {
            _record_liveness = true;
          } else if (strchr(config, 'L')) {
            _record_liveness = true;
            _record_heap_usage = true;
          }
          if (_record_liveness && ratio) { // live-sample ratio is only
                                           // applicable when tracking liveness
            char *endptr;
            _live_samples_ratio =
                std::max(std::min(strtod(ratio, &endptr), 1.0),
                         0.01); // subsample at least 1% but not more than 100%
          }
        } else {
          // enable both allocations and liveness tracking
          _record_allocations = true;
          _record_liveness = true;
        }
      }

      CASE("generations")
      _gc_generations = value != NULL && strcmp(value, "true") == 0;
      if (_gc_generations && _memory <= 0) {
        _memory =
            4 * 1024 *
            1024; // very conservative sampling interval to reduce overhead
      }

      CASE("interval")
      if (value == NULL || (_interval = parseUnits(value, UNIVERSAL)) <= 0) {
        msg = "Invalid interval";
      }

      CASE("jstackdepth")
      if (value == NULL || (_jstackdepth = atoi(value)) <= 0) {
        msg = "jstackdepth must be > 0";
      }

      CASE("safemode")
      _safe_mode = value == NULL ? INT_MAX : (int)strtol(value, NULL, 0);

      CASE("file")
      if (value == NULL || value[0] == 0) {
        msg = "file must not be empty";
      }
      _file = value;

      CASE("log")
      _log = value == NULL || value[0] == 0 ? NULL : value;

      CASE("loglevel")
      if (value == NULL || value[0] == 0) {
        msg = "loglevel must not be empty";
      }
      _loglevel = value;

      // Filters
      CASE("filter")
      _filter = value == NULL ? "" : value;

      CASE("allkernel")
      _ring = RING_KERNEL;

      CASE("alluser")
      _ring = RING_USER;

      CASE("cstack")
      if (value != NULL) {
        if (strcmp(value, "fp") == 0) {
          _cstack = CSTACK_FP;
        } else if (strcmp(value, "dwarf") == 0) {
          _cstack = CSTACK_DWARF;
        } else if (strcmp(value, "lbr") == 0) {
          _cstack = CSTACK_LBR;
        } else if (strcmp(value, "vm") == 0) {
          _cstack = CSTACK_VM;
        } else if (strcmp(value, "vmx") == 0) {
          _cstack = CSTACK_VMX;
        } else {
          _cstack = CSTACK_NO;
        }
      }

      CASE("attributes")
      if (value != NULL) {
        std::string input(value);
        std::size_t start = 0;
        std::size_t end;
        while ((end = input.find(";", start)) != std::string::npos) {
          _context_attributes.push_back(input.substr(start, end - start));
          start = end + 1;
        }
        _context_attributes.push_back(input.substr(start));
      }

      CASE("lightweight")
      if (value != NULL) {
        switch (value[0]) {
        case 'y': // yes
        case 't': // true
          _lightweight = true;
          break;
        default:
          _lightweight = false;
        }
      }
            CASE("wallsampler")
                if (value != NULL) {
                    switch (value[0]) {
                        case 'j':
                            _wallclock_sampler = JVMTI;
                            break;
                        case 'a':
                        default:
                            _wallclock_sampler = ASGCT;
                    }
                }

      DEFAULT()
      if (_unknown_arg == NULL)
        _unknown_arg = arg;
    }
  }

  // Return error only after parsing all arguments, when 'log' is already set
  if (msg != NULL) {
    return Error(msg);
  }

  if (_event == NULL && _cpu < 0 && _wall < 0 && _memory < 0) {
    _event = EVENT_CPU;
  }

  if (VM::isOpenJ9()) {
    if (_cstack == CSTACK_FP) {
      // J9 is compiled without FP
      //   switch to DWARF for better results
      _cstack = CSTACK_DWARF;
    }
  }

  return Error::OK;
}

const char *Arguments::file() {
  if (_file != NULL && strchr(_file, '%') != NULL) {
    return expandFilePattern(_file);
  }
  return _file;
}

// Should match statically computed HASH(arg)
long long Arguments::hash(const char *arg) {
  long long h = 0;
  for (int shift = 0; *arg != 0; shift += 5) {
    h |= (*arg++ & 31LL) << shift;
  }
  return h;
}

// Expands the following patterns:
//   %p       process id
//   %t       timestamp (yyyyMMdd-hhmmss)
//   %n{MAX}  sequence number
//   %{ENV}   environment variable
const char *Arguments::expandFilePattern(const char *pattern) {
  char *ptr = _buf;
  char *end = _buf + EXTRA_BUF_SIZE - 1;

  while (ptr < end && *pattern != 0) {
    char c = *pattern++;
    if (c == '%') {
      c = *pattern++;
      if (c == 0) {
        break;
      } else if (c == 'p') {
        ptr += snprintf(ptr, end - ptr, "%d", getpid());
        continue;
      } else if (c == 't') {
        time_t timestamp = time(NULL);
        struct tm t;
        localtime_r(&timestamp, &t);
        ptr += snprintf(ptr, end - ptr, "%d%02d%02d-%02d%02d%02d",
                        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour,
                        t.tm_min, t.tm_sec);
        continue;
      } else if (c == '{') {
        char env_key[128];
        const char *p = strchr(pattern, '}');
        if (p != NULL && p - pattern < static_cast<int>(sizeof(env_key))) {
          memcpy(env_key, pattern, p - pattern);
          env_key[p - pattern] = 0;
          const char *env_value = getenv(env_key);
          if (env_value != NULL) {
            ptr += snprintf(ptr, end - ptr, "%s", env_value);
            pattern = p + 1;
            continue;
          }
        }
      }
    }
    *ptr++ = c;
  }

  *(ptr < end ? ptr : end) = 0;
  return _buf;
}

long Arguments::parseUnits(const char *str, const Multiplier *multipliers) {
  char *end;
  long result = strtol(str, &end, 0);

  char c = *end;
  if (c == 0) {
    return result;
  }
  if (c >= 'A' && c <= 'Z') {
    c += 'a' - 'A';
  }

  for (const Multiplier *m = multipliers; m->symbol; m++) {
    if (c == m->symbol) {
      return result * m->multiplier;
    }
  }

  return -1;
}

Arguments::~Arguments() {
  if (_buf != NULL) {
    free(_buf);
    _buf = NULL;
  }
}

void Arguments::save(Arguments &other) {
  other = *this;
  other._buf = NULL;
  other._shared = true;
}
