#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <inttypes.h>
#include <math.h>
#include<string.h>
#include<iostream>
#include "sim.h"
#include <bits/stdc++.h>

#define DEBUG 1;


Memory::~Memory()
{
    //printf("Memory destructor.\n");
}

void Memory::HandleRequest(unsigned long long addr, int arg_set,int read,int &hit)
{
#ifdef DEBUG
    printf("  Mem always hit!\n");
#endif
    content= new dex[arg_set];
    hit = 1;
    stats.access_counter += 1;

    if (read == 1)
    {
        for (int i = 0; i < arg_set; i++)
            content[i] = (char)0x0;
    }
}

void Cache::SetConfig(CacheConfig cc)
{
    unsigned int tmp;
    config_ = cc;

    way = cc.associativity;
    b_s = cc.b_size;
    set = cc.size/(b_s*way);
    pref_N= cc.pref_N;
    pref_M= cc.pref_M;
    idx_bits=(int)log2(set);
    offset_bits=(int)log2(b_s);
    tag_bits= 32-idx_bits-offset_bits;
    pref_valid= new bool[pref_N];
    pref_counter=new int[pref_N];
    valid = new bool*[set];
    dirty = new bool*[set];
    cache_array = new dex*[set];
    prefet= new dex*[pref_N];
    counter = new int*[set];
    for (int i = 0; i < set; i++)
    {
        valid[i] = new bool[way];
        dirty[i] = new bool[way];
        cache_array[i] = new dex[way];
        counter[i] = new int[way];
        for (int j = 0; j < way; j++)
        {
            valid[i][j] = false;
            dirty[i][j] = false;
            cache_array[i][j] = 0x0;
            counter[i][j] = 0;
        }
    }
    for(int i=0;i<pref_N;i++){
        pref_valid[i]= false;
        prefet[i]= new dex [pref_M];
        pref_counter[i]= 0;
        for(int j=0;j<pref_M;j++){
            prefet[i][j]= 0x0;
        }
    }
	offset_mask = mask(offset_bits);
	index_mask = mask(idx_bits + offset_bits);
	tag_mask = mask(tag_bits + idx_bits + offset_bits);

#ifdef DEBUG
    printf("set num = %d    ", set);
    printf("way num = %d    ", way);
    printf("Block size = %d\n", b_s);
    printf("tag bits = %d    ", tag_bits);
    printf("idx bits = %d    ", idx_bits);
    printf("offset bits = %d\n", offset_bits);
#endif
}




void Cache::print(int arg_set, int arg_line)
{

    printf("SET %d:\n", arg_set);
    printf("  LINE %d: valid = %5s    dirty = %5s    LRUcounter = %d    tag = %llx\n    content:", arg_line, valid[arg_set][arg_line]?"True":"False", dirty[arg_set][arg_line]?"True":"False", counter[arg_set][arg_line], cache_array[arg_set][arg_line]);
    printf("access = %d\n", stats.access_counter);
    printf("miss = %d\n", stats.miss_num);
    printf("fetch = %d\n", stats.fetch_num);
    printf("replace = %d\n", stats.replace_num);
}



dex mask(int N){
    dex mask = 0;
	for(int i=0; i<N; i++)
	{
		mask <<=1;
		mask |=1;
	}
	return mask;
}

dex Cache :: mask(int N)
{
	dex mask = 0;
	for(int i=0; i<N; i++)
	{
		mask <<=1;
		mask |=1;
	}
	return mask;
}

dex Cache:: get_tag(dex addr) {
    return ((addr & tag_mask)>>(idx_bits  + offset_bits));
    }
dex Cache:: get_idx(dex addr) {
    return ((addr & index_mask)>>(offset_bits));
    }
dex Cache:: get_offset(dex addr) {
    return (addr & offset_mask);
    }



int Cache::find_line(dex addr, int arg_set, int &line)
{
    dex arg_tag;
    arg_tag= get_tag(addr);
    for (int i = 0; i < way; i++)
        if (valid[arg_set][i] && cache_array[arg_set][i] == arg_tag)
        {
            line = i;
            return 1;
        }
    line = -1;
    return 0;
}

