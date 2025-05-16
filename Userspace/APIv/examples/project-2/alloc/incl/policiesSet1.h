#ifndef POLICIESSET1_H
#define POLICIESSET1_H



#include "../../../../include/usm/usm.h"


struct optEludeList {
    struct page * usmPage;      // no pointer needed..
    struct list_head iulist;
    struct list_head proclist;
};

extern struct to_swap *toSwapr;

extern struct list_head freeList;
extern struct list_head usedList;

extern pthread_mutex_t policiesSet1lock;


// extern int check_swap_out_dummyLT(struct usm_event * event);

// extern void check_swap_in_dummyLT(struct usm_event * event);


struct usm_process_swap
{
    struct to_swap *swapNode;
    int process;
     struct list_head iulist;
};

extern struct usm_process_swap *process_swap;
extern struct list_head processList;


#endif