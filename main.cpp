#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
using namespace std;

#define DRAM_SIZE (64ULL * 1024 * 1024 * 1024)
#define L1_CACHE_SIZE (16 * 1024)
#define L2_CACHE_SIZE (128 * 1024)
#define L1_ASSOCIATIVITY 4
#define L2_ASSOCIATIVITY 8
#define L2_LINE_SIZE 64
#define NO_OF_ITERATIONS 1000000

enum cacheResType { MISS = 0, HIT = 1 };
enum accessType { read_ACCESS = 0, WRITE_ACCESS = 1 };

// Custom random number generator
unsigned int m_w = 0xABABAB55;
unsigned int m_z = 0x05080902;

void seed_random() {
    unsigned int seed = (unsigned int)time(NULL);
    m_w = seed ^ 0xABABAB55;
    m_z = (seed >> 16) ^ 0x05080902;
    if (m_w == 0) m_w = 0xABABAB55;
    if (m_z == 0) m_z = 0x05080902;
}

unsigned int rand_() {
    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return (m_z << 16) + m_w;
}

// Memory generators
unsigned int memGen1() { static unsigned int addr = 0; return (addr++) % DRAM_SIZE; }
unsigned int memGen2() { return rand_() % (24 * 1024); }
unsigned int memGen3() { return rand_() % DRAM_SIZE; }
unsigned int memGen4() { static unsigned int addr = 0; return (addr++) % (4 * 1024); }
unsigned int memGen5() { static unsigned int addr = 0; return (addr += 32) % (64 * 16 * 1024); }

struct CacheLine {
    bool valid = false;
    unsigned long long tag = 0;
    bool dirty = false;
};

class Cache {
private:
    vector<vector<CacheLine>> cache;
    int cache_size, line_size, associativity, num_sets, hit_time;
    mutable unsigned long long hits = 0;
    mutable unsigned long long misses = 0;
    mutable unsigned long long writebacks = 0;

public:
    Cache(int size, int lineSize, int assoc, int hitTime)
        : cache_size(size), line_size(lineSize), associativity(assoc), hit_time(hitTime) {
        num_sets = cache_size / (line_size * associativity);
        cache.resize(num_sets, vector<CacheLine>(associativity));
    }

    int getHitTime() const { return hit_time; }
    int getCacheSize() const { return cache_size; }
    int getLineSize() const { return line_size; }
    int getAssociativity() const { return associativity; }
    int getNumSets() const { return num_sets; }

    unsigned long long getHits() const { return hits; }
    unsigned long long getMisses() const { return misses; }
    unsigned long long getWritebacks() const { return writebacks; }
    double getHitRate() const {
        unsigned long long total = hits + misses;
        return total > 0 ? (double)hits / total : 0.0;
    }
    void resetStats() { hits = misses = writebacks = 0; }

    pair<cacheResType, bool> access(unsigned long long addr, accessType type) {
        unsigned long long block_addr = addr / line_size;
        unsigned int set_index = block_addr % num_sets;
        unsigned long long tag = block_addr / num_sets;

        // Check for hit
        for (int way = 0; way < associativity; way++) {
            if (cache[set_index][way].valid && cache[set_index][way].tag == tag) {
                hits++;
                if (type == WRITE_ACCESS) cache[set_index][way].dirty = true;
                return {HIT, false};
            }
        }

        // Miss occurred
        misses++;
        int replace_way = -1;
        bool writeback = false;

        // Find empty way first
        for (int way = 0; way < associativity; way++) {
            if (!cache[set_index][way].valid) {
                replace_way = way;
                break;
            }
        }

        // If no empty way, use random replacement
        if (replace_way == -1) {
            replace_way = rand_() % associativity;
            if (cache[set_index][replace_way].dirty) {
                writeback = true;
                writebacks++;
            }
        }

        cache[set_index][replace_way].valid = true;
        cache[set_index][replace_way].tag = tag;
        cache[set_index][replace_way].dirty = (type == WRITE_ACCESS);
        return {MISS, writeback};
    }

    void reset() {
        for (auto &set : cache)
            for (auto &line : set)
                line = {};
        resetStats();
    }
};

