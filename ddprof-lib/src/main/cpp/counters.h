/*
 * Copyright 2023 Datadog, Inc
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
#ifndef JAVA_PROFILER_LIBRARY_COUNTERS_H
#define JAVA_PROFILER_LIBRARY_COUNTERS_H

#include "arch.h"
#include <vector>

#define DD_COUNTER_TABLE(X) \
    X(DICTIONARY_BYTES, "dictionary_bytes") \
    X(DICTIONARY_CLASSES_BYTES, "dictionary_classes_bytes") \
    X(DICTIONARY_ENDPOINTS_BYTES, "dictionary_endpoints_bytes") \
    X(DICTIONARY_CONTEXT_BYTES, "dictionary_context_bytes") \
    X(DICTIONARY_PAGES, "dictionary_pages") \
    X(DICTIONARY_CLASSES_PAGES, "dictionary_classes_pages") \
    X(DICTIONARY_ENDPOINTS_PAGES, "dictionary_endpoints_pages") \
    X(DICTIONARY_CONTEXT_PAGES, "dictionary_context_pages") \
    X(DICTIONARY_KEYS, "dictionary_keys") \
    X(DICTIONARY_CLASSES_KEYS, "dictionary_classes_keys") \
    X(DICTIONARY_ENDPOINTS_KEYS, "dictionary_endpoints_keys") \
    X(DICTIONARY_CONTEXT_KEYS, "dictionary_context_keys") \
    X(DICTIONARY_KEYS_BYTES, "dictionary_keys_bytes") \
    X(DICTIONARY_CLASSES_KEYS_BYTES, "dictionary_classes_keys_bytes") \
    X(DICTIONARY_ENDPOINTS_KEYS_BYTES, "dictionary_endpoints_keys_bytes") \
    X(DICTIONARY_CONTEXT_KEYS_BYTES, "dictionary_context_keys_bytes") \
    X(CONTEXT_STORAGE_BYTES, "context_storage_bytes") \
    X(CONTEXT_STORAGE_PAGES, "context_storage_pages") \
    X(CALLTRACE_STORAGE_BYTES, "calltrace_storage_bytes") \
    X(CALLTRACE_STORAGE_TRACES, "calltrace_storage_traces") \
    X(LINEAR_ALLOCATOR_BYTES, "linear_allocator_bytes") \
    X(LINEAR_ALLOCATOR_CHUNKS, "linear_allocator_chunks") \
    X(THREAD_IDS_COUNT, "thread_ids_count")  \
    X(THREAD_NAMES_COUNT, "thread_names_count") \
    X(THREAD_FILTER_PAGES, "thread_filter_pages") \
    X(THREAD_FILTER_BYTES, "thread_filter_bytes")
#define X_ENUM(a, b) a,
typedef enum CounterId : int {
    DD_COUNTER_TABLE(X_ENUM) DD_NUM_COUNTERS
} CounterId;
#undef X_ENUM

class Counters {
private:
    volatile long long* _counters;
    static long long* init();
    Counters() : _counters() {
        #ifdef COUNTERS
        _counters = Counters::init();
        #endif // COUNTERS
    }
public:
    static Counters& instance() {
        static Counters instance;
        return instance;
    }

    Counters(Counters const&) = delete;
    void operator=(Counters const&) = delete;

    static long long* getCounters() {
        return const_cast<long long*>(Counters::instance()._counters);
    }

    static constexpr int size() {
        return DD_NUM_COUNTERS * sizeof(long long) * 8;
    }

    static void set(CounterId counter, long long value, int offset = 0) {
        #ifdef COUNTERS
        storeRelease(Counters::instance()._counters[(static_cast<int>(counter) + offset) * 8], value);
        #endif // COUNTERS
    }

    static void increment(CounterId counter, long long delta = 1, int offset = 0) {
        #ifdef COUNTERS
        atomicInc(Counters::instance()._counters[(static_cast<int>(counter) + offset) * 8], delta);
        #endif // COUNTERS
    }

    static void decrement(CounterId counter, long long delta = 1, int offset = 0) {
        #ifdef COUNTERS
        increment(counter, -delta, offset);
        #endif // COUNTERS
    }

    static std::vector<const char*> describeCounters() {
        #ifdef COUNTERS
        #define X_NAME(a, b) b,
        return {
            DD_COUNTER_TABLE(X_NAME)
        };
        #undef X_NAME
        #else
        return {};
        #endif // COUNTERS
    }

};


#endif //JAVA_PROFILER_LIBRARY_COUNTERS_H
