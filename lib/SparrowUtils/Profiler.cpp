#include "llvm/Support/raw_ostream.h"
#include "SparrowUtils/Profiler.h"

using namespace Sparrow;
using namespace llvm;

#define VALUE_UNDEF -1

Profiler::Profiler(property_t property)
        : properties_to_track(property) {
    reset();
    is_snapshoted = false;
}

Profiler::~Profiler() {

}

#ifdef __linux__
static int pick_raw_info(const char* info_name, const char* info_file) {
    int ret = VALUE_UNDEF;

    char temp[2048], label[64];
    FILE *fp;

    fp = fopen(info_file, "r");
    if (fp == NULL)
        return ret;

    char* fget_res = nullptr;
    while(true) {
        fget_res = fgets(temp, 1024, fp);
        if (!fget_res) {
            ret = VALUE_UNDEF;
            break;
        }
        sscanf(temp, "%s %d", label, &ret);
        if(strncmp(label, info_name, strlen(info_name)) == 0) {
            break;
        }
    }

    fclose(fp);
    return ret;
}

static int pick_proc_info(const char* info_name) {
    return pick_raw_info(info_name, "/proc/self/status");
}

static int pick_sys_info(const char* info_name) {
    return pick_raw_info(info_name, "/proc/meminfo");
}

#endif

int Profiler::pick_memory() {
    int ret = VALUE_UNDEF;
#ifdef __linux__
    ret = pick_proc_info("VmSize");
#endif
    return ret;
}

int Profiler::pick_peak_memory() {
    int ret = VALUE_UNDEF;
#ifdef __linux__
    ret = pick_proc_info("VmPeak");
#endif
    return ret;
}

int Profiler::pick_remaining_memory() {
    int ret = VALUE_UNDEF;
#ifdef __linux__
    ret = pick_sys_info("MemAvailable");
#endif
    return ret;
}

void Profiler::reset_memory() {

#ifndef __linux__
    // currently we only support memory measurement on linux
    return;
#endif

    int cur_mem = pick_memory();
    last_mem = cur_mem;
}

void Profiler::snapshot_memory() {

#ifndef __linux__
    // currently we only support memory measurement on linux
    return;
#endif

    int cur_mem = pick_memory();
    if (cur_mem == VALUE_UNDEF || last_mem == VALUE_UNDEF) {
        snapshoted_memory = VALUE_UNDEF;
    } else {
        snapshoted_memory = cur_mem - last_mem;
    }
}

void Profiler::print_memory(raw_ostream& o, const char* title) {
    assert (is_snapshoted && "printing the profiling result without a snapshot");

    print_memory_by_kb(o, title, snapshoted_memory);
}

void Profiler::print_memory_by_kb(raw_ostream& o, const char* title, int memory_value) {

#ifndef __linux__
    // currently we only support memory measurement on linux
    return;
#endif

    if (memory_value == VALUE_UNDEF) {
        o << "Memory tracking for " << title << " error\n";
        return;
    }

    int mem_cost_kb_val = memory_value & 0x3FF;
    int mem_cost_in_mb = memory_value >> 10;
    int mem_cost_mb_val = mem_cost_in_mb & 0x3FF;
    int mem_cost_in_gb = mem_cost_in_mb >> 10;

    o << title << " Memory: \t";
    if (mem_cost_in_gb != 0) {
        o << mem_cost_in_gb << "G ";
    }

    if (mem_cost_in_mb != 0) {
        o << mem_cost_mb_val << "M ";
    }

    o << mem_cost_kb_val << "KB\n";
}

void Profiler::reset_time() {
    time(&last_time);
}

void Profiler::snapshot_time() {
    time_t curr_time;
    time(&curr_time);

    snapshoted_time = (size_t) difftime(curr_time, last_time);
}

void Profiler::print_time(raw_ostream& o, const char* title) {
    assert (is_snapshoted && "printing the profiling result without a snapshot");

    print_time_by_second(o, title, snapshoted_time);
}

void Profiler::print_time_by_second(raw_ostream& o, const char* title, int time_value) {
    size_t hour = time_value / 3600;
    size_t min = time_value / 60;
    size_t min_val = min % 60;
    size_t sec = time_value % 60;

    o << title << " Time: \t";

    if (hour != 0) {
        o<< hour << "h ";
    }

    if (min != 0) {
        o << min_val << "m ";
    }

    o << sec << "s\n";
}

void Profiler::reset() {
    if (properties_to_track & TIME)
        reset_time();

    if (properties_to_track & MEMORY)
        reset_memory();

}

void Profiler::create_snapshot() {
    is_snapshoted = true;

    if (properties_to_track & TIME)
        snapshot_time();

    if (properties_to_track & MEMORY)
        snapshot_memory();
}

void Profiler::create_reset_snapshot() {
    create_snapshot();
    reset();
}

void Profiler::print_snapshot_result(raw_ostream& o, const char* title) {
    if (!is_snapshoted) {
        create_snapshot();
    }

    if (properties_to_track & TIME)
        print_time(o, title);

    if (properties_to_track & MEMORY)
        print_memory(o, title);
}

void Profiler::print_peak_memory(raw_ostream& o) {
    int peak_memory = pick_peak_memory();
    print_memory_by_kb(o, "Peak", peak_memory);
}

int Profiler::getRemainingMemInKB() {
    return pick_remaining_memory();
}

bool Profiler::hasEnoughMemory(int MemoryRequiredInKB, int* remain) {
    int remaining = getRemainingMemInKB();

    if (remain) {
        *remain = remaining;
    }

    if (remaining == VALUE_UNDEF) {
        return true;
    }

    if (remaining <= MemoryRequiredInKB) {
        outs() << "[Out of memory] Requiring " << MemoryRequiredInKB << "KB, but " << remaining << "left\n";
    }

    return remaining > MemoryRequiredInKB;
}