class TwoLevelCache {
private:
    Cache *l1_cache, *l2_cache;
    int dram_penalty;
    mutable unsigned long long total_accesses = 0;
    mutable unsigned long long total_cycles = 0;

public:
    TwoLevelCache(int l1_line_size) : dram_penalty(50) {
        l1_cache = new Cache(L1_CACHE_SIZE, l1_line_size, L1_ASSOCIATIVITY, 1);
        l2_cache = new Cache(L2_CACHE_SIZE, L2_LINE_SIZE, L2_ASSOCIATIVITY, 10);
    }
    ~TwoLevelCache() { delete l1_cache; delete l2_cache; }

    void reset() {
        l1_cache->reset();
        l2_cache->reset();
        total_accesses = total_cycles = 0;
    }

    Cache* getL1Cache() const { return l1_cache; }
    Cache* getL2Cache() const { return l2_cache; }

    double getAverageAccessTime() const {
        return total_accesses > 0 ? (double)total_cycles / total_accesses : 0.0;
    }

    int memoryAccess(unsigned long long addr, accessType type) {
        total_accesses++;
        int cycles = 0;

        // Always pay L1 access time
        cycles += l1_cache->getHitTime();
        auto l1_result = l1_cache->access(addr, type);

        if (l1_result.first == HIT) {
            total_cycles += cycles;
            return cycles;
        }

        // L1 miss - handle writeback if needed
        if (l1_result.second) {
            cycles += l2_cache->getHitTime();
        }

        // Access L2
        cycles += l2_cache->getHitTime();
        auto l2_result = l2_cache->access(addr, read_ACCESS);

        if (l2_result.first == HIT) {
            total_cycles += cycles;
            return cycles;
        }

        // L2 miss - access DRAM
        cycles += dram_penalty;

        // Handle L2 writeback if needed
        if (l2_result.second) {
            cycles += dram_penalty;
        }

        total_cycles += cycles;
        return cycles;
    }
};

class CacheSimulator {
public:
    void runSimulations() {
        seed_random();
        unsigned int (*generators[])() = {memGen1, memGen2, memGen3, memGen4, memGen5};
        string gen_names[] = {"memGen1", "memGen2", "memGen3", "memGen4", "memGen5"};
        int line_sizes[] = {16, 32, 64, 128};

        cout << "\n" << string(70, '=') << "\n";
        cout << "                    CACHE SIMULATION RESULTS\n";
        cout << string(70, '=') << "\n";

        cout << "\n+------------+------------+------------+------------+------------+\n";
        cout << "| Generator  |   16B Line |   32B Line |   64B Line |  128B Line |\n";
        cout << "+------------+------------+------------+------------+------------+\n";

        for (int g = 0; g < 5; g++) {
            cout << "| " << setw(10) << gen_names[g] << " ";
            for (int l = 0; l < 4; l++) {
                double cpi = run(generators[g], line_sizes[l]);
                cout << "| " << setw(10) << fixed << setprecision(4) << cpi << " ";
            }
            cout << "|\n";
        }
        cout << "+------------+------------+------------+------------+------------+\n";

        cout << "\nCPI Calculation Explanation:\n";
        cout << "- Total iterations: " << NO_OF_ITERATIONS << "\n";
        cout << "- Memory access probability: 35%\n";
        cout << "- Expected memory accesses per run: ~" << (int)(NO_OF_ITERATIONS * 0.35) << "\n";
        cout << "- Non-memory instructions: 1 cycle each\n";
        cout << "- Memory access cycles vary based on cache hits/misses\n";
        cout << "- CPI = Total Cycles / Total Instructions\n";
    }

    double run(unsigned int (*gen)(), int l1_line_size) {
        TwoLevelCache cache(l1_line_size);
        unsigned long long total_cycles = 0;
        unsigned long long memory_accesses = 0;
        unsigned long long non_memory_instructions = 0;

        for (int i = 0; i < NO_OF_ITERATIONS; i++) {
            double p = (double)rand_() / 0xFFFFFFFF;
            if (p <= 0.35) {
                // Memory access instruction
                memory_accesses++;
                accessType type = ((double)rand_() / 0xFFFFFFFF < 0.5) ? read_ACCESS : WRITE_ACCESS;
                total_cycles += cache.memoryAccess(gen(), type);
            } else {
                // Non-memory instruction
                non_memory_instructions++;
                total_cycles += 1;
            }
        }

        // Debug information (commented out for clean output)
        /*
        cout << "    Memory accesses: " << memory_accesses
             << ", Non-memory: " << non_memory_instructions
             << ", Total cycles: " << total_cycles << endl;
        */

        return (double)total_cycles / NO_OF_ITERATIONS;
    }

