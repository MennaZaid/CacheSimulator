# Project 2: Two-Level Cache Performance Analysis Report  
**Summer 2025 – CSCE 2303: Computer Organization and Assembly Language Programming**  
**Dr. Mohamed Shalan – July 2025**

## Team Members
- **Mennatallah Zaid** – 900232367  
- **Mennatallah Essam** – 900223396  
- **Ahmed Mohamed** – 900221597

---

## 📌 Introduction

This project presents a performance analysis of a two-level set-associative cache simulator. The simulator models L1 and L2 caches with varying L1 line sizes to observe their effect on Cycles Per Instruction (CPI). Five distinct memory access patterns were used to emulate different workload behaviors.

---

## ⚙️ Methodology

### Cache Configuration
- **L1 Cache:** 16KB, 4-way, 1-cycle hit time, line size: 16B/32B/64B/128B  
- **L2 Cache:** 128KB, 8-way, 10-cycle hit time, 64B line size  
- **Main Memory:** 64GB address space, 50-cycle access latency  

### Simulation Parameters
- 1,000,000 instructions per run  
- 35% of instructions access memory (50% reads, 50% writes)  
- Write-back policy with random replacement  
- Non-memory instructions cost 1 cycle  

### Memory Access Patterns
- **memGen1:** Sequential access across 64GB  
- **memGen2:** Random access within 24KB  
- **memGen3:** Random access across 64GB  
- **memGen4:** Sequential access within 4KB  
- **memGen5:** Strided access (32B stride) within 1MB  

---

## ✅ Testing and Validation

### Test Coverage
- 18 test cases across 5 categories  
- 100% pass rate validating:  
  - Hit/miss behavior  
  - L1/L2 integration  
  - Writeback propagation  
  - Access patterns  
  - Statistical metrics

### Test Categories
- **Basic Functionality:** Hit/miss detection, line alignment, tag matching  
- **Hierarchy Behavior:** L1→L2→DRAM flow, timing validation  
- **Memory Generators:** Pattern correctness and bounds  
- **Performance Metrics:** CPI, hit rate accuracy  
- **Stress Tests:** High-volume stability, reset behavior, associativity validation  

---

## 📊 Results Summary

| Generator  | 16B Line | 32B Line | 64B Line | 128B Line |
|------------|----------|----------|----------|-----------|
| memGen1    | 1.6993   | 1.4883   | 1.3807   | 1.1902    |
| memGen2    | 3.0557   | 3.0582   | 3.0631   | 3.0648    |
| memGen3    | 23.6813  | 23.7544  | 23.7230  | 23.7644   |
| memGen4    | 1.0058   | 1.0045   | 1.0038   | 1.0019    |
| memGen5    | 14.9849  | 14.9935  | 12.8220  | 7.0622    |

---

## 📈 Data Analysis

### Key Findings

- **Spatial Locality:** memGen1 and memGen5 show significant CPI reductions as L1 line size increases  
- **Working Set Fit:** memGen4 (4KB set) achieves optimal CPI ≈ 1.00 across all line sizes  
- **Random Access:** memGen2 and memGen3 unaffected by line size due to lack of locality  
- **Stride Optimization:** memGen5 benefits most from 64B–128B lines due to alignment with stride  

---

## 💡 Discussion

### Performance Insights

- **memGen4:** Ideal behavior—working set fits in L1 cache  
- **memGen1:** Sequential access exploits spatial locality well  
- **memGen2:** L2-bound; random access limits line size effectiveness  
- **memGen5:** Performance boosts when stride aligns with line size  
- **memGen3:** High CPI due to non-local, DRAM-heavy behavior  

### Cache Design Implications

1. **Working Set Size Matters:** Fit in L1/L2 = low CPI  
2. **Access Pattern Dictates Gains:** Sequential > Strided > Random  
3. **Line Size Must Match Access Pattern:** Bigger isn't always better  
4. **Hierarchy Matters:** L2 helps mitigate DRAM penalties if locality exists  

---

## ✅ Conclusions

- Line size tuning is crucial for workloads with spatial locality  
- Cache configuration must be matched to application behavior  
- The two-level cache improves performance—but only for "cache-friendly" programs  
- Random access patterns remain bottlenecked by DRAM latency  

---

## 🔬 Repository

📁 Project Source Code:  
**[https://github.com/MennaZaid/CacheSimulator.git](https://github.com/MennaZaid/CacheSimulator.git)**

---

## 👥 Group Contributions

### Mennatallah Zaid
- Developed memGen1–memGen5  
- Implemented custom RNG  
- Built `Cache` class with set-associative logic  
- Designed and implemented testing framework  
- Contributed to report documentation  

### Ahmed Mohamed
- Implemented `TwoLevelCache` system  
- Handled memory hierarchy, hit/miss logic, and cycle tracking  
- Integrated L1–L2–DRAM pipeline  

### Mennatallah Essam
-Designed the main simulation loop and execution logic

-Implemented memory access tracking and result collection

-Formatted and displayed cache statistics clearly for analysis

-Built the CPI calculation logic based on hit/miss penalties

-Integrated all modules into a working multi-level cache simulation

> **Each member played a key role in building a reliable, flexible, and accurate cache simulator.**

---