void Cache::read_hit(dex addr,int arg_set)
{
    int arg_line=0;
    find_line(addr,arg_set,arg_line);
#ifdef DEBUG
    printf("  Cache location: set %d line %d\n", arg_set, arg_line);
#endif
    printf("It is a read hit\n");
    stats.read++;
    printf("Reads =%d \n", stats.read);
    LRU_update(arg_set, arg_line);
}

void Cache::read_miss(dex addr, int arg_set)
{
    int sub_line = -1, max_cnt = 0;
    bool found = false;
    stats.read_miss++;
    dex arg_tag= get_tag(addr);

    for (int i = 0; i < way; i++){
        if (!valid[arg_set][i]) // To find empty block in cache
        {
            found = true;
            sub_line = i;
            break;
        }
        else if (valid[arg_set][i] && max_cnt < counter[arg_set][i]) // To find LRU
        {
            max_cnt = counter[arg_set][i];
            sub_line = i;
        }
    }
    int line=0;
    int hit = lower_lvl->find_line(addr,lower_lvl->set,line);
    int arg_set_low= addr & mask(lower_lvl->set);

#ifdef DEBUG
    if (found)
        printf("  Find empty: set %d line %d\n", arg_set, sub_line);
    else
        printf("  Replace: set %d line %d\n", arg_set, sub_line);
#endif
    if (!found) // replace(writeback from 1st level to next level)
    {
        if (dirty[arg_set][sub_line])   // write-back
        {
#ifdef DEBUG
            printf("    Write set %d line %d back\n", arg_set, sub_line);
            printf("      Line addr = %llx, line size = %x\n",
                   ((arg_tag << idx_bits) | arg_set) << offset_bits,
                   1 << offset_bits);
#endif      

            stats.write_back++;
            lower_lvl->HandleRequest(addr,arg_set_low, 0,hit);
            stats.lower_access_counter++; // lower access counter is traffic to lower level
            valid[arg_set][sub_line] = true;
            dirty[arg_set][sub_line] = false;
            evict_block(addr,sub_line,arg_set);
            read_hit(addr, arg_set);

            /*if (config_.use_prefetch == 1)
                prefetch(addr);
                */
        }

    }
#ifdef DEBUG
   else{
    printf("    Fetch to set %d line %d\n", arg_set, sub_line);
    printf("      Line addr = %llx, line size = %x\n",
           (addr >> offset_bits) << offset_bits,
           1 << offset_bits);
#endif
    valid[arg_set][sub_line] = true;
    dirty[arg_set][sub_line] = false;
    printf("It is a read miss \n");
    printf("Read misses = %d \n", stats.read_miss);
    lower_lvl->HandleRequest(addr,arg_set_low, 1,hit);
    stats.lower_access_counter++;
    evict_block(addr,sub_line,arg_set);
    read_hit(addr, arg_set);

   /* if (config_.use_prefetch == 1)
        prefetch(addr);
*/
}
}

void Cache::write_hit(dex addr, int arg_set)
{
    int arg_line=0;
    find_line(addr, arg_set, arg_line);
#ifdef DEBUG
    printf("  Cache location: set %d line %d\n", arg_set, arg_line);
#endif      // write back
#ifdef DEBUG
        printf("  Make set %d line %d dirty\n", arg_set, arg_line);
#endif
    dirty[arg_set][arg_line] = true;    // dirty
    valid[arg_set][arg_line] = true;
    cache_array[arg_set][arg_line]= get_tag(addr);
    stats.write++;
    printf("It is a write hit \n");
    printf("Writes are %d \n", stats.write);
    LRU_update(arg_set, arg_line);
}