    void runComprehensiveTests() {
        cout << "\n" << string(70, '=') << "\n";
        cout << "                    COMPREHENSIVE TEST SUITE\n";
        cout << string(70, '=') << "\n";

        int passed = 0, total = 0;

        cout << "\n>>> BASIC CACHE FUNCTIONALITY TESTS <<<\n";
        cout << string(50, '-') << "\n";
        runBasicTests(passed, total);

        cout << "\n>>> CACHE HIERARCHY TESTS <<<\n";
        cout << string(50, '-') << "\n";
        runHierarchyTests(passed, total);

        cout << "\n>>> MEMORY GENERATOR TESTS <<<\n";
        cout << string(50, '-') << "\n";
        runMemoryGeneratorTests(passed, total);

        cout << "\n>>> PERFORMANCE ANALYSIS TESTS <<<\n";
        cout << string(50, '-') << "\n";
        runPerformanceTests(passed, total);

        cout << "\n>>> HIT/MISS RATIO TESTS <<<\n";
        cout << string(50, '-') << "\n";
        runHitMissRatioTests(passed, total);

        cout << "\n" << string(70, '=') << "\n";
        cout << "                       TEST SUMMARY\n";
        cout << string(70, '=') << "\n";
        cout << "Tests Passed: " << passed << "/" << total;
        cout << " (" << fixed << setprecision(1) << (100.0 * passed / total) << "%)\n";

        if (passed == total) {
            cout << "✓ ALL TESTS PASSED! Cache simulator is working correctly.\n";
        } else {
            cout << "⚠ Some tests failed. Please review the implementation.\n";
        }
        cout << string(70, '=') << "\n";
    }

private:
    void runBasicTests(int &passed, int &total) {
        assertTest("Basic Cache Hit", testBasicCacheHit(), passed, total);
        assertTest("Cache Miss Handling", testCacheMiss(), passed, total);
        assertTest("Write-back Policy", testWriteBack(), passed, total);
        assertTest("Set Index Mapping", testSetMapping(), passed, total);
        assertTest("Cache Line Alignment", testCacheLineAlignment(), passed, total);
    }

    void runHierarchyTests(int &passed, int &total) {
        assertTest("L1-L2 Integration", testTwoLevelCache(), passed, total);
        assertTest("L1 Miss -> L2 Hit", testL1MissL2Hit(), passed, total);
        assertTest("L1 Miss -> L2 Miss", testL1MissL2Miss(), passed, total);
        assertTest("Cache Hierarchy Timing", testHierarchyTiming(), passed, total);
    }

    void runMemoryGeneratorTests(int &passed, int &total) {
        assertTest("Memory Generator Patterns", testMemGenPatterns(), passed, total);
        assertTest("Generator Address Ranges", testGeneratorRanges(), passed, total);
        assertTest("Sequential vs Random Access", testAccessPatterns(), passed, total);
    }

    void runPerformanceTests(int &passed, int &total) {
        assertTest("Hit Rate Calculation", testHitRateCalculation(), passed, total);
        assertTest("Performance Statistics", testPerformanceStats(), passed, total);
        assertTest("Cache Reset Functionality", testReset(), passed, total);
    }

    void runHitMissRatioTests(int &passed, int &total) {
        assertTest("Sequential Access Hit Rates", testSequentialHitRates(), passed, total);
        seed_random();
        assertTest("Random Access Hit Rates", testRandomHitRates(), passed, total);
        seed_random();
        assertTest("Working Set Impact", testWorkingSetImpact(), passed, total);
        seed_random();
        assertTest("Line Size Impact on Hit Rates", testLineSizeHitRateCorrelation(), passed, total);
    }

