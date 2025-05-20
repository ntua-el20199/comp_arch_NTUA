#ifndef CACHE_H
#define CACHE_H

#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>
#include <sstream>   // For std::ostringstream
#include <cassert>   // For assert()

// Pin types (ADDRINT, UINT32, UINT64, etc.) are assumed to be available
// from includes in simulator.cpp (pin.H)

/*****************************************************************************/
/* Policy about L2 inclusion of L1's content                                 */
/*****************************************************************************/
#ifndef L2_INCLUSIVE
#  define L2_INCLUSIVE 1
#endif
/*****************************************************************************/


/*****************************************************************************/
/* Cache allocation strategy on stores                                       */
/*****************************************************************************/
enum {
    STORE_ALLOCATE = 0,
    STORE_NO_ALLOCATE
};
#ifndef STORE_ALLOCATION
#  define STORE_ALLOCATION STORE_ALLOCATE
#endif
/*****************************************************************************/

// KILO, dec2str, IsPowerOf2, FloorLog2 are expected from globals.h

// Renamed helper functions to avoid conflict with Pin's utilities
static inline std::string cache_ljstr(const std::string& s, UINT32 width) {
    std::string t = s;
    if (t.length() < width) t.append(width - t.length(), ' ');
    return t;
}

static inline std::string cache_fltstr(double v, INT32 w, INT32 p) {
    std::ostringstream o;
    o.width(w);
    o.precision(p);
    o.setf(std::ios::fixed);
    o << v;
    return o.str();
}


typedef UINT64 CACHE_STATS; 

class CACHE_TAG
{
  private:
    ADDRINT _tag;

  public:
    CACHE_TAG(ADDRINT tag = 0) : _tag(tag) {}
    bool operator==(const CACHE_TAG &right) const { return _tag == right._tag; }
    operator ADDRINT() const { return _tag; }
};
CACHE_TAG INVALID_TAG(-1);


namespace CACHE_SET
{

class LRU
{
  protected:
    std::vector<CACHE_TAG> _tags;
    UINT32 _associativity;

 public:
    LRU(UINT32 associativity = 8) : _associativity(associativity) {
        _tags.clear();
    }

    VOID SetAssociativity(UINT32 associativity) {
        _associativity = associativity;
        _tags.clear();
    }
    UINT32 GetAssociativity() const { return _associativity; }

    std::string Name() const { return "LRU"; }
    
    bool Find(CACHE_TAG tag) {
        for (std::vector<CACHE_TAG>::iterator it = _tags.begin();
             it != _tags.end(); ++it) {
            if (*it == tag) { 
                CACHE_TAG found_tag = *it; 
                _tags.erase(it);
                _tags.push_back(found_tag); 
                return true;
            }
        }
        return false;
    }

    CACHE_TAG Replace(CACHE_TAG tag) {
        CACHE_TAG ret = INVALID_TAG;
        if (_associativity > 0) { // Proceed only if associativity is valid
            if (_tags.size() >= _associativity) { 
                ret = *_tags.begin();
                _tags.erase(_tags.begin());
            }
            _tags.push_back(tag);
        } else {
             // Cannot add to zero-associativity cache, return the tag itself or handle error
             return tag; 
        }
        return ret;
    }
    
    VOID DeleteIfPresent(CACHE_TAG tag) {
        for (std::vector<CACHE_TAG>::iterator it = _tags.begin();
             it != _tags.end(); ++it) {
            if (*it == tag) { 
                _tags.erase(it);
                break;
            }
        }
    }
};

class LFU
{
  protected:
    struct LfuLine {
        CACHE_TAG tag;
        UINT32 frequency;
        LfuLine(CACHE_TAG t, UINT32 f) : tag(t), frequency(f) {}
    };
    std::vector<LfuLine> _lines;
    UINT32 _associativity;

  public:
    LFU(UINT32 associativity = 8) : _associativity(associativity) {
        _lines.clear();
    }

    VOID SetAssociativity(UINT32 associativity) {
        _associativity = associativity;
        _lines.clear();
    }
    UINT32 GetAssociativity() const {
        return _associativity;
    }
    std::string Name() const { return "LFU"; }

