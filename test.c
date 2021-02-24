#include <stdio.h>
#include<stdlib.h>
#include<string.h>
#include <stdint.h>
#include <sched.h>

#define CACHELINE_SIZE 64

typedef struct node 
{
    struct node *next; //8 bytes
} Node, *ListNode;

ListNode root;


u_int64_t start_high, start_low, end_high, end_low, start_time, end_time;

void flush(void* p) {
    asm volatile ("clflush (%0)\n"
      :
      : "c" (p)
      : "rax");
}

#define CLOCK_START(start_high, start_low, start_time) \
    asm volatile ("CPUID\n"\
                    "RDTSC\n\t"\
                    "mov %%rdx, %0\n"\
                    "mov %%rax, %1\n"\
                    : "=r" (start_high), "=r" (start_low)\
                    :: "%rax", "%rbx", "%rcx", "%rdx");\
    start_time = (start_high << 32) | start_low


#define CLOCK_END(cycles_high, cycles_low, end_time) \
    asm volatile ("RDTSCP\n\t"\
                  "mov %%rdx, %0\n\t"\
                  "mov %%rax, %1\n\t"\
                  "CPUID\n\t"\
                  : "=r" (cycles_high), "=r" (cycles_low)\
                  :: "%rax", "%rbx", "%rcx", "%rdx");\
    end_time = (cycles_high << 32) | cycles_low


/**
 * @brief flush the content in address [start+backward, start+forward) from cache 
 * 
 * @param start 
 * @param backward 
 * @param forward 
 * @modify 
 */
void flushAll(void* start, int backward, int forward){
    for(int i= backward; i< forward; i+=CACHELINE_SIZE){
        void * address = start + (i);
        flush(address);
    }
}

/**
 * @brief  using asm code to access a virtual address.
 * 
 * @param p virtual address to access
 */
#define MEMACCESS(p)\
asm volatile ("movq (%0), %%rax\n"\
    :\
    : "c" (p)\
    : "rax")\


/* 
Rounding; only works for n = power of two 
For ROUND: return the smallest number that is a multiple of n and >=a; e.g. n=4096: a=4096, return 4096; a=2, return 4096 
For ROUNDDOWN: return the biggest number that is a multiple of n and <=a; e.g. n=4096: a=4096, return 4096; a=2, return 0
*/
#define ROUND(a, n)	(((((u_long)(a))+(n)-1)) & ~((n)-1)) 
#define ROUNDDOWN(a, n)	(((u_long)(a)) & ~((n)-1))
// min max macro function
#ifndef HIDEMINMAX
#define MAX(X,Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X,Y) (((X) < (Y)) ? (X) : (Y))
#endif


void StrideTest(){
    void * pool = malloc(2*4096); // alloc 2 OS page
    void * page_start = (void *)ROUND(((u_int64_t)pool), 4096);// here we align the pool to the start of one OS page 
    root = (ListNode) page_start; //assgin line0 to root0
    ListNode p,q;
    p = root;
    q = (ListNode) (page_start + 3*CACHELINE_SIZE); // line 3 of the page
    p->next = q;
    p = p->next;
    q = (ListNode) (page_start + 6*CACHELINE_SIZE); // line 6 of the page
    p->next = q;
    p = p->next;
    q = (ListNode) (page_start + 9*CACHELINE_SIZE); // line 9 of the page
    p->next = q;
    p = p->next;
    p->next = NULL;// till now, the root is line0->3->6->9
    ListNode measure_node = (ListNode) (page_start + 12*CACHELINE_SIZE);


    int timer[200]={0}; // record access time to cache line 12
    memset(timer,0, 200);

    int repeat_times = 10000; // repeat measure times
    
    int temp;
    for(int i=0; i<repeat_times; i++){// outer loop will do the test for many times
            p = root;

            flushAll((void *)root, 0, 4096);

            // sleep and delay 1000 cycles to make sure the whole OS　page is flushed from cache
            for(int j=0; j<1000;j++){

            }
            // training access pattern
            
            // Access to the access pattern for one time(Access pattern: line 0->3->6->9)
            while(p!=NULL){
                MEMACCESS(p);
                p = p-> next;
                for(int j=0; j<1000;j++){ // We may use fence as well, but Intel mannual says maybe a fence in progress can prevent the L1 IP-based prefetch

                }
            }


            // measure line 12
            asm volatile("mfence");
            CLOCK_START(start_high, start_low, start_time);
            MEMACCESS(measure_node);
            CLOCK_END(end_high, end_low, end_time);
            uint64_t time = end_time - start_time;
            asm volatile("mfence");

            timer[MIN(199, time/5)]++; // here div 5 just reduce the elems in the 
    }

    // output the access line pattern
    printf("This test is for stride pattern(run the access pattern for only once)!\n");
    printf("Repeat prefetch test: Access line---> 0 3 6 9, cache time for line 12:\n");

    // output cache hit result
    for(int i=0; i< 200; i++){
        if(timer[i]>0) printf("time: %d, cnt: %d, percent:%.3f%\n",5*i, timer[i], 1.0*timer[i]/repeat_times*100);
    }

    free(pool);
}



int main(){
    //set affanity so that the program will run on one core
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(0, &cpu_set);
    if (sched_setaffinity(0, sizeof(cpu_set), &cpu_set) < 0) {
        printf("set affanity error!");
        exit(EXIT_FAILURE);
    }


    StrideTest();

    return 0;
}