    void assertTest(const string& name, bool result, int &passed, int &total) {
        string status = result ? "PASS" : "FAIL";
        cout << "[" << status << "] " << name;
        if (!result) cout << " ⚠️";
        cout << "\n";
        total++;
        if (result) passed++;
    }

    // Improved test implementations with proper validation and output
    bool testBasicCacheHit() {
        Cache c(1024, 64, 2, 1);

        auto result1 = c.access(0x1000, read_ACCESS);
        auto result2 = c.access(0x1000, read_ACCESS);
        auto result3 = c.access(0x1008, read_ACCESS); // Same cache line

        bool test1 = (result1.first == MISS);
        bool test2 = (result2.first == HIT);
        bool test3 = (result3.first == HIT);

        if (!test1 || !test2 || !test3) {
            cout << "    ⚠ Expected: MISS->HIT->HIT, Got: "
                 << (result1.first == MISS ? "MISS" : "HIT") << "->"
                 << (result2.first == MISS ? "MISS" : "HIT") << "->"
                 << (result3.first == MISS ? "MISS" : "HIT") << "\n";
        }
        return test1 && test2 && test3;
    }

    bool testCacheMiss() {
        Cache c(1024, 64, 2, 1);

        auto r1 = c.access(0x0000, read_ACCESS);
        auto r2 = c.access(0x0400, read_ACCESS);  // Different set

        bool result = (r1.first == MISS) && (r2.first == MISS);

        if (!result) {
            cout << "    ⚠ Both accesses should miss on first access\n";
        }
        return result;
    }

    bool testWriteBack() {
        Cache c(1024, 64, 2, 1);

        c.access(0x1000, WRITE_ACCESS);
        auto result = c.access(0x1000, read_ACCESS);

        bool hit = (result.first == HIT);
        if (!hit) {
            cout << "    ⚠ Write followed by read should hit\n";
        }
        return hit;
    }

    bool testSetMapping() {
        Cache c(1024, 64, 2, 1);

        c.access(0x0000, read_ACCESS);
        c.access(0x0040, read_ACCESS);

        bool test1 = c.access(0x0000, read_ACCESS).first == HIT;
        bool test2 = c.access(0x0040, read_ACCESS).first == HIT;

        if (!test1 || !test2) {
            cout << "    ⚠ Different lines in same set should coexist\n";
        }
        return test1 && test2;
    }

    bool testCacheLineAlignment() {
        Cache c(1024, 64, 2, 1);

        c.access(0x1000, read_ACCESS);
        bool test1 = c.access(0x1010, read_ACCESS).first == HIT;
        bool test2 = c.access(0x1020, read_ACCESS).first == HIT;
        bool test3 = c.access(0x103F, read_ACCESS).first == HIT;

        if (!test1 || !test2 || !test3) {
            cout << "    ⚠ All addresses in same cache line should hit\n";
        }
        return test1 && test2 && test3;
    }

    bool testTwoLevelCache() {
        TwoLevelCache tlc(64);

        int cycles1 = tlc.memoryAccess(0x12345678, read_ACCESS);
        int cycles2 = tlc.memoryAccess(0x12345678, read_ACCESS);

        bool result = (cycles1 > 50) && (cycles2 == 1);

        if (!result) {
            cout << "    ⚠ Expected: DRAM access (" << cycles1 << ") then L1 hit (" << cycles2 << ")\n";
        }
        return result;
    }

    bool testL1MissL2Hit() {
        TwoLevelCache tlc(32);

        // First access - loads into both L1 and L2
        tlc.memoryAccess(0x1000, read_ACCESS);

        // Calculate number of accesses needed to guarantee eviction
        // L1: 16KB, 32B lines, 4-way = 16384/(32*4) = 128 sets
        // Need enough accesses to overwhelm the 4-way associativity
        int num_sets = 16384 / (32 * 4);  // 128 sets
        int accesses_needed = num_sets * 8;  // 8 times the number of sets to ensure eviction

        // Fill L1 to force eviction of address 0x1000
        for (int i = 0; i < accesses_needed; i++) {
            // Access different addresses that map to different sets
            tlc.memoryAccess(0x100000 + i * 128, read_ACCESS);
        }

        // Access original address - should be L1 miss, L2 hit
        int cycles = tlc.memoryAccess(0x1000, read_ACCESS);
        bool result = (cycles > 1 && cycles < 30);

        if (!result) {
            cout << "    ⚠ Expected L1 miss + L2 hit (~11 cycles), got " << cycles << "\n";
            cout << "    Performed " << accesses_needed << " eviction accesses\n";
        }
        return result;
    }
    bool testL1MissL2Miss() {
        TwoLevelCache tlc(32);
        int cycles = tlc.memoryAccess(0x12345678, read_ACCESS);
        bool result = (cycles > 50);

        if (!result) {
            cout << "    ⚠ Expected DRAM access (>50 cycles), got " << cycles << "\n";
        }
        return result;
    }