    bool Find(CACHE_TAG tag_to_find) {
        for (auto& line : _lines) {
            if (line.tag == tag_to_find) {
                line.frequency++; 
                return true;    
            }
        }
        return false; 
    }

    CACHE_TAG Replace(CACHE_TAG new_tag) {
        CACHE_TAG evicted_tag = INVALID_TAG;

        if (_associativity == 0) return new_tag;

        if (_lines.size() < _associativity) {
            _lines.emplace_back(new_tag, 1);
        } else {
            UINT32 min_frequency = std::numeric_limits<UINT32>::max();
            std::vector<LfuLine>::iterator victim_it = _lines.end(); 

            for (auto it = _lines.begin(); it != _lines.end(); ++it) {
                if (it->frequency < min_frequency) {
                    min_frequency = it->frequency;
                    victim_it = it;
                }
            }
            
            if (victim_it != _lines.end()) { 
                evicted_tag = victim_it->tag;
                victim_it->tag = new_tag;
                victim_it->frequency = 1; 
            } else if (!_lines.empty()) { 
                evicted_tag = _lines.begin()->tag;
                _lines.begin()->tag = new_tag;
                _lines.begin()->frequency = 1;
            } else {
                 _lines.emplace_back(new_tag, 1); 
            }
        }
        return evicted_tag;
    }

    VOID DeleteIfPresent(CACHE_TAG tag_to_delete) {
        for (auto it = _lines.begin(); it != _lines.end(); ++it) {
            if (it->tag == tag_to_delete) {
                _lines.erase(it);
                break; 
            }
        }
    }
};

} // namespace CACHE_SET


template <class SET>
class TWO_LEVEL_CACHE
{
  public:
    typedef enum {
        ACCESS_TYPE_LOAD,
        ACCESS_TYPE_STORE,
        ACCESS_TYPE_NUM
    } ACCESS_TYPE;

  private:
    enum {
        HIT_L1 = 0,
        HIT_L2,
        MISS_L2,
        ACCESS_RESULT_NUM
    };

    static const UINT32 HIT_MISS_NUM = 2;
    CACHE_STATS _l1_access[ACCESS_TYPE_NUM][HIT_MISS_NUM];
    CACHE_STATS _l2_access[ACCESS_TYPE_NUM][HIT_MISS_NUM];
    UINT32 _latencies[ACCESS_RESULT_NUM];
    SET *_l1_sets;
    SET *_l2_sets;
    const std::string _name;
    const UINT32 _l1_cacheSize;
    const UINT32 _l2_cacheSize;
    const UINT32 _l1_blockSize;
    const UINT32 _l2_blockSize;
    const UINT32 _l1_associativity;
    const UINT32 _l2_associativity;
    const UINT32 _l1_lineShift; 
    const UINT32 _l2_lineShift;
    const UINT32 _l1_setIndexMask; 
    const UINT32 _l2_setIndexMask;
    const UINT32 _l2_prefetch_lines;

    CACHE_STATS L1SumAccess(bool hit) const {
        CACHE_STATS sum = 0;
        for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
            sum += _l1_access[accessType][hit];
        return sum;
    }
    CACHE_STATS L2SumAccess(bool hit) const {
        CACHE_STATS sum = 0;
        for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
            sum += _l2_access[accessType][hit];
        return sum;
    }

    UINT32 L1NumSets() const { return (_l1_associativity > 0) ? (_l1_setIndexMask + 1) : 0; }
    UINT32 L2NumSets() const { return (_l2_associativity > 0) ? (_l2_setIndexMask + 1) : 0; }

    UINT32 L1CacheSize() const { return _l1_cacheSize; }
    UINT32 L2CacheSize() const { return _l2_cacheSize; }
    UINT32 L1BlockSize() const { return _l1_blockSize; }
    UINT32 L2BlockSize() const { return _l2_blockSize; }
    UINT32 L1Associativity() const { return _l1_associativity; }
    UINT32 L2Associativity() const { return _l2_associativity; }
    UINT32 L1LineShift() const { return _l1_lineShift; }
    UINT32 L2LineShift() const { return _l2_lineShift; }
    UINT32 L1SetIndexMask() const { return _l1_setIndexMask; }
    UINT32 L2SetIndexMask() const { return _l2_setIndexMask; }

