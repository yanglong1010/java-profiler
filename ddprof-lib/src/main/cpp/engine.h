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

#ifndef _ENGINE_H
#define _ENGINE_H

#include "arguments.h"


class Engine {
  protected:
    static bool updateCounter(volatile unsigned long long& counter, unsigned long long value, unsigned long long interval) {
        if (interval <= 1) {
            return true;
        }

        while (true) {
            unsigned long long prev = counter;
            unsigned long long next = prev + value;
            if (next < interval) {
                if (__sync_bool_compare_and_swap(&counter, prev, next)) {
                    return false;
                }
            } else {
                if (__sync_bool_compare_and_swap(&counter, prev, next % interval)) {
                    return true;
                }
            }
        }
    }

  public:

    virtual const char* name() {
        return "Engine";
    }

    virtual Error check(Arguments& args);
    virtual Error start(Arguments& args);
    virtual void stop();

    virtual int registerThread(int tid) { return -1; }
    virtual void unregisterThread(int tid) {}

    virtual void enableEvents(bool enabled) {
        // do nothing
    }
};

#endif // _ENGINE_H
