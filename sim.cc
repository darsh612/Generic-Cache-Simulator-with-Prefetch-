#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <inttypes.h>
#include <math.h>
#include<string.h>
#include<iostream>
#include <utility>
#include "sim.h"
#include <bits/stdc++.h>
#include <algorithm>


void Cache::SetConfig(CacheConfig cc)
{
    config_ = cc;
    size =cc.size;
    way = cc.associativity;
    b_s = cc.b_size;
    set = cc.size/(b_s*way);
    pref_N= cc.pref_N;
    pref_M= cc.pref_M;
    prefet_use= cc.use_prefetch;
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
            counter[i][j] = j;
        }
    }
    for(int i=0;i<pref_N;i++){
        pref_valid[i]= false;
        prefet[i]= new dex [pref_M];
        pref_counter[i]= i;
        for(int j=0;j<pref_M;j++){
            prefet[i][j]= 0x0;
        }
    }
	offset_mask = mask(offset_bits);
	index_mask = mask(idx_bits + offset_bits);
	tag_mask = mask(tag_bits + idx_bits + offset_bits);

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

int Cache::find_line(dex addr, int &line)
{
    dex arg_set= get_idx(addr);
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

void Cache::read_hit(dex addr)
{
    int read =1;
    int arg_line=0;
    int arg_set= get_idx(addr);
    find_line(addr,arg_line);
    //dirty[arg_set][arg_line];
    //stats.read++;
    //printf("addr Updated: %llx \n",addr);
    //printf("This is a read hit\n");
    LRU_update(addr, arg_line);
}

void Cache::read_miss(dex addr)
{
    int read =1;
    int arg_set= get_idx(addr);
    int sub_line = -1, max_cnt = -1;
    bool found = false;
    dex old_addr=0;
    stats.lower_access_counter++;
    for (int i = 0; i < way; i++){
        if (!valid[arg_set][i]) // To find empty block in cache
        {
            found = true;
            sub_line = i;
            break;
        }
        if (valid[arg_set][i] && max_cnt <= counter[arg_set][i]) // To find LRU
        {
            max_cnt = counter[arg_set][i];
            sub_line = i;
            old_tag= cache_array[arg_set][sub_line];
            old_addr= ((old_tag)<<(idx_bits+offset_bits))|((arg_set)<<(offset_bits));
        }
    }
    
    if (!found && dirty[arg_set][sub_line] ) // replace(writeback from 1st level to next level)
    {  
        stats.write_back++;
        if(lower_lvl==NULL){
        }
        else{
        //printf("addr: %llx c= %d \n", addr, counter[arg_set][sub_line]);
        lower_lvl->HandleRequest(old_addr, 0);
        }
    }

   /* else if(!found && !dirty[arg_set][sub_line]){
        if(lower_lvl==NULL){
        }
        else{
        lower_lvl->HandleRequest(old_addr, 1);
        }
   }*/
    if(lower_lvl == NULL){
        //printf("addr: %llx c= %d \n", addr, counter[arg_set][sub_line]);
    }
    else{
        lower_lvl->HandleRequest(addr,1);
    }
    evict_block(addr,sub_line);
    valid[arg_set][sub_line] = true;
    read_hit(addr); 
    dirty[arg_set][sub_line] = false;
}


void Cache::write_hit(dex addr)
{
    int read=0;
    int arg_set= get_idx(addr);
    int arg_line=0;
    find_line(addr, arg_line);
    dirty[arg_set][arg_line]= true;
    //stats.write++;
    //printf("addr Updated: %llx\n", addr);
    //printf("This is a write hit\n");
    LRU_update(addr, arg_line);
}

void Cache::write_miss(dex addr)
{
    int read =0;
    int arg_set= get_idx(addr);
    int sub_line = -1, max_cnt = -1;
    bool found = false; 
    dex old_addr=0;
    stats.lower_access_counter++;
    
    for (int i = 0; i < way; i++){
        if (!valid[arg_set][i]) // empty line found
        {
            found = true;
            sub_line = i;
            break;
        }
        else if (valid[arg_set][i] && max_cnt <= counter[arg_set][i]) // LRU line found
        {
            max_cnt = counter[arg_set][i];
            sub_line = i;
            old_tag= cache_array[arg_set][sub_line];
            old_addr= (((old_tag)<<(idx_bits+offset_bits))|((arg_set)<<(offset_bits)));
        }
    }

    if (!found && dirty[arg_set][sub_line]) // LRU
    {
        stats.write_back++;
        if(lower_lvl==NULL){
            //printf("addr: %llx c= %d \n", addr, counter[arg_set][sub_line]);
        }
        else{
        //printf("addr: %llx c= %d \n", addr, counter[arg_set][sub_line]);
        lower_lvl->HandleRequest(old_addr, 0);
        }
    }
    /*else if(!found && !dirty[arg_set][sub_line])
    {
        if(lower_lvl==NULL){
        }
        else{
        lower_lvl->HandleRequest(old_addr, 1);
        }
    }*/
    if(lower_lvl == NULL){
    }
    else{
        lower_lvl->HandleRequest(addr,1);
    }
    evict_block(addr,sub_line);
    valid[arg_set][sub_line]=true;
    dirty[arg_set][sub_line]= true;
    write_hit(addr);
    
}

void Cache::LRU_update(dex addr, int arg_line)
{
    dex arg_set = get_idx(addr);
    for (int i = 0; i < way; i++){
        //printf("old i=%d arg_set=%d c=%d arg_line= %d \n", i,arg_set,counter[arg_set][i], arg_line);
        if (i != arg_line){
            if(counter[arg_set][i]<counter[arg_set][arg_line]){
              counter[arg_set][i]++;
           }
        }
    }

    for (int i = 0; i < way; i++){
        if (i == arg_line){//counter[arg_set][i] = 0;
            counter[arg_set][i]=0;
        }
        //printf("new i=%d arg_set=%d c=%d arg_line= %d \n", i,arg_set,counter[arg_set][i], arg_line);
    }

    /*for(int i=0;i<way;i++){
        printf("i=%d arg_set=%d c=%d arg_line= %d \n", i,arg_set,counter[arg_set][i], arg_line);
        if(i==arg_line){
            counter[arg_set][i]=0;
            //printf("i= %d c=%d arg_line= %d \n",i,counter[arg_set][i], arg_line );
        }
        else{
            if(counter[arg_set][i]<arg_line){
            counter[arg_set][i]++;
            }
        }
    }*/
        //else counter[arg_set][i]++;
}

void Cache:: evict_block(dex addr, int sub_line){

    int arg_tag=get_tag(addr);
    int arg_set= get_idx(addr);
    cache_array[arg_set][sub_line]=arg_tag;
    }

void Cache::HandleRequest(dex addr,int read)
{
    unsigned long long _tag = get_tag(addr);
    unsigned long long arg_set = get_idx(addr);
    unsigned long long _offset = get_offset(addr);
    int _line;
    int stream=0; 
    int hit=0;
    int prefe_hit=0;
    dex prefet_addr;
    int buf_line=0;
    prefet_addr= pref_addr(addr);
    if(lower_lvl==NULL){
        if(prefet_use){
           prefe_hit=pref_check_hit(prefet_addr, stream);
           //find_stream_fill(stream);
           if(prefe_hit){
            printf("req addr: %llx prefet_addr: %llx \n", addr, prefet_addr);
           }
        }
    }

    hit = find_line(addr, _line);
    printf("SB Hit(1)/Miss(0) - %d and address - %llx\n",prefe_hit, addr);

    if (read == 1)  // read
    {
        if (hit == 1)   // read hit
        {   
            //printf("Cache Read hit\n");
            if(!prefe_hit){
                //printf("Prefetch miss\n");
            }
            else{
                //printf("Prefetch hit\n");
                pref_hit(prefet_addr,stream, buf_line);

                LRU_pref_update(stream);
                printf("sb addr: %llx pref_counter:%d \n", addr, pref_counter[stream]);
            }
            read_hit(addr);
            stats.read++;
            stats.access_counter++;
            return;
        }
        else    // read miss
        {
            //printf("Cache Read miss\n");
            stats.read++;
            if(!prefe_hit){
                //printf("Read Miss\n");
                //printf("prefetch miss\n");
                pref_miss(prefet_addr,stream);
                LRU_pref_update(stream);
                stats.read_miss++;
                stats.miss_num++;
            }
            else{
                //printf("Prefetch hit\n");
                pref_hit(prefet_addr,stream,buf_line);
                LRU_pref_update(stream);
            }
            stats.access_counter++;
            read_miss(addr);
            return;
        }
    }
    else    // write
    {
        if (hit == 1)   // write hit
        {     
            //printf("Cache Write hit\n");
            if(!prefe_hit){
               // printf("Prefetch miss\n");
            }
            else{
                //printf("Prefetch hit\n");
                pref_hit(prefet_addr, stream, buf_line);
                LRU_pref_update(stream);
            }
            write_hit(addr);
            stats.write++;
            stats.access_counter++;
            return;
        }
        else    // write miss
        {
            //printf("Cache Write miss\n");
            stats.write++;
            stats.access_counter++;
            
            if(!prefe_hit){
                //printf("Write miss\n");
                //printf("Prefetch miss\n");
                pref_miss(prefet_addr,stream);
                LRU_pref_update(stream);
                stats.write_miss++;
                stats.miss_num++;
            }
            else{
                //printf("Prefetch hit\n");
                pref_hit(prefet_addr, stream, buf_line);
                LRU_pref_update(stream);
            }
            write_miss(addr);
            return;
        }
    }
}


void Cache:: find_stream_fill(int &stream){
    for(int i=0;i<pref_N;i++){
        if(!pref_valid[i]){
            stream=i;
            break;
        }
        else if(pref_valid[i] && pref_counter[i]>=pref_N-1){
            stream=i;
            break;
        }
    }
}

void Cache:: prefet_fill(dex addr, int &stream){
    pref_valid[stream]=true;
    for(int i=0;i<pref_M;i++){
        printf("SB MISS: updated address = %llx n = %d\n",addr+i+1, stream);
        prefet[stream][i]= addr+i+1;
        stats.prefetch_num++;
    }
}


void Cache:: LRU_pref_update(int stream){
    for(int i=0;i<pref_N;i++){
        printf("pref_counter= %d stream: %d\n", pref_counter[i],stream);
        if(i != stream)
        {
            if (pref_counter[i] < pref_counter[stream])
                pref_counter[i]++;
        }
    }
    for(int i=0;i<pref_N;i++){
        if(i == stream)
        {
            pref_counter[i]=0;
        }
    }
}

int Cache:: pref_check_hit(dex addr, int stream){
    int min_num= pref_counter[0];
    int buf_line=0;
    for(int i=0;i<pref_N;i++){
        if(pref_valid[i] && pref_counter[i]<=min_num){
            for(int j=0;j<pref_M;j++){
                if(prefet[i][j]==addr){ 
                   buf_line=j;
                   stream =i;
                   min_num=pref_counter[i];           //MRU
                }
            }
        }
    }
    if(prefet[stream][buf_line]==addr){
        printf("This is a prefetch hit\n");
        return 1;
    }
    else{
        find_stream_fill(stream);
        return 0;
    }
}

void Cache:: pref_hit(dex addr, int stream, int buf_line){
    buf_line=0;
    for(int i=0;i<pref_M;i++){
        if(prefet[stream][i]==addr){
            buf_line=i;
            break;
        }
    }
    for(int i=buf_line;i<pref_M;i++){
        prefet[stream][i-buf_line]=prefet[stream][i];
    }
    for(int i=-1;i<buf_line;i++){
        printf("SB HIT: updated address = %llx n = %d\n",prefet[stream][pref_M-buf_line-1]+i+2, stream);
        prefet[stream][pref_M-buf_line+i]= prefet[stream][pref_M-buf_line-1]+i+2;
        stats.prefetch_num++;
    }
}

dex Cache:: pref_addr(dex addr){
   return ((addr)>> offset_bits);
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
    Cache l1, l2, l1_1, l2_1;
    CacheConfig L1, L2;
    char *c="D";
    int arg_set;
    L1.size= params.L1_SIZE;
    L1.associativity=params.L1_ASSOC;
    L1.b_size=params.BLOCKSIZE;
    L2.size=params.L2_SIZE;
    L2.associativity=params.L2_ASSOC;
    L2.b_size=params.BLOCKSIZE;
    if(params.PREF_N==0){
        L1.use_prefetch=0;
        L2.use_prefetch=0;
        L1.pref_M=0;
        L1.pref_N=0;
        L2.pref_M=0;
        L2.pref_N=0;
    }
    else if(params.L2_SIZE==0 && params.PREF_N!=0){
        L1.use_prefetch=1;
        L2.use_prefetch=0;
        L1.pref_N= params.PREF_N;
        L1.pref_M= params.PREF_M;
        L2.pref_N=0;
        L2.pref_M=0;
    }
    else if(params.L2_SIZE!=0 && params.PREF_N!=0){
        L1.use_prefetch=0;
        L2.use_prefetch=1;
        L1.pref_N=0;
        L1.pref_M=0;
        L2.pref_N=params.PREF_N;
        L2.pref_M= params.PREF_M;
    }
    l1_1.SetConfig(L1);
    l1.SetConfig(L1);
    if(L2.size==0){   
    }
    else{
    l2.SetConfig(L2);
    l2_1.SetConfig(L2);
    }
    if(params.L2_SIZE==0){
        l1.SetLower((Cache*)NULL);
    }
    else{
        l1.SetLower(&l2);
        l2.SetLower((Cache*)NULL);
    }

    Stats s, s1;
    s.miss_num = 0;
    s.access_counter = 0;
    s.read=0;
    s.write=0;
    s.read_miss=0;
    s.write_miss=0;
    s.write_back=0;
    s.miss_num=0;
    s.lower_access_counter = 0;
    s.fetch_num = 0;
    s.replace_num = 0;
    s.prefetch_num =0;

    s1.miss_num = 0;
    s1.access_counter = 0;
    s1.read=0;
    s1.write=0;
    s1.read_miss=0;
    s1.write_miss=0;
    s1.write_back=0;
    s1.miss_num=0;
    s1.lower_access_counter = 0;
    s1.fetch_num = 0;
    s1.replace_num = 0;
    s1.prefetch_num =0;

    l1.SetStats(s1);
    l2.SetStats(s);


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
  
  printf("\n");

   // Read requests from the trace file and echo them back.
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2) {	// Stay in the loop if fscanf() successfully parsed two tokens as specified.
      //printf("address = %llx\n",addr);
      if (rw == 'r'){
         //printf("%s %x\n", &rw, addr);
         read =1;
         l1.HandleRequest(addr, read);
        
        /*for(int i=0;i<l1.pref_N;i++){
            printf("READ: num = %d address = %llx lruc = %d content - ", i, addr, l1.pref_counter[i]);
            for(int j=0;j<l1.pref_M;j++){
                printf("%llx ", l1.prefet[i][j]);
            }
            printf("\n");
        }
        printf("\n");*/
      }
      else if (rw == 'w'){
         //printf("%s %x\n", &rw, addr);
         read=0;
         l1.HandleRequest(addr, read);

        /* for(int i=0;i<l1.pref_N;i++){
            //printf("WRITE: num = %d address = %llx lruc = %d content - ", i, addr, l1.pref_counter[i]);
            for(int j=0;j<l1.pref_M;j++){
                printf("%llx ", l1.prefet[i][j]);
            }
            printf("\n");
        }
        printf("\n");*/
      }
      else {
         printf("Error: Unknown request type %c.\n", rw);
	 exit(EXIT_FAILURE);
      }
      
    }

int min_cnt=0;
int min_cnt1=0;
int min_line=0;
int min_line1=0;


    
    
   printf("===== L1 contents =====\n");
   
   for(int i=0;i<l1.set;i++){
        printf("set\t%d\t:",i);
        for(int rec=0;rec<l1.way;rec++){
            for(int j=0;j<l1.way;j++){
                //printf("i=%d, j=%d, lruc=%d\n",i,j,l1.counter[i][j]);
                if(rec==l1.counter[i][j]){
                    printf("%llx %s ", l1.cache_array[i][j], l1.dirty[i][j]?"D ":"  ");
                }
            }
        }
        printf("\n");
   }
    

    if(L2.size==0)
    {
    }
    else{
    printf("\n");
    printf("===== L2 contents =====\n");
    for(int i=0;i<l2.set;i++){
        printf("set\t%d\t:",i);
        for(int rec=0;rec<l2.way;rec++){
            for(int j=0;j<l2.way;j++){
                if(rec == l2.counter[i][j]){
                    printf("%llx %s ", l2.cache_array[i][j], l2.dirty[i][j]?"D ":"  ");
            }
            }
        }
        printf("\n");
      }
    }
    if(l1.prefet_use==1){
          printf("===== Stream Buffer(s) contents =====\n");
          for(int i=0;i<l1.pref_N;i++){
            printf("%d\t%d ",l1.pref_valid[i], l1.pref_counter[i]);
             for(int j=0;j<l1.pref_M;j++){
                printf("%llx\t",l1.prefet[i][j]);
            }
             printf("\n");
          }
      }
    else if(l2.prefet_use==1){
        printf("===== Stream Buffer(s) contents =====\n");
        for(int i=0;i<l2.pref_N;i++){
            printf("%d\t%d ",l2.pref_valid[i], l2.pref_counter[i]);
            for(int j=0;j<l2.pref_M;j++){
                printf("%llx\t",l2.prefet[i][j]);
            }
            printf("\n");
        }
}
    
   
   printf("\n");

    l1.GetStats(s1);

    printf("===== Measurements =====\n");
    printf("a. L1 reads:\t\t\t%d\n", s1.read);
    printf("b. L1 read misses:\t\t%d\n", s1.read_miss);
    printf("c. L1 writes:\t\t\t%d\n", s1.write);
    printf("d. L1 write misses:\t\t%d\n", s1.write_miss);

    s1.miss_rate= (float)s1.miss_num/s1.access_counter;
    printf("e. L1 miss rate:\t\t%.4f\n",s1.miss_rate);
    printf("f. L1 writebacks:\t\t%d\n", s1.write_back);
    printf("g. L1 prefetches:\t\t%d\n",s1.prefetch_num);

    l2.GetStats(s);
    if(s.read==0){
        s.miss_rate=0;
    }
    else{
    s.miss_rate= (float)s.read_miss/s.read;
    }

    printf("h. L2 reads (demand):\t\t%d\n", s.read);
    printf("i. L2 read misses (demand):\t%d\n", s.read_miss);
    printf("j. L2 reads (prefetch):\t0\n");
    printf("k. L2 read misses (prefetch):\t0\n");
    printf("l. L2 writes :\t\t\t%d\n", s.write);
    printf("m. L2 write misses:\t\t%d\n", s.write_miss);
    printf("n. L2 Miss rate:\t\t%.4f\n",s.miss_rate);
    printf("o. L2 writebacks:\t\t%d\n", s.write_back);
    printf("p. L2 prefetches:\t\t%d\n",s.prefetch_num);
    if(L2.size!=0){
       printf("q. memory traffic:\t\t%d\n",s.read_miss+s.write_miss+s.write_back+s.prefetch_num);
    }
    else{
        printf("q. memory traffic:\t\t%d\n",s1.read_miss+s1.write_miss+s1.write_back+s1.prefetch_num);
    }
    return 0;
}