    VOID SplitAddress(const ADDRINT addr, UINT32 lineShift, UINT32 setIndexMask,
                      CACHE_TAG & tag, UINT32 & setIndex) const {
        tag = addr >> lineShift;
        UINT32 num_sets = setIndexMask + 1;
        if (num_sets > 0 && IsPowerOf2(num_sets)) { 
            setIndex = tag & setIndexMask;
            tag = tag >> FloorLog2(num_sets);
        } else { 
            setIndex = 0;
        }
    }

  public:
    TWO_LEVEL_CACHE(std::string name,
                UINT32 l1CacheSize, UINT32 l1BlockSize, UINT32 l1Associativity,
                UINT32 l2CacheSize, UINT32 l2BlockSize, UINT32 l2Associativity,
                UINT32 l2PrefetchLines,
                UINT32 l1HitLatency = 1, UINT32 l2HitLatency = 15,
                UINT32 l2MissLatency = 250);

    CACHE_STATS L1Hits(ACCESS_TYPE accessType) const { return _l1_access[accessType][true];}
    CACHE_STATS L2Hits(ACCESS_TYPE accessType) const { return _l2_access[accessType][true];}
    CACHE_STATS L1Misses(ACCESS_TYPE accessType) const { return _l1_access[accessType][false];}
    CACHE_STATS L2Misses(ACCESS_TYPE accessType) const { return _l2_access[accessType][false];}
    CACHE_STATS L1Accesses(ACCESS_TYPE accessType) const { return L1Hits(accessType) + L1Misses(accessType);}
    CACHE_STATS L2Accesses(ACCESS_TYPE accessType) const { return L2Hits(accessType) + L2Misses(accessType);}
    CACHE_STATS L1Hits() const { return L1SumAccess(true);}
    CACHE_STATS L2Hits() const { return L2SumAccess(true);}
    CACHE_STATS L1Misses() const { return L1SumAccess(false);}
    CACHE_STATS L2Misses() const { return L2SumAccess(false);}
    CACHE_STATS L1Accesses() const { return L1Hits() + L1Misses();}
    CACHE_STATS L2Accesses() const { return L2Hits() + L2Misses();}

    std::string StatsLong(std::string prefix = "") const;
    std::string PrintCache(std::string prefix = "") const;
    UINT32 Access(ADDRINT addr, ACCESS_TYPE accessType);
};

template <class SET>
TWO_LEVEL_CACHE<SET>::TWO_LEVEL_CACHE(
                std::string name,
                UINT32 l1CacheSize, UINT32 l1BlockSize, UINT32 l1Associativity,
                UINT32 l2CacheSize, UINT32 l2BlockSize, UINT32 l2Associativity,
                UINT32 l2PrefetchLines,
                UINT32 l1HitLatency, UINT32 l2HitLatency, UINT32 l2MissLatency)
  : _name(name),
    _l1_cacheSize(l1CacheSize),
    _l2_cacheSize(l2CacheSize),
    _l1_blockSize(l1BlockSize),
    _l2_blockSize(l2BlockSize),
    _l1_associativity(l1Associativity),
    _l2_associativity(l2Associativity),
    _l1_lineShift(l1BlockSize > 0 ? FloorLog2(l1BlockSize) : 0),
    _l2_lineShift(l2BlockSize > 0 ? FloorLog2(l2BlockSize) : 0),
    _l1_setIndexMask((l1Associativity > 0 && l1BlockSize > 0 && l1CacheSize >= (l1Associativity * l1BlockSize)) ? (l1CacheSize / (l1Associativity * l1BlockSize)) - 1 : 0),
    _l2_setIndexMask((l2Associativity > 0 && l2BlockSize > 0 && l2CacheSize >= (l2Associativity * l2BlockSize)) ? (l2CacheSize / (l2Associativity * l2BlockSize)) - 1 : 0),
    _l2_prefetch_lines(l2PrefetchLines)
{
    assert(l1BlockSize == 0 || IsPowerOf2(l1BlockSize)); // Allow 0 block size for disabled cache
    assert(l2BlockSize == 0 || IsPowerOf2(l2BlockSize));
    UINT32 l1_num_sets = L1NumSets();
    UINT32 l2_num_sets = L2NumSets();
    if (l1_num_sets > 0) assert(IsPowerOf2(l1_num_sets)); 
    if (l2_num_sets > 0) assert(IsPowerOf2(l2_num_sets));

    assert(_l1_cacheSize <= _l2_cacheSize || _l2_cacheSize == 0);
    assert(_l1_blockSize <= _l2_blockSize || _l2_blockSize == 0);

    _l1_sets = (l1_num_sets > 0) ? new SET[l1_num_sets] : nullptr;
    _l2_sets = (l2_num_sets > 0) ? new SET[l2_num_sets] : nullptr;

    _latencies[HIT_L1] = l1HitLatency;
    _latencies[HIT_L2] = l2HitLatency;
    _latencies[MISS_L2] = l2MissLatency;

    if (_l1_sets) {
      for (UINT32 i = 0; i < l1_num_sets; i++)
          _l1_sets[i].SetAssociativity(_l1_associativity);
    }
    if (_l2_sets) {
      for (UINT32 i = 0; i < l2_num_sets; i++)
          _l2_sets[i].SetAssociativity(_l2_associativity);
    }

    for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++) {
        _l1_access[accessType][false] = 0;
        _l1_access[accessType][true] = 0;
        _l2_access[accessType][false] = 0;
        _l2_access[accessType][true] = 0;
    }
}

