#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>

using namespace std;

#define DRAM_SIZE (64*1024*1024*1024ULL)  // 64GB
#define L1_CACHE_SIZE (16*1024)           // 16KB
#define L2_CACHE_SIZE (128*1024)          // 128KB
#define L1_ASSOCIATIVITY 4                // 4-way associative
#define L2_ASSOCIATIVITY 8                // 8-way associative
#define L2_LINE_SIZE 64                   // Fixed 64B for L2
#define NO_OF_ITERATIONS 1000000          // 1M iterations

enum cacheResType {MISS=0, HIT=1};
enum accessType {READ_ACCESS=0, write_ACCESS=1};

// Random number generator
unsigned int m_w = 0xABABAB55;
unsigned int m_z = 0x05080902;

unsigned int rand_() {
    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return (m_z << 16) + m_w;
}

// Memory generators
unsigned int memGen1() {
    static unsigned int addr = 0;
    return (addr++) % (1024*1024);  // Reduced to prevent overflow issues
}

unsigned int memGen2() {
    return rand_() % (24*1024);
}

unsigned int memGen3() {
    return rand_() % (1024*1024);  // Reduced to prevent overflow
}

unsigned int memGen4() {
    static unsigned int addr = 0;
    return (addr++) % (4*1024);
}

unsigned int memGen5() {
    static unsigned int addr = 0;
    return (addr += 32) % (64*16*1024);
}

// Helper function to calculate log2
int log2_int(int value) {
    int result = 0;
    while (value > 1) {
        value >>= 1;
        result++;
    }
    return result;
}

// Cache line structure
struct CacheLine {
    bool valid;
    unsigned int tag;
    bool dirty;  // For write-back

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
    int offset_bits;
    int set_bits;

public:
    Cache(int size, int lineSize, int assoc, int hitTime)
        : cache_size(size), line_size(lineSize), associativity(assoc), hit_time(hitTime) {
        num_sets = cache_size / (line_size * associativity);
        cache.resize(num_sets, vector<CacheLine>(associativity));

        // Calculate bit positions
        offset_bits = log2_int(line_size);
        set_bits = log2_int(num_sets);

        // Initialize random seed only once
        static bool seed_initialized = false;
        if (!seed_initialized) {
            srand(time(NULL));
            seed_initialized = true;
        }
    }

    int getHitTime() { return hit_time; }

    pair<cacheResType, bool> access(unsigned int addr, accessType type) {
        // Extract set index and tag using correct bit manipulation
        unsigned int set_index = (addr >> offset_bits) & ((1 << set_bits) - 1);
        unsigned int tag = addr >> (offset_bits + set_bits);

        // Check for hit
        for (int way = 0; way < associativity; way++) {
            if (cache[set_index][way].valid && cache[set_index][way].tag == tag) {
                // Hit
                if (type == WRITE_ACCESS) {
                    cache[set_index][way].dirty = true;
                }
                return make_pair(HIT, false);
            }
        }

        // Miss - need to find replacement way
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
            replace_way = rand() % associativity;
            // Check if writeback is needed
            if (cache[set_index][replace_way].valid && cache[set_index][replace_way].dirty) {
                writeback_needed = true;
            }
        }

        // Install new line
        cache[set_index][replace_way].valid = true;
        cache[set_index][replace_way].tag = tag;
        cache[set_index][replace_way].dirty = (type == WRITE_ACCESS);

        return make_pair(MISS, writeback_needed);
    }

    void printStats() {
        cout << "Cache: " << cache_size/1024 << "KB, " << line_size << "B line size, "
             << associativity << "-way, " << num_sets << " sets\n";
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

    int memoryAccess(unsigned int addr, accessType type) {
        int cycles = 1; // Base CPU cycle

        // Try L1 cache
        pair<cacheResType, bool> l1_result = l1_cache->access(addr, type);

        if (l1_result.first == HIT) {
            return cycles;  // L1 hit - total 1 cycle
        }

        // L1 miss - check L2
        // Handle L1 writeback first if needed
        if (l1_result.second) {
            // L1 writeback - this is handled internally, just add L2 access time
            cycles += l2_cache->getHitTime();
        }

        // Now check L2 for the original request
        pair<cacheResType, bool> l2_result = l2_cache->access(addr, READ_ACCESS);
        cycles += l2_cache->getHitTime();

        if (l2_result.first == HIT) {
            // L2 hit - load data into L1
            return cycles;
        }

        // L2 miss - go to DRAM
        cycles += dram_penalty;

        // Handle L2 writeback if needed
        if (l2_result.second) {
            cycles += dram_penalty;  // Additional penalty for L2 writeback to DRAM
        }

        return cycles;
    }
};

