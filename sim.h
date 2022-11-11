#ifndef SIM_CACHE_H
#define SIM_CACHE_H

#include <stdint.h>
#include <stdio.h>

typedef 
struct {
   uint32_t BLOCKSIZE;
   uint32_t L1_SIZE;
   uint32_t L1_ASSOC;
   uint32_t L2_SIZE;
   uint32_t L2_ASSOC;
   uint32_t PREF_N;
   uint32_t PREF_M;
} cache_params_t;

typedef unsigned long long dex; 

typedef struct Stats {
  int access_counter;
  int read;
  int read_miss;
  int write;
  int write_miss;
  int write_back;
  int miss_num;
  float miss_rate;
  int lower_access_counter;
  int replace_num; // Evict old lines
  int fetch_num; // Fetch lower layer
  int prefetch_num; // Prefetch
  int pretch_read;
  int prefetch_write;
} Stats;


typedef struct CacheConfig_ {
    int size;
    int associativity;
    int b_size; // Number of cache sets
    int use_prefetch;
    int pref_N;
    int pref_M;
} CacheConfig;

CacheConfig config_;


class Cache {
public:
    Cache() {}
    ~Cache() {}

    // Sets & Gets
    void SetConfig(CacheConfig cc);
    void GetConfig(CacheConfig &cc) { cc = config_; }
    void SetLower(Cache *ll){lower_lvl = ll;} 
    // Main access process
    void HandleRequest(dex addr,int read);
    void print(int arg_set, int arg_line);
    void SetStats(Stats ss) { stats = ss; }
    void GetStats(Stats &ss) { ss = stats; }

    int tag_bits, idx_bits, offset_bits;
    dex offset_mask, tag_mask, index_mask;
    int set, way, b_s, pref_N, pref_M,size;
    bool **valid, **dirty;
    bool *pref_valid;
    bool prefet_use;
    dex **cache_array;
    dex **prefet;
    int *pref_counter;
    int **counter;

    dex mask(int N);

    dex get_tag(dex addr);
    dex get_idx(dex addr);
    dex get_offset(dex addr);
    dex pref_addr(dex addr);
   
    dex old_tag;

    int find_line(dex arg_tag, int &line);
    void read_hit(dex addr);
    void read_miss(dex addr);
    void write_hit(dex addr);
    void write_miss(dex addr);
    void evict_block(dex addr,int sub_line);
    

    void LRU_update(dex addr, int arg_line);

    bool is_set_full(int arg_set);

    void prefetch(dex addr);
    void prefet_fill(dex addr, int &stream);
    void find_stream_fill(int &stream);
    void LRU_pref_update(int stream);
    int pref_check_hit(dex addr, int stream);
    void pref_hit(dex addr, int stream, int buf_line);
    void pref_miss(dex addr, int &stream);
    void pref_update(dex addr, int &stream);
    dex new_addr(dex addr);
   
    Cache *lower_lvl;
    CacheConfig config_;
    Stats stats;

};

#endif