template <class SET>
std::string TWO_LEVEL_CACHE<SET>::StatsLong(std::string prefix) const
{
    const UINT32 headerWidth = 19;
    const UINT32 numberWidth = 12;
    std::string out_str;
    
    out_str += prefix + "L1 Cache Stats:" + "\n";
    for (UINT32 i = 0; i < ACCESS_TYPE_NUM; i++) {
        const ACCESS_TYPE accessType = ACCESS_TYPE(i);
        UINT64 currentL1Accesses = L1Accesses(accessType);
        double l1HitRate = (currentL1Accesses == 0) ? 0.0 : (100.0 * L1Hits(accessType) / currentL1Accesses);
        double l1MissRate = (currentL1Accesses == 0) ? 0.0 : (100.0 * L1Misses(accessType) / currentL1Accesses);
        std::string type(accessType == ACCESS_TYPE_LOAD ? "L1-Load" : "L1-Store");
        out_str += prefix + cache_ljstr(type + "-Hits:      ", headerWidth) + dec2str(L1Hits(accessType), numberWidth) + "  " + cache_fltstr(l1HitRate, 2, 6) + "%\n";
        out_str += prefix + cache_ljstr(type + "-Misses:    ", headerWidth) + dec2str(L1Misses(accessType), numberWidth) + "  " + cache_fltstr(l1MissRate, 2, 6) + "%\n";
        out_str += prefix + cache_ljstr(type + "-Accesses:  ", headerWidth) + dec2str(currentL1Accesses, numberWidth) + "  " + cache_fltstr( (currentL1Accesses==0)?0.0:100.0, 2, 6) + "%\n";
        out_str += prefix + "\n";
    }
    UINT64 totalL1Acc = L1Accesses();
    double totalL1HitRate = (totalL1Acc == 0) ? 0.0 : (100.0 * L1Hits() / totalL1Acc);
    double totalL1MissRate = (totalL1Acc == 0) ? 0.0 : (100.0 * L1Misses() / totalL1Acc);
    out_str += prefix + cache_ljstr("L1-Total-Hits:      ", headerWidth) + dec2str(L1Hits(), numberWidth) + "  " + cache_fltstr(totalL1HitRate, 2, 6) + "%\n";
    out_str += prefix + cache_ljstr("L1-Total-Misses:    ", headerWidth) + dec2str(L1Misses(), numberWidth) + "  " + cache_fltstr(totalL1MissRate, 2, 6) + "%\n";
    out_str += prefix + cache_ljstr("L1-Total-Accesses:  ", headerWidth) + dec2str(totalL1Acc, numberWidth) + "  " + cache_fltstr((totalL1Acc==0)?0.0:100.0, 2, 6) + "%\n";
    out_str += "\n";

    out_str += prefix + "L2 Cache Stats:" + "\n";
    for (UINT32 i = 0; i < ACCESS_TYPE_NUM; i++) {
        const ACCESS_TYPE accessType = ACCESS_TYPE(i);
        UINT64 currentL2Accesses = L2Accesses(accessType);
        double l2HitRate = (currentL2Accesses == 0) ? 0.0 : (100.0 * L2Hits(accessType) / currentL2Accesses);
        double l2MissRate = (currentL2Accesses == 0) ? 0.0 : (100.0 * L2Misses(accessType) / currentL2Accesses);
        std::string type(accessType == ACCESS_TYPE_LOAD ? "L2-Load" : "L2-Store");
        out_str += prefix + cache_ljstr(type + "-Hits:      ", headerWidth) + dec2str(L2Hits(accessType), numberWidth) + "  " + cache_fltstr(l2HitRate, 2, 6) + "%\n";
        out_str += prefix + cache_ljstr(type + "-Misses:    ", headerWidth) + dec2str(L2Misses(accessType), numberWidth) + "  " + cache_fltstr(l2MissRate, 2, 6) + "%\n";
        out_str += prefix + cache_ljstr(type + "-Accesses:  ", headerWidth) + dec2str(currentL2Accesses, numberWidth) + "  " + cache_fltstr((currentL2Accesses==0)?0.0:100.0, 2, 6) + "%\n";
        out_str += prefix + "\n";
    }
    UINT64 totalL2Acc = L2Accesses();
    double totalL2HitRate = (totalL2Acc == 0) ? 0.0 : (100.0 * L2Hits() / totalL2Acc);
    double totalL2MissRate = (totalL2Acc == 0) ? 0.0 : (100.0 * L2Misses() / totalL2Acc);
    out_str += prefix + cache_ljstr("L2-Total-Hits:      ", headerWidth) + dec2str(L2Hits(), numberWidth) + "  " + cache_fltstr(totalL2HitRate, 2, 6) + "%\n";
    out_str += prefix + cache_ljstr("L2-Total-Misses:    ", headerWidth) + dec2str(L2Misses(), numberWidth) + "  " + cache_fltstr(totalL2MissRate, 2, 6) + "%\n";
    out_str += prefix + cache_ljstr("L2-Total-Accesses:  ", headerWidth) + dec2str(totalL2Acc, numberWidth) + "  " + cache_fltstr((totalL2Acc==0)?0.0:100.0, 2, 6) + "%\n";
    out_str += prefix + "\n";
    return out_str;
}