// Function pointer type for memory generators
typedef unsigned int (*MemGenFunc)();

// Simulation function
double runSimulation(MemGenFunc memGen, int l1_line_size, const string& gen_name) {
    TwoLevelCache cache(l1_line_size);
    unsigned long long total_cycles = 0;
    int memory_accesses = 0;

    for (int i = 0; i < NO_OF_ITERATIONS; i++) {
        double p = (double)rand_() / RAND_MAX;  // Use RAND_MAX instead of UINT32_MAX

        if (p <= 0.35) {  // 35% memory instructions
            unsigned int addr = memGen();
            double rdwr = (double)rand_() / RAND_MAX;
            accessType type = (rdwr < 0.5) ? READ_ACCESS : WRITE_ACCESS;

            int cycles = cache.memoryAccess(addr, type);
            total_cycles += cycles;
            memory_accesses++;
        } else {
            total_cycles += 1;  // Non-memory instruction takes 1 cycle
        }
    }

    double cpi = (double)total_cycles / NO_OF_ITERATIONS;

    // Debug output for first run
    static bool first_run = true;
    if (first_run) {
        cout << "Debug info for " << gen_name << " with " << l1_line_size << "B line size:\n";
        cout << "Memory accesses: " << memory_accesses << " / " << NO_OF_ITERATIONS
             << " (" << (100.0 * memory_accesses / NO_OF_ITERATIONS) << "%)\n";
        cout << "Total cycles: " << total_cycles << ", CPI: " << cpi << "\n\n";
        first_run = false;
    }

    return cpi;
}

int main() {
    // Memory generators array
    MemGenFunc generators[5] = {memGen1, memGen2, memGen3, memGen4, memGen5};
    string gen_names[5] = {"memGen1", "memGen2", "memGen3", "memGen4", "memGen5"};

    // L1 line sizes to test
    int line_sizes[4] = {16, 32, 64, 128};

    cout << "Two-Level Cache Simulator Results\n";
    cout << "=================================\n\n";

    // Results table header
    cout << setw(12) << "Generator";
    for (int i = 0; i < 4; i++) {
        cout << setw(12) << (to_string(line_sizes[i]) + "B");
    }
    cout << "\n";

    cout << string(60, '-') << "\n";

    // Run simulations
    for (int gen = 0; gen < 5; gen++) {
        cout << setw(12) << gen_names[gen];

        for (int size = 0; size < 4; size++) {
            double cpi = runSimulation(generators[gen], line_sizes[size], gen_names[gen]);
            cout << setw(12) << fixed << setprecision(3) << cpi;
        }
        cout << "\n";
    }

    cout << "\nSimulation Parameters:\n";
    cout << "- L1 Cache: " << (L1_CACHE_SIZE/1024) << "KB, " << L1_ASSOCIATIVITY << "-way associative, 1 cycle hit time\n";
    cout << "- L2 Cache: " << (L2_CACHE_SIZE/1024) << "KB, " << L2_ASSOCIATIVITY << "-way associative, 10 cycle hit time\n";
    cout << "- DRAM: 50 cycle penalty\n";
    cout << "- " << NO_OF_ITERATIONS << " iterations per simulation\n";
    cout << "- 35% memory instructions, 50% reads, 50% writes\n";
    cout << "- Write-back policy with random replacement\n";

    // Test cache configuration
    cout << "\nCache Configuration Test:\n";
    Cache test_l1(L1_CACHE_SIZE, 64, L1_ASSOCIATIVITY, 1);
    test_l1.printStats();
    Cache test_l2(L2_CACHE_SIZE, L2_LINE_SIZE, L2_ASSOCIATIVITY, 10);
    test_l2.printStats();

    return 0;
}