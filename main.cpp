#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>

using namespace std;

#define DRAM_SIZE (64*1024*1024)          // 64MB (not 64GB to prevent overflow)
#define L1_CACHE_SIZE (16*1024)           // 16KB
#define L2_CACHE_SIZE (128*1024)          // 128KB
#define L1_ASSOCIATIVITY 4                // 4-way associative
#define L2_ASSOCIATIVITY 8                // 8-way associative
#define L2_LINE_SIZE 64                   // Fixed 64B for L2
#define NO_OF_ITERATIONS 10000          // 1M iterations

enum cacheResType {MISS=0, HIT=1};
enum accessType {READ_ACCESS=0, WRITE_ACCESS=1};  // Fixed: READ_ACCESS not read_ACCESS

// Random number generator (same as original project)
unsigned int m_w = 0xABABAB55;
unsigned int m_z = 0x05080902;

unsigned int rand_() {
    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return (m_z << 16) + m_w;
}

// Memory generators (matching original project specification)
unsigned int memGen1() {
    static unsigned int addr = 0;
    return (addr++) % DRAM_SIZE;
}

unsigned int memGen2() {
    return rand_() % (24*1024);
}

unsigned int memGen3() {
    return rand_() % DRAM_SIZE;
}

unsigned int memGen4() {
    static unsigned int addr = 0;
    return (addr++) % (4*1024);
}

unsigned int memGen5() {
    static unsigned int addr = 0;
    return (addr += 32) % (64*16*1024);
}

// Cache line structure
struct CacheLine {
    bool valid;
    unsigned int tag;
    bool dirty;

    CacheLine() : valid(false), tag(0), dirty(false) {}
};

// Cache class
class Cache {
private:
    vector<vector<CacheLine>> cache;
    int cache_size;
    int line_size;
    int associativity;
    int num_sets;
    int hit_time;

public:
    Cache(int size, int lineSize, int assoc, int hitTime)
        : cache_size(size), line_size(lineSize), associativity(assoc), hit_time(hitTime) {

        num_sets = cache_size / (line_size * associativity);
        cache.resize(num_sets, vector<CacheLine>(associativity));
    }

    int getHitTime() const { return hit_time; }

    pair<cacheResType, bool> access(unsigned int addr, accessType type) {
        // Calculate set index and tag
        unsigned int block_addr = addr / line_size;
        unsigned int set_index = block_addr % num_sets;
        unsigned int tag = block_addr / num_sets;

        // Check for hit
        for (int way = 0; way < associativity; way++) {
            if (cache[set_index][way].valid && cache[set_index][way].tag == tag) {
                // Hit - mark dirty if write
                if (type == WRITE_ACCESS) {
                    cache[set_index][way].dirty = true;
                }
                return make_pair(HIT, false);
            }
        }

        // Miss - find replacement victim
        int replace_way = -1;
        bool writeback_needed = false;

        // Look for invalid line first
        for (int way = 0; way < associativity; way++) {
            if (!cache[set_index][way].valid) {
                replace_way = way;
                break;
            }
        }

        // If no invalid line, use random replacement
        if (replace_way == -1) {
            replace_way = rand_() % associativity;
            // Check if writeback needed for victim
            if (cache[set_index][replace_way].dirty) {
                writeback_needed = true;
            }
        }

        // Install new line
        cache[set_index][replace_way].valid = true;
        cache[set_index][replace_way].tag = tag;
        cache[set_index][replace_way].dirty = (type == WRITE_ACCESS);

        return make_pair(MISS, writeback_needed);
    }

    void reset() {
        for (int set = 0; set < num_sets; set++) {
            for (int way = 0; way < associativity; way++) {
                cache[set][way].valid = false;
                cache[set][way].dirty = false;
                cache[set][way].tag = 0;
            }
        }
    }
};

// Two-level cache simulator
class TwoLevelCache {
private:
    Cache* l1_cache;
    Cache* l2_cache;
    int dram_penalty;

public:
    TwoLevelCache(int l1_line_size) : dram_penalty(50) {
        l1_cache = new Cache(L1_CACHE_SIZE, l1_line_size, L1_ASSOCIATIVITY, 1);
        l2_cache = new Cache(L2_CACHE_SIZE, L2_LINE_SIZE, L2_ASSOCIATIVITY, 10);
    }

    ~TwoLevelCache() {
        delete l1_cache;
        delete l2_cache;
    }

    void reset() {
        l1_cache->reset();
        l2_cache->reset();
    }

    int memoryAccess(unsigned int addr, accessType type) {
        int cycles = 0;

        // Try L1 cache first
        pair<cacheResType, bool> l1_result = l1_cache->access(addr, type);
        cycles += l1_cache->getHitTime();  // Always pay L1 access time

        if (l1_result.first == HIT) {
            return cycles;  // L1 hit
        }

        // L1 miss - handle L1 writeback if needed
        if (l1_result.second) {
            // Need to write dirty line from L1 to L2
            // This is simplified - in reality we'd need the victim address
            cycles += l2_cache->getHitTime();
        }

        // Check L2 for the requested data
        pair<cacheResType, bool> l2_result = l2_cache->access(addr, READ_ACCESS);  // Fixed: READ_ACCESS
        cycles += l2_cache->getHitTime();

        if (l2_result.first == HIT) {
            // L2 hit - data found in L2
            return cycles;
        }

        // L2 miss - go to DRAM
        cycles += dram_penalty;

        // Handle L2 writeback if needed
        if (l2_result.second) {
            cycles += dram_penalty;  // Write dirty L2 line to DRAM
        }

        return cycles;
    }
};

// Function pointer type for memory generators
typedef unsigned int (*MemGenFunc)();

// Simulation function
double runSimulation(MemGenFunc memGen, int l1_line_size, const string& gen_name, bool verbose = false) {
    TwoLevelCache cache(l1_line_size);
    unsigned long long total_cycles = 0;
    int memory_access_count = 0;

    for (int i = 0; i < NO_OF_ITERATIONS; i++) {
        double p = (double)rand_() / (double)0xFFFFFFFF;

        if (p <= 0.35) {  // 35% memory instructions
            unsigned int addr = memGen();
            double rdwr = (double)rand_() / (double)0xFFFFFFFF;
            accessType type = (rdwr < 0.5) ? READ_ACCESS : WRITE_ACCESS;

            int cycles = cache.memoryAccess(addr, type);
            total_cycles += cycles;
            memory_access_count++;

            // Optional verbose output
            if (verbose) {
                cout << "Access " << setw(7) << memory_access_count
                     << " (Iter " << setw(7) << i << "): 0x"
                     << setfill('0') << setw(8) << hex << addr
                     << setfill(' ') << " - ";
                if (cycles == 1) {
                    cout << "L1 HIT  (1 cycle)";
                } else if (cycles <= 11) {
                    cout << "L2 HIT  (" << dec << cycles << " cycles)";
                } else {
                    cout << "L2 MISS (" << dec << cycles << " cycles)";
                }
                cout << "\n";
            }
        } else {
            total_cycles += 1;  // Non-memory instruction takes 1 cycle
        }
    }

    if (verbose) {
        cout << "Total memory accesses: " << memory_access_count << "\n";
        cout << "Total cycles: " << total_cycles << "\n";
    }

    double cpi = (double)total_cycles / NO_OF_ITERATIONS;
    return cpi;
}