template <class SET>
std::string TWO_LEVEL_CACHE<SET>::PrintCache(std::string prefix) const
{
    std::string out_str;
    out_str += prefix + "--------\n";
    out_str += prefix + _name + "\n";
    out_str += prefix + "--------\n";
    if (L1Associativity() > 0) {
        out_str += prefix + "  L1-Data Cache:\n";
        out_str += prefix + "    Size(KB):       " + dec2str(this->L1CacheSize()/KILO, 5) + "\n";
        out_str += prefix + "    Block Size(B):  " + dec2str(this->L1BlockSize(), 5) + "\n";
        out_str += prefix + "    Associativity:  " + dec2str(this->L1Associativity(), 5) + "\n";
        out_str += prefix + "    Sets:           " + dec2str(this->L1NumSets(), 5) + " - " + (_l1_sets && L1NumSets() > 0 ? _l1_sets[0].Name() : "N/A") + "\n";
        out_str += prefix + "\n";
    }
    if (L2Associativity() > 0) {
        out_str += prefix + "  L2-Data Cache:\n";
        out_str += prefix + "    Size(KB):       " + dec2str(this->L2CacheSize()/KILO, 5) + "\n";
        out_str += prefix + "    Block Size(B):  " + dec2str(this->L2BlockSize(), 5) + "\n";
        out_str += prefix + "    Associativity:  " + dec2str(this->L2Associativity(), 5) + "\n";
        out_str += prefix + "    Sets:           " + dec2str(this->L2NumSets(), 5) + " - " + (_l2_sets && L2NumSets() > 0 ? _l2_sets[0].Name() : "N/A") + "\n";
        out_str += prefix + "\n";
    }
    out_str += prefix + "Latencies: " + dec2str(_latencies[HIT_L1], 4) + " "
                                  + dec2str(_latencies[HIT_L2], 4) + " "
                                  + dec2str(_latencies[MISS_L2], 4) + "\n";
    out_str += prefix + "Store_allocation: " + (STORE_ALLOCATION == STORE_ALLOCATE ? "Yes" : "No") + "\n";
    out_str += prefix + "L2_inclusive: " + (L2_INCLUSIVE == 1 ? "Yes" : "No") + "\n";
    out_str += "\n";
    return out_str;
}

