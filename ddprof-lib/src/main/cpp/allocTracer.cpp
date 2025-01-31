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

#include "allocTracer.h"
#include "context.h"
#include "stackFrame.h"
#include "thread.h"


// !!! This class is currently unused and all implementation code was commented out !!!
// If allocation profiling on JDK 8 is required this class will have to be brought back to life

int AllocTracer::_trap_kind;
Trap AllocTracer::_in_new_tlab(0);
Trap AllocTracer::_outside_tlab(1);

u64 AllocTracer::_interval;
volatile u64 AllocTracer::_allocated_bytes;


// Called whenever our breakpoint trap is hit
void AllocTracer::trapHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    #ifdef NEVER
    StackFrame frame(ucontext);
    int event_type;
    uintptr_t total_size;
    uintptr_t instance_size;

    // PC points either to BREAKPOINT instruction or to the next one
    if (_in_new_tlab.covers(frame.pc())) {
        // send_allocation_in_new_tlab(Klass* klass, HeapWord* obj, size_t tlab_size, size_t alloc_size, Thread* thread)
        // send_allocation_in_new_tlab_event(KlassHandle klass, size_t tlab_size, size_t alloc_size)
        event_type = BCI_ALLOC;
        total_size = _trap_kind == 1 ? frame.arg2() : frame.arg1();
        instance_size = _trap_kind == 1 ? frame.arg3() : frame.arg2();
    } else if (_outside_tlab.covers(frame.pc())) {
        // send_allocation_outside_tlab(Klass* klass, HeapWord* obj, size_t alloc_size, Thread* thread)
        // send_allocation_outside_tlab_event(KlassHandle klass, size_t alloc_size);
        event_type = BCI_ALLOC_OUTSIDE_TLAB;
        total_size = _trap_kind == 1 ? frame.arg2() : frame.arg1();
        instance_size = 0;
    } else {
        // Not our trap
        Profiler::instance()->trapHandler(signo, siginfo, ucontext);
        return;
    }

    // Leave the trapped function by simulating "ret" instruction
    uintptr_t klass = frame.arg0();
    frame.ret();

    if (_interval > 0 && updateCounter(_allocated_bytes, total_size, _interval)) {
        recordAllocation(ucontext, event_type, klass, total_size, instance_size);
    }
    #endif
}

void AllocTracer::recordAllocation(void* ucontext, int event_type, uintptr_t rklass,
                                   uintptr_t total_size, uintptr_t instance_size) {
    #ifdef NEVER
    int tid = ProfiledThread::currentTid();

    AllocEvent event;
    event._total_size = total_size;
    event._instance_size = instance_size;

    if (VMStructs::hasClassNames()) {
        VMSymbol* symbol = VMKlass::fromHandle(rklass)->name();
        int id = Profiler::instance()->lookupClass(symbol->body(), symbol->length());
        if (id == -1) {
            return;
        }
        event._id = id;
    }

    Profiler::instance()->recordSample(ucontext, total_size, tid, event_type, &event);
    #endif
}

Error AllocTracer::check(Arguments& args) {
    #ifdef NEVER
    if (_in_new_tlab.entry() != 0 && _outside_tlab.entry() != 0) {
        return Error::OK;
    }

    CodeCache* libjvm = VMStructs::libjvm();
    const void* ne;
    const void* oe;

    if ((ne = libjvm->findSymbolByPrefix("_ZN11AllocTracer27send_allocation_in_new_tlab")) != NULL &&
        (oe = libjvm->findSymbolByPrefix("_ZN11AllocTracer28send_allocation_outside_tlab")) != NULL) {
        _trap_kind = 1;  // JDK 10+
    } else if ((ne = libjvm->findSymbolByPrefix("_ZN11AllocTracer33send_allocation_in_new_tlab_eventE11KlassHandleP8HeapWord")) != NULL &&
               (oe = libjvm->findSymbolByPrefix("_ZN11AllocTracer34send_allocation_outside_tlab_eventE11KlassHandleP8HeapWord")) != NULL) {
        _trap_kind = 1;  // JDK 8u262+
    } else if ((ne = libjvm->findSymbolByPrefix("_ZN11AllocTracer33send_allocation_in_new_tlab_event")) != NULL &&
               (oe = libjvm->findSymbolByPrefix("_ZN11AllocTracer34send_allocation_outside_tlab_event")) != NULL) {
        _trap_kind = 2;  // JDK 7-9
    } else {
        return Error("No AllocTracer symbols found. Are JDK debug symbols installed?");
    }

    _in_new_tlab.assign(ne);
    _outside_tlab.assign(oe);
    _in_new_tlab.pair(_outside_tlab);
    #endif

    return Error::OK;
}

Error AllocTracer::start(Arguments& args) {
    #ifdef NEVER
    Error error = check(args);
    if (error) {
        return error;
    }

    _interval = args._memory > 0 ? args._memory : 0;
    _allocated_bytes = 0;

    if (!_in_new_tlab.install() || !_outside_tlab.install()) {
        return Error("Cannot install allocation breakpoints");
    }
    #endif

    return Error::OK;
}

void AllocTracer::stop() {
    #ifdef NEVER
    _in_new_tlab.uninstall();
    _outside_tlab.uninstall();
    #endif
}