void Cache::write_miss(dex addr, int arg_set)
{
    int sub_line = -1, max_cnt = 0;
    bool found = false; 
    stats.write_miss++;   
    
    for (int i = 0; i < way; i++){
        if (!valid[arg_set][i]) // empty line found
        {
            found = true;
            sub_line = i;
            break;
        }
        else if (max_cnt < counter[arg_set][i]) // LRU line found
        {
            max_cnt = counter[arg_set][i];
            sub_line = i;
        }
    }
    
    int line=0;
    int hit = lower_lvl->find_line(addr,lower_lvl->set,line);
    int arg_set_low= addr & mask(lower_lvl->set);

#ifdef DEBUG
        if (found)
            printf("  Find empty: set %d line %d\n", arg_set, sub_line);
        else
            printf("  Replace: set %d line %d\n", arg_set, sub_line);
#endif
    if (!found) // LRU
    {
        stats.replace_num++;
        stats.write_back++;
        if (dirty[arg_set][sub_line])   // write-back
        {
#ifdef DEBUG
                printf("    Write set %d line %d back\n", arg_set, sub_line);
                printf("      Line addr = %llx, line size = %x\n",
                       ((cache_array[arg_set][sub_line] << idx_bits) | arg_set) << offset_bits,
                       1 << offset_bits);
#endif
            stats.write_back++;
            lower_lvl->HandleRequest(addr,arg_set_low, 0, hit);
            stats.lower_access_counter++;
            valid[arg_set][sub_line] = true;
            dirty[arg_set][sub_line] = true;
            evict_block(addr,sub_line, arg_set);
            write_hit(addr, arg_set);
           /* if (config_.use_prefetch == 1)
                prefetch(addr);     
                */
        }
    }
    else{
#ifdef DEBUG
        printf("    Fetch to set %d line %d\n", arg_set, sub_line);
        printf("      Line addr = %llx, line size = %x\n",
               (addr >> offset_bits) << offset_bits,
               1 << offset_bits);
#endif
    printf("It is a write miss \n");
    printf("Write miss are = %d \n", stats.write_miss);
    valid[arg_set][sub_line] = true;
    dirty[arg_set][sub_line] = true;
    lower_lvl->HandleRequest(addr,arg_set_low, 1,hit);
    stats.lower_access_counter++;
    write_hit(addr, arg_set);

    /*if (config_.use_prefetch == 1)
        prefetch(addr);
    */
}
}

void Cache::LRU_update(int arg_set, int arg_line)
{
    for (int i = 0; i < way; i++)
        if (i == arg_line) counter[arg_set][i] = 0;
        else counter[arg_set][i]++;
}

void Cache:: evict_block(dex addr, int sub_line, int arg_set){
    int arg_tag=get_tag(addr);
    cache_array[arg_set][sub_line]=arg_tag;
    }

void Cache::HandleRequest(unsigned long long addr,int arg_set,int read,int &hit)
{
    unsigned long long _tag = get_tag(addr);
    unsigned long long _idx = get_idx(addr);
    unsigned long long _offset = get_offset(addr);
    int _line;

    hit = find_line(_tag, _idx, _line);

    if (hit == 0 && is_set_full(_idx))
    {    
        int arg_set_low= addr & mask(lower_lvl->set);
        int arg_line_low=0;
        int lower_hit= find_line(addr, arg_set_low, arg_line_low);    
        lower_lvl->HandleRequest(addr, arg_set_low, read,lower_hit);
        if(read=0){
            stats.write_miss++;
        }
        else{
            stats.read_miss++;
        }
        stats.access_counter++;
        LRU_update(arg_set,_line);
        return;
    }

    if (read == 1)  // read
    {
        if (hit == 1)   // read hit
        {
#ifdef DEBUG
            printf("  Read hit!");
#endif
            //stats.read++;
            stats.access_counter++;
            read_hit(addr, _idx);
#ifdef DEBUG
            //print(_idx, _line);
#endif
            return;
        }
        else    // read miss
        {
#ifdef DEBUG
            printf("  Read miss!");
#endif
            stats.access_counter++;
            read_miss(addr, _idx);
#ifdef DEBUG
            //print(_idx,_line);
#endif
            return;
        }
    }
    else    // write
    {
        if (hit == 1)   // write hit
        {
#ifdef DEBUG
            printf("  Write hit!");
#endif
            
            stats.access_counter++;
            //stats.write++;
            write_hit(addr, _idx);
#ifdef DEBUG
            //print(_idx,_line);
#endif
            return;
        }
        else    // write miss
        {
#ifdef DEBUG
            printf("  Write miss!");
#endif
            stats.access_counter++;
            //stats.write_miss++;
            write_miss(addr, _idx);
#ifdef DEBUG
            //print(_idx,_line);
#endif
            return;
        }
    }
}