template <class SET>
UINT32 TWO_LEVEL_CACHE<SET>::Access(ADDRINT addr, ACCESS_TYPE accessType)
{
    CACHE_TAG l1Tag, l2Tag;
    UINT32 l1SetIndex = 0, l2SetIndex = 0; 
    bool l1Hit = false; 
    bool l2Hit = false; 
    UINT32 cycles = 0;

    // L1 Access
    if (L1Associativity() > 0 && _l1_sets && L1NumSets() > 0) {
        SplitAddress(addr, L1LineShift(), L1SetIndexMask(), l1Tag, l1SetIndex);
        SET & l1Set = _l1_sets[l1SetIndex];
        l1Hit = l1Set.Find(l1Tag);
        _l1_access[accessType][l1Hit]++;
    } else {
        l1Hit = false; 
    }
    cycles = _latencies[HIT_L1];

    // L1 Miss Handling
    if (!l1Hit) { 
        if (L1Associativity() > 0 && _l1_sets && L1NumSets() > 0) {
            if (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == STORE_ALLOCATE) {
                 _l1_sets[l1SetIndex].Replace(l1Tag); // l1SetIndex is valid if L1NumSets > 0
            }
        }

        // L2 Access
        if (L2Associativity() > 0 && _l2_sets && L2NumSets() > 0) {
            SplitAddress(addr, L2LineShift(), L2SetIndexMask(), l2Tag, l2SetIndex);
            SET & l2Set = _l2_sets[l2SetIndex];
            l2Hit = l2Set.Find(l2Tag);
            _l2_access[accessType][l2Hit]++; 
            cycles += _latencies[HIT_L2];

            if (!l2Hit) { // L2 Miss
                CACHE_TAG l2_evicted_tag = l2Set.Replace(l2Tag);
                cycles += _latencies[MISS_L2]; 

                if ((L2_INCLUSIVE == 1) && !(l2_evicted_tag == INVALID_TAG) && 
                    (L1Associativity() > 0 && _l1_sets && L1NumSets() > 0) && L1BlockSize() > 0) { // Ensure L1 is active and L1BlockSize > 0
                    
                    ADDRINT num_l2_set_bits = (L2NumSets() > 0) ? FloorLog2(L2NumSets()) : 0;
                    ADDRINT reconstructed_l2_block_addr = (ADDRINT(l2_evicted_tag) << num_l2_set_bits) | l2SetIndex;
                    reconstructed_l2_block_addr <<= L2LineShift();

                    for (UINT32 offset_in_l2_block = 0; offset_in_l2_block < L2BlockSize(); offset_in_l2_block += L1BlockSize()) {
                        ADDRINT l1_block_addr_to_check = reconstructed_l2_block_addr + offset_in_l2_block;
                        CACHE_TAG l1_tag_to_invalidate;
                        UINT32 l1_set_idx_to_invalidate = 0; 
                        SplitAddress(l1_block_addr_to_check, L1LineShift(), L1SetIndexMask(), l1_tag_to_invalidate, l1_set_idx_to_invalidate);
                        
                        if (l1_set_idx_to_invalidate < L1NumSets()) { 
                           _l1_sets[l1_set_idx_to_invalidate].DeleteIfPresent(l1_tag_to_invalidate);
                        }
                    }
                }
            }
        } else { // L2 is disabled or L1 hit (covered), or L1 miss and L2 disabled
             cycles += _latencies[HIT_L2]; // This accounts for the L2 check latency.
             cycles += _latencies[MISS_L2]; // If L2 is effectively skipped, this is main memory.
        }
    }
    return cycles;
}

#endif // CACHE_H