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
  int miss_time;
  int access_time; // In nanoseconds
  int lower_access_counter;
  int replace_num; // Evict old lines
  int fetch_num; // Fetch lower layer
  int prefetch_num; // Prefetch
} Stats;

typedef struct Latency {
  int hit_latency; // In nanoseconds
  int bus_latency; // Added to each request
} Latency;

typedef struct CacheConfig_ {
    int size;
    int associativity;
    int b_size; // Number of cache sets
    int use_prefetch;
    int pref_N;
    int pref_M;
} CacheConfig;

CacheConfig config_;

class Storage {
 public:
  Storage() {}
  virtual ~Storage() {}

  // Sets & Gets
  void SetStats(Stats ss) { stats = ss; }
  void GetStats(Stats &ss) { ss = stats; }
  void SetLatency(Latency sl) { latency = sl; }
  void GetLatency(Latency &sl) { sl = latency; }

  // Main access process
  // [in]  addr: access address
  // [in]  bytes: target number of bytes
  // [in]  read: 0|1 for write|read
  // [i|o] content: in|out data
  // [out] hit: 0|1 for miss|hit
  // [out] time: total access time
  virtual void HandleRequest(dex addr, int arg_set, int read, int &hit);

 protected:
  Stats stats;
  Latency latency;
};

Storage *memory;


class Memory: public Storage {
 public:
  Memory() {}
  ~Memory();
  

  // Main access process
  void HandleRequest(dex addr,int set,int read,int &hit);

 private:
  // Memory implement
  dex *content;

};

class Cache: public Storage {
public:
    Cache() {}
    ~Cache() {}

    // Sets & Gets
    void SetConfig(CacheConfig cc);
    void GetConfig(CacheConfig &cc) { cc = config_; }
    void SetLower(Cache *lower_lvl); 
    void SetLower(Storage *memory);
    // Main access process
    void HandleRequest(dex addr, int arg_set,int read,int &hit);
    void print(int arg_set, int arg_line);

private:
    int tag_bits, idx_bits, offset_bits;
    dex offset_mask, tag_mask, index_mask;
    int set, way, b_s, pref_N, pref_M;
    bool **valid, **dirty;
    bool *pref_valid;
    dex **cache_array;
    dex **prefet;
    int *pref_counter;
    int **counter;

    dex mask(int N);

    dex get_tag(dex addr);
    dex get_idx(dex addr);
    dex get_offset(dex addr);

    int find_line(dex arg_tag, int arg_set, int &line);
    void read_hit(dex addr, int arg_set);
    void read_miss(dex addr, int arg_set);
    void write_hit(dex addr, int arg_set);
    void write_miss(dex addr, int arg_set);
    void evict_block(dex addr,int sub_line, int arg_set);

    void LRU_update(int arg_set, int arg_line);

    bool is_set_full(int arg_set);

    void prefetch(dex curr_addr);
    void prefet_fill(dex addr, int &stream);
    void find_stream_fill(int &stream);
    void LRU_pref_update(int stream);
    int pref_check_hit(dex addr, int stream, int buf_line);
    void pref_hit(dex addr, int stream, int buf_line);
    void pref_miss(dex addr, int &stream);
    void pref_update(dex addr, int &stream);
   
    Cache *lower_lvl;
    Storage *memory;
    CacheConfig config_;

};

#endif