bool Cache::is_set_full(int arg_set)
{
    for (int i = 0; i < way; i++)
        if (!valid[arg_set][i])
            return false;
    return true;
}


void Cache::prefetch(unsigned long long addr)
{ 
    int stream =-1;
    int buf_line=-1;
    int prefet_hit=0;
    prefet_hit=pref_check_hit(addr,stream,buf_line);
    int arg_set_low= addr & mask(lower_lvl->set);
    int wb_hit=0;
    int line=0;
    int arg_tag_lower= lower_lvl->get_tag(addr);
    wb_hit=find_line(arg_tag_lower,arg_set_low,line);
    int already_in_cache = -1;
    find_line(get_tag(addr), get_idx(addr), already_in_cache);
    if (already_in_cache != -1) // already in cache, don't prefetch
    {  
        if(prefet_hit==1){
            pref_hit(addr,stream,buf_line);
        }
    }
    
    else{

    if(prefet_hit==1){
    stats.prefetch_num++;
    int arg_set = get_idx(addr);
    int sub_line = -1, max_cnt = 0;
    bool found = false;

    for (int i = 0; i < way; i++)
        if (!valid[arg_set][i]) // empty line found
        {
            found = true;
            sub_line = i;
            break;
        }
        else if (valid[arg_set][i] && max_cnt < counter[arg_set][i]) // LRU line found
        {
            max_cnt = counter[arg_set][i];
            sub_line = i;
        }


    if (!found) // replace
    {
        if (dirty[arg_set][sub_line])   // write-back
        {
            lower_lvl->HandleRequest(addr,arg_set,0, wb_hit);
        }
    }

    valid[arg_set][sub_line] = true;
    dirty[arg_set][sub_line] = false;
    cache_array[arg_set][sub_line] = get_tag(addr);
    stats.prefetch_num++;

    int lower_hit = 0;
    lower_lvl->HandleRequest(addr, arg_set, 1,wb_hit);
    pref_hit(addr, stream, buf_line);
    LRU_pref_update(stream);
    } 
    else{
        pref_miss(addr, stream);
        LRU_pref_update(stream);
    }
}
}

void Cache:: find_stream_fill(int &stream){
    int max_count=0;
    for(int i=0;i<pref_N;i++){
        if(!pref_valid[i]){
            stream=i;
            break;
        }
        else if(pref_valid[i] && max_count<pref_counter[i]){
            max_count=pref_counter[i];           //LRU stream found
            stream=i;
        }
    }
    stream = -1;
}

void Cache:: prefet_fill(dex addr, int &stream){
    for(int i=0;i<pref_M;i++){
        prefet[stream][i]= addr+b_s*i;
    }
}


void Cache:: LRU_pref_update(int stream){
    for(int i=0;i<pref_N;i++){
        if(i==stream) pref_counter[i]=0;
        else pref_counter[i]++;
    }  
}

int Cache:: pref_check_hit(dex addr, int stream, int buf_line){
    int min_num= pref_counter[0];
    int hit=0;
    for(int i=0;i<pref_N;i++){
        if(pref_valid[i] && min_num>pref_counter[i]){
            stream =i;
            min_num=pref_counter[i];
            for(int j=0;j<pref_M;j++){
                if(prefet[i][j]==addr){
                   hit++; 
                   buf_line=j;
                }
            }
        }
    }
    if(hit ==0){
        stream=-1;
        buf_line=-1;
        return 0;
    }
    else{
        return 1;
    }
}

void Cache:: pref_hit(dex addr, int stream, int buf_line){
    for(int i=buf_line;i<pref_M-1;i++){
        prefet[stream][i]=prefet[stream][i+1];
    }
    prefet[stream][pref_M]= prefet[stream][pref_M-1]+b_s;
}

void Cache:: pref_miss(dex addr, int &stream){
    find_stream_fill(stream);
    prefet_fill(addr, stream);
}

