
#include "../../alloc/incl/policiesSet1.h"

#include <sys/stat.h>

/*
    Sooo.. what to do here... -_-'
*/

#define workersByPagesThreshold 100

extern pthread_mutex_t policiesSet1Swaplock;
extern struct list_head swapList;
extern struct list_head swapListTime;

extern struct list_head toSwapInList;

extern struct  to_swap *toSwapNode_glob;

extern struct list_head timeList;

struct proc_swp_create_list_arg {
    int nr;
    struct list_head * procSwpLst;
};

// Marqueurs FHP
struct fhp_marker {
    unsigned long base;
    int pages;
    int process;
    int allocated; // flag pour éviter les allocations multiples
    int swap_device_number;
    struct list_head list;
};