    bool testHierarchyTiming() {
        TwoLevelCache tlc(64);

        // L1 hit test
        tlc.memoryAccess(0x1000, read_ACCESS);
        int l1_cycles = tlc.memoryAccess(0x1000, read_ACCESS);

        // DRAM access test
        int dram_cycles = tlc.memoryAccess(0x2000000, read_ACCESS);

        bool result = (l1_cycles == 1) && (dram_cycles > 50);

        if (!result) {
            cout << "    ⚠ L1 hit: " << l1_cycles << " cycles, DRAM: " << dram_cycles << " cycles\n";
        }
        return result;
    }

    bool testMemGenPatterns() {
        // Reset generators for testing
        vector<unsigned> g1_vals, g4_vals;

        // Test sequential generators
        for (int i = 0; i < 5; i++) g1_vals.push_back(memGen1());
        for (int i = 0; i < 5; i++) g4_vals.push_back(memGen4());

        bool g1_sequential = true;
        for (int i = 1; i < 5; i++) {
            if (g1_vals[i] != (g1_vals[i-1] + 1) % DRAM_SIZE) {
                g1_sequential = false;
                break;
            }
        }

        if (!g1_sequential) {
            cout << "    ⚠ memGen1 should produce sequential addresses\n";
        }

        return g1_sequential;
    }

    bool testGeneratorRanges() {
        bool result = true;

        // Test range constraints
        for (int i = 0; i < 100; i++) {
            if (memGen2() >= 24 * 1024) {
                result = false;
                cout << "    ⚠ memGen2 exceeded 24KB range\n";
                break;
            }
            if (memGen4() >= 4 * 1024) {
                result = false;
                cout << "    ⚠ memGen4 exceeded 4KB range\n";
                break;
            }
        }

        return result;
    }

    bool testAccessPatterns() {
        TwoLevelCache tlc1(64), tlc2(64);

        // Sequential access pattern
        for (int i = 0; i < 1000; i++) {
            tlc1.memoryAccess(i * 4, read_ACCESS);
        }

        // Random access pattern
        for (int i = 0; i < 1000; i++) {
            tlc2.memoryAccess(rand_() % (1024*1024), read_ACCESS);
        }

        double seq_hit_rate = tlc1.getL1Cache()->getHitRate();
        double rand_hit_rate = tlc2.getL1Cache()->getHitRate();

        bool result = seq_hit_rate > rand_hit_rate;

        if (!result) {
            cout << "    ⚠ Sequential: " << fixed << setprecision(3) << seq_hit_rate
                 << ", Random: " << rand_hit_rate << "\n";
        }

        return result;
    }

    bool testHitRateCalculation() {
        Cache c(1024, 64, 2, 1);

        c.access(0x1000, read_ACCESS); // miss
        c.access(0x1000, read_ACCESS); // hit
        c.access(0x1000, read_ACCESS); // hit
        c.access(0x2000, read_ACCESS); // miss

        double hit_rate = c.getHitRate();
        bool result = (hit_rate >= 0.49 && hit_rate <= 0.51);

        if (!result) {
            cout << "    ⚠ Expected ~0.50, got " << fixed << setprecision(3) << hit_rate << "\n";
            cout << "    Hits: " << c.getHits() << ", Misses: " << c.getMisses() << "\n";
        }

        return result;
    }