/*  "argc" holds the number of command-line arguments.
    "argv[]" holds the arguments themselves.

    Example:
    ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
    argc = 9
    argv[0] = "./sim"
    argv[1] = "32"
    argv[2] = "8192"
    ... and so on
*/
int main (int argc, char *argv[]) {
   FILE *fp;			// File pointer.
   char *trace_file;		// This variable holds the trace file name.
   cache_params_t params;	// Look at the sim.h header file for the definition of struct cache_params_t.
   char rw;			// This variable holds the request's type (read or write) obtained from the trace.
   uint32_t addr;		// This variable holds the request's address obtained from the trace.
				// The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.

   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9) {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }
        
   // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
   params.BLOCKSIZE = (uint32_t) atoi(argv[1]);
   params.L1_SIZE   = (uint32_t) atoi(argv[2]);
   params.L1_ASSOC  = (uint32_t) atoi(argv[3]);
   params.L2_SIZE   = (uint32_t) atoi(argv[4]);
   params.L2_ASSOC  = (uint32_t) atoi(argv[5]);
   params.PREF_N    = (uint32_t) atoi(argv[6]);
   params.PREF_M    = (uint32_t) atoi(argv[7]);
   trace_file       = argv[8];
    int read;
    Memory m;
    Cache l1, l2;
    CacheConfig L1, L2;
    int arg_set;
    L1.size= params.L1_SIZE;
    L1.associativity=params.L1_ASSOC;
    L1.b_size=params.BLOCKSIZE;
    L2.size=params.L2_SIZE;
    L2.associativity=params.L2_ASSOC;
    L2.b_size=params.BLOCKSIZE;
    if(!params.PREF_N){
        L1.use_prefetch=0;
        L2.use_prefetch=0;
        L1.pref_M=0;
        L1.pref_N=0;
        L2.pref_M=0;
        L2.pref_N=0;
    }
    else if(params.L2_SIZE==0 && params.PREF_N){
        L1.use_prefetch=1;
        L2.use_prefetch=0;
        L1.pref_N= params.PREF_N;
        L1.pref_M= params.PREF_M;
        L2.pref_N=0;
        L2.pref_M=0;
    }
    else{
        L1.use_prefetch=0;
        L2.use_prefetch=1;
        L1.pref_N=0;
        L1.pref_M=0;
        L2.pref_N=params.PREF_N;
        L2.pref_M= params.PREF_M;
    }

    l1.SetConfig(L1);
    l2.SetConfig(L2);

    if(params.L2_SIZE==0){
        l1.SetLower(&m);
    }
    else{
        l1.SetLower(&l2);
        l2.SetLower(&m);
    }

    Stats s;
    s.access_time = 0;
    s.miss_num = 0;
    s.access_counter = 0;
    s.read=0;
    s.write=0;
    s.read_miss=0;
    s.write_miss=0;
    s.miss_num=0;
    s.lower_access_counter = 0;
    s.fetch_num = 0;
    s.replace_num = 0;
    s.prefetch_num =0;

    m.SetStats(s);
    l1.SetStats(s);
    l2.SetStats(s);

    Latency ml;
    ml.bus_latency = 0;
    ml.hit_latency = 100;
    m.SetLatency(ml);
    
    Latency ll;
    ll.bus_latency = 0;
    ll.hit_latency = 3;
    l1.SetLatency(ll);

    ll.bus_latency = 6;
    ll.hit_latency = 4;
    l2.SetLatency(ll);



   // Open the trace file for reading.
   fp = fopen(trace_file, "r");
   if (fp == (FILE *) NULL) {
      // Exit with an error if file open failed.
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }
    
   // Print simulator configuration.
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n", trace_file);
   printf("===================================\n");

   // Read requests from the trace file and echo them back.
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2) {	// Stay in the loop if fscanf() successfully parsed two tokens as specified.
      if (rw == 'r'){
         char content[100];
         int hit;
         int time;
         printf("r %x\n", addr);
         read =1;
         arg_set= addr &mask(L1.size/(L1.associativity*L1.b_size));
         l1.HandleRequest(addr, arg_set, read, hit);
      }
      else if (rw == 'w'){
         printf("w %x\n", addr);
         char content[100];
         int hit;
         int time;
         read=0;
         arg_set= addr &mask(L1.size/(L1.associativity*L1.b_size));
         l1.HandleRequest(addr, arg_set, read, hit);
      }
      else {
         printf("Error: Unknown request type %c.\n", rw);
	 exit(EXIT_FAILURE);
      }
    }


    l1.GetStats(s);
    printf("Number of L1 reads are: %d\n", s.read);
    printf("Number of L1 read misses are: %d\n", s.read_miss);
    printf("Number of L1 writes are: %d\n", s.write);
    printf("Number of L1 write misses are: %d\n", s.write_miss);
    //printf("Number of L1 reads_1 are: %d\n", s1.read_1);
    //printf("Number of L1 read misses_1 are: %d\n", s1.read_miss_1);
    //printf("Number of L1 writes_1 are: %d\n", s1.write_1);
    //printf("Number of L1 write misses_1 are: %d\n", s1.write_miss_1);
    printf("Number of L1 writebacks to next level are: %d\n", s.write_back);
    //printf("Number of L1 prefetches are: %d\n", s1.prefetch_num);
    //printf("Miss rate of L1: %f\n",mr_l1);
    printf("\nTotal L1 access count: %d\n", s.access_counter);
    printf("Total L1 access time: %d cycles\n", s.access_time);
    printf("  L1 AAT = %f cycles\n", (float)s.access_time/(float)s.access_counter);
    printf("Total L1 miss count: %d\n", s.miss_num);
    printf("  Miss rate = %f%%\n", (float)s.miss_num/(float)s.access_counter*100.0);
    //printf("Total L1 lower access when miss count: %d\n", s.lower_access_counter);
    //printf("  Lower access per miss = %f\n", (float)s.lower_access_counter/(float)s.miss_num);
    //printf("Total L1 miss time:%d cycles\n", s.miss_time);
    //printf("  Miss penalty = %f cycles\n", (float)s.miss_time/(float)s.miss_num);
    printf("Total L1 fetch count: %d\n", s.fetch_num);
    printf("Total L1 replace count: %d\n", s.replace_num);
    printf("Total L1 prefetch count: %d\n", s.prefetch_num);

    l2.GetStats(s);
    printf("Number of L2 reads are: %d\n", s.read);
    printf("Number of L2 read misses are: %d\n", s.read_miss);
    printf("Number of L2 writes are: %d\n", s.write);
    printf("Number of L2 write misses are: %d\n", s.write_miss);
    //printf("Number of L2 reads_1 are: %d\n", s.read_1);
    //printf("Number of L2 read misses_1 are: %d\n", s.read_miss_1);
    //printf("Number of L2 writes_1 are: %d\n", s.write_1);
    //printf("Number of L2 write misses_1 are: %d\n", s.write_miss_1);
    printf("Number of L2 writebacks to next level are: %d\n", s.write_back);
    //printf("Number of L2 prefetches are: %d\n", s.prefetch_num);
    //printf("Miss rate of L2: %f\n",mr_l2);
    printf("Total L2 access count: %d\n", s.access_counter);
    printf("Total L2 access time: %d cycles\n", s.access_time);
    printf("  L2 AAT = %f cycles\n", (float)s.access_time/(float)s.access_counter);
    printf("Total L2 miss count: %d\n", s.miss_num);
    printf("  Miss rate = %f%%\n", (float)s.miss_num/(float)s.access_counter*100.0);
    //printf("Total L1 lower access when miss count: %d\n", s.lower_access_counter);
    //printf("  Lower access per miss = %f\n", (float)s.lower_access_counter/(float)s.miss_num);
    //printf("Total L2 miss time:%d cycles\n", s.miss_time);
    //printf("  Miss penalty = %f cycles\n", (float)s.miss_time/(float)s.miss_num);
    printf("Total L2 fetch count: %d\n", s.fetch_num);
    printf("Total L2 replace count: %d\n", s.replace_num);
    printf("Total L2 prefetch count: %d\n", s.prefetch_num);

    m.GetStats(s);
    printf("\nTotal Memory access count: %d\n", s.access_counter);
    printf("Total Memory access time: %d cycles\n", s.access_time);
    printf("  Mem AAT = %f cycles\n", (float)s.access_time/(float)s.access_counter);
    return(0);
}