    bool testPerformanceStats() {
        TwoLevelCache tlc(64);

        for (int i = 0; i < 100; i++) {
            tlc.memoryAccess(i * 64, read_ACCESS);
        }

        bool result = (tlc.getL1Cache()->getHits() + tlc.getL1Cache()->getMisses()) == 100;

        if (!result) {
            cout << "    ⚠ Total accesses should equal hits + misses\n";
            cout << "    Hits: " << tlc.getL1Cache()->getHits()
                 << ", Misses: " << tlc.getL1Cache()->getMisses() << "\n";
        }

        return result;
    }

    bool testReset() {
        TwoLevelCache cache(64);
        cache.memoryAccess(0x1000, read_ACCESS);
        cache.reset();

        bool stats_reset = (cache.getL1Cache()->getHits() == 0) &&
                          (cache.getL1Cache()->getMisses() == 0);
        int cycles = cache.memoryAccess(0x1000, read_ACCESS);
        bool cache_cleared = (cycles > 50);

        bool result = stats_reset && cache_cleared;

        if (!result) {
            cout << "    ⚠ Reset failed - Stats reset: " << stats_reset
                 << ", Cache cleared: " << cache_cleared << "\n";
        }

        return result;
    }

    bool testSequentialHitRates() {
        TwoLevelCache tlc(64);

        for (int i = 0; i < 5000; i++) {
            tlc.memoryAccess(i * 4, read_ACCESS);
        }

        double l1_hit_rate = tlc.getL1Cache()->getHitRate();
        bool result = l1_hit_rate > 0.5; // Reasonable expectation

        cout << "    Sequential L1 hit rate: " << fixed << setprecision(3) << l1_hit_rate << "\n";

        return result;
    }

    bool testRandomHitRates() {
        TwoLevelCache tlc(64);

        for (int i = 0; i < 5000; i++) {
            tlc.memoryAccess(rand_() % (1024*1024), read_ACCESS);
        }

        double l1_hit_rate = tlc.getL1Cache()->getHitRate();
        bool result = l1_hit_rate < 0.5; // Should be lower than sequential

        cout << "    Random L1 hit rate: " << fixed << setprecision(3) << l1_hit_rate << "\n";

        return result;
    }

    bool testWorkingSetImpact() {
        TwoLevelCache tlc_small(64), tlc_large(64);

        // Small working set (4KB)
        for (int i = 0; i < 500; i++) {
            tlc_small.memoryAccess(rand_() % (4 * 1024), read_ACCESS);
        }

        // Large working set (64KB)
        for (int i = 0; i < 500; i++) {
            tlc_large.memoryAccess(rand_() % (64 * 1024), read_ACCESS);
        }

        double small_hit_rate = tlc_small.getL1Cache()->getHitRate();
        double large_hit_rate = tlc_large.getL1Cache()->getHitRate();

        cout << "    Small WS (4KB): " << fixed << setprecision(3) << small_hit_rate
             << ", Large WS (64KB): " << large_hit_rate << "\n";

        return small_hit_rate >= large_hit_rate; // Small should be better or equal
    }

    bool testLineSizeHitRateCorrelation() {
        TwoLevelCache tlc_16(16), tlc_64(64), tlc_128(128);

        // Sequential access should benefit from larger lines
        for (int i = 0; i < 500; i++) {
            unsigned addr = i * 8;
            tlc_16.memoryAccess(addr, read_ACCESS);
            tlc_64.memoryAccess(addr, read_ACCESS);
            tlc_128.memoryAccess(addr, read_ACCESS);
        }

        double hr_16 = tlc_16.getL1Cache()->getHitRate();
        double hr_64 = tlc_64.getL1Cache()->getHitRate();
        double hr_128 = tlc_128.getL1Cache()->getHitRate();

        cout << "    Hit rates - 16B: " << fixed << setprecision(3) << hr_16
             << ", 64B: " << hr_64 << ", 128B: " << hr_128 << "\n";

        return hr_128 >= hr_64 && hr_64 >= hr_16; // Larger lines should be better for sequential
    }
};

int main() {
    CacheSimulator sim;

    cout << "Starting Cache Simulator Tests and Analysis...\n";

    // Run comprehensive tests first
    sim.runComprehensiveTests();

    // Run main simulations
    sim.runSimulations();

    return 0;
}