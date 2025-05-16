#ifndef USM_H
#define USM_H

#include "../policies/evict/swap.h"
//#include "../policies/oom/oom.h"
#include <linux/userfaultfd.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <string.h>

#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>


#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <signal.h>
#include <pthread.h>

#include "hashmap.h"
#include <assert.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define MAX_PROC 1000
// #define MAX_USM_SIZE 10240	// getThroughsysconfLater..
#define SYS_PAGE_SIZE sysconf(_SC_PAGESIZE)
// static const unsigned int USM_CHUNK_SIZE = (unsigned int) 2*1024*1024*1024;

#define _FILE_OFFSET_BITS 64



extern struct page {
    intptr_t data;
    unsigned long virtualAddress;
    unsigned long physicalAddress;
    int process;						// if -1 -> shared.. TODO... should hasten things..? -_-'
    void * usedListPositionPointer;
    struct list_head * processUsedListPointer;
    // bool shared;     // TODO this + checks
    // vma... dude, we always got beginnings... find a way to calculate 'em all.... that should be cool.!
#ifdef PLock
    pthread_mutex_t lock;				// hell..
#endif
} * pagesList;



extern int port, fd;
extern void * usmZone;
extern int usmCMAFd, usmNewProcFd, usmFreePagesFd, usm_com_end_point;
extern unsigned long poolSize;
extern unsigned int globalPagesNumber;
extern unsigned long basePFN;
extern unsigned int globalPageSize;
extern volatile unsigned long usedMemory;
extern volatile unsigned long swappedMemory;
extern unsigned int workersNumber;
extern unsigned short allocPoliciesNumber;
extern char * allocAssignmentStrategy;		// ya don't need it..
extern int init;

extern struct hashmap *usm_alloc_policies;
extern struct hashmap *usm_swap_policies;
struct usm_process_link {
    char * procName;				// them threads.. just get the main's name for now
    unsigned long alloc_pol;
    unsigned long swp_pol;          // dude, if we got the freaking swapping thread to be modular/generic, we could do both ways... like man these should be about everything : periodicity, condition (mem. threshold & Co., let's be crazy : CPU usage, etc) but the problems'd be the freaking struct.s... for now, let's try and do something utterly basic or even hyper naïve.
    int prio;
};
extern struct hashmap *usm_process_linking;
extern unsigned long allocPolicyThresholds [100];


extern struct processDesc {
     char name [16];
    // name.... strcpy..
    struct usm_alloc_policy_ops * alloc_policy;							// there'd('ll) be a ver. if we want some global one or not before accessing this, when handling events #default'true|false
    struct usm_swap_policy_ops * swap_policy;
    unsigned long allocated;
    unsigned long swapped;
    struct list_head swapList;          // take a copy and launch a cleaning thread at procDeath.. as there'll be some new bro. coming... hmm TODO double check its arrival..
    struct list_head usedList;      // to be used if wanted..? Or mandatory? Anyway, alloc. module's compliance TODO
    // FILE * backfile;     .. sad.. but could be replaced later by usm_swap_dev..or most probably .._node
    struct usm_swap_dev * prefferedSwapDevice;          // filled with cfg....      | #byNumber..
    int prio;					// usm_cnl_prio to be considered.... problem'd be this'dn't be between processes, but between ones from the same worker.. btw, ftm it's still one channel one process because of the way it's done in the kernel...
    // ... other "per process" info.
    pthread_mutex_t lock;		// hell..
    pthread_mutex_t alcLock;
    pthread_mutex_t swpLock;
    unsigned long lastTakenOffset;
    struct list_head swapCache;
} * procDescList;			// the main problem now is that there is a channel per process, for uffd.. some mod.s to do, to put them all in an "usm" channel? To give some more meaning to this all?


extern struct usm_ops {
    // int (*usm_init)(unsigned int poolSize, unsigned int pageSize);	| there shouldn't be many usm_inits.. #Shouldn'tBeProgrammable	// maybe for the definition of them policies.. but meh...
    int (*usm_setup) (unsigned int pagesNumber/*unsigned int pageSize... for him to combine pages #Huge*/);
    void (*usm_free)();
} dev_usm_ops, dev_usm_swap_ops;

extern struct usm_global_ops {
    struct usm_ops * dev_usm_alloc_ops;
    struct usm_ops * dev_usm_swap_ops;
    struct usm_ops * dev_usm_oom_ops;
} global_ops;

extern void usm_launch(struct usm_global_ops globOps); //(struct usm_ops * alloc_usm_ops, struct usm_ops * swap_usm_ops); //appeler pour démarrer la machinerie USM. C'est cette fonction qui fait en sorte que USM se met en action et commencer à traiter des requêtes.

extern int usm_set_alloc_policy_assignment_strategy(char *alloc_policy_assignment_strategy_cfg);//donne le fichier qui contient les règles d'assignation des processus aux politiques
static inline int usm_register_alloc_policy(struct usm_alloc_policy_ops *alloc_policy, char * policy_name, int is_default) {		//enregistre une politique dans usm
    if (is_default)
        default_alloc_policy=*alloc_policy;
    if(!hashmap_set(usm_alloc_policies,&(struct usm_policy) {
    .ops=(unsigned long)alloc_policy,.name=policy_name
    }))		// weirdly no strcpy problem....
        if(hashmap_oom(usm_alloc_policies))
            return 1;
    allocPoliciesNumber++;
    return 0;
}

static inline int usm_register_swap_policy(struct usm_swap_policy_ops *swap_policy, char * policy_name, int is_default) {		//enregistre une politique dans usm
    if (is_default)
        default_swap_policy=*swap_policy;
    if(!hashmap_set(usm_alloc_policies,&(struct usm_policy) {
    .ops=(unsigned long)swap_policy,.name=policy_name
    }))		// weirdly no strcpy problem....
        if(hashmap_oom(usm_alloc_policies))
            return 1;
    allocPoliciesNumber++;
  
    return 0;
}

extern void appendUSMChannel(struct usm_channel_ops *ops, int channelFd, struct list_head *workerChannelList);

extern void usm_parse_args(char *argv[], int argc);

/*
	Tous les helpers functions ici
*/


static inline int usmPolicyDefinedHint(struct usm_event * event)
{   
    return procDescList[event->origin].alloc_policy->usm_hints(event);        
}

static inline void increaseProcessAllocatedMemory(unsigned int process, unsigned int size) {
    pthread_mutex_lock(&procDescList[process].lock);
    procDescList[process].allocated+=size;
    pthread_mutex_unlock(&procDescList[process].lock);
}

static inline void decreaseProcessAllocatedMemory(unsigned int process, unsigned int size) {		// ^ and then always check if applicable, i.e. we receive a page, go to decrease its linked process' allocated memory, then see it's a defined value of no touch..
    pthread_mutex_lock(&procDescList[process].lock);
    procDescList[process].allocated-=size;
    pthread_mutex_unlock(&procDescList[process].lock);
}

static inline void resetUsmProcess(unsigned int process) {			// TODO : add a linked list of all its pages.. #rmap-_-'... and get rid of 'em all's process slot to 0, except -1s i.e. shared or special (?..) pages.
    pthread_mutex_lock(&procDescList[process].lock);
    procDescList[process].alloc_policy=&default_alloc_policy;
    procDescList[process].swap_policy=&default_swap_policy;
    procDescList[process].allocated=0;
    procDescList[process].swapped=0;
    INIT_LIST_HEAD(&(procDescList[process].swapList));
    INIT_LIST_HEAD(&(procDescList[process].swapCache));
    procDescList[process].prio=0;
    pthread_mutex_unlock(&procDescList[process].lock);
}

static inline void *usmPfnToUsedListNode(unsigned long pfn) {
    return pagesList[pfn].usedListPositionPointer;
}

static inline void usmSetUsedListNode(struct page * page, void * usedListNode) {
    page->usedListPositionPointer=usedListNode;
}

static inline void usmResetUsedListNode(struct page * page) {
    page->usedListPositionPointer=NULL;
}

static inline unsigned int usmPfnToPageIndex(unsigned long pfn) {
    return pfn-basePFN;
}

static inline struct page * usmEventToPage(struct usm_event * event) {
    if(unlikely(usmPfnToPageIndex(event->paddr) >= globalPagesNumber || event->paddr < basePFN)) {
#ifdef DEBUG
        printf("[Sys] Event to page result uncoherent\n");
#endif
        return NULL;
    }
    return pagesList+(event->paddr-basePFN);
}

/*static inline void usmResetPage(struct usm_event * event) {	
    struct page * usm_page = usmEventToPage(event);	
    pthread_mutex_lock(&procDescList[usm_page->process].alcLock);            // toMove

    victimPageNode = list_first_entry(&procDescList[event->origin].usedList,struct optEludeList, proclist);
    list_del_init(&usm_page->);
    pthread_mutex_unlock(&procDescList[usm_page->process].alcLock);
}*/

static inline int usmPolicyDefinedPageIndexFree(struct usm_event * event) {			// uffd's unmap struct. should be able to give us at least one batch of PFNs...
    return procDescList[(usmEventToPage(event))->process].alloc_policy->usm_pindex_free(event);			// toPFN&Co.Comply...
}

inline int usmPolicyDefinedFree(struct usm_event * event) {
    while (event->length>0) {													// all this to be given to the pol. dev.
        if(procDescList[event->origin].alloc_policy->usm_free(event))			// origin should be compliant...
            return 1;
        event->vaddr+=SYS_PAGE_SIZE;
        event->length-=SYS_PAGE_SIZE;
    }
    return 0;
}

static inline int usmPolicyDefinedAlloc(struct usm_event * event) {
    return procDescList[event->origin].alloc_policy->usm_alloc(event);
}

static inline int usmSubmitAllocEvent(struct usm_event * event) {
    // some check on passed PFN.. but it'd further slow down things... dev. should.. know... right? 
    return usm_cnl_userfaultfd_ops.usm_submit_evt(event);
}


static inline int usmSubmitHintReturnEvent(struct usm_event * event) {
    // some check on passed PFN.. but it'd further slow down things... dev. should.. know... right? 
    return usm_cnl_hints_ops.usm_submit_evt(event);
}

static inline void usmSetAllocPolicy(int process, struct usm_alloc_policy_ops *policy) {		// ver. in proc. somewhere...!
    procDescList[process].alloc_policy=policy;
}

static inline void usmSetSwapPolicy(int process, struct usm_swap_policy_ops *policy) {		// ver. in proc. somewhere...!
    procDescList[process].swap_policy=policy;
}

static int reportAndReturnDefault (int val) {
#ifdef DEBUG
    printf("Problem here : %d\n", val);
    getchar();
#endif
    return 0;
}

static inline int usmGetEventPriority(struct usm_event * event) {
    return (event->origin>=0&&event->origin<MAX_PROC)?procDescList[event->origin].prio:reportAndReturnDefault(event->origin);
}

static inline void usmSetProcessPriority(struct usm_event * event, int priority) {
    assert(event->origin<MAX_PROC);
    procDescList[event->origin].prio=priority;
}

static inline void * usmPageToUsedListNode(struct page * page) {
    if(unlikely(!page->usedListPositionPointer)) {
#ifdef DEBUG
        printf("[Sys] Undefined used list node.\n");
#endif
        return NULL;
    }
    return page->usedListPositionPointer;
}

static inline void usmLinkPage(struct page * page, struct usm_event * event) {
    // lock actually probably not needed...
    /*
    	We could just use the page.. but the additional steps shouldn't be worth it.. -_-'

    	usmEventToPage(event)->virtualAddress=event->vaddr;
    	usmEventToPage(event)->process=event->origin;
    */
    if(page) {
        page->virtualAddress=event->vaddr;
        page->process=event->origin;
    }
}

#ifdef PLock
static inline void usmLockPage(struct page * page) {
    pthread_mutex_lock(&page->lock);
}
static inline void usmUnlockPage(struct page * page) {
    pthread_mutex_unlock(&page->lock);
}
#endif

static inline void usmSetupProcess (struct usm_event * event) {
    struct usm_process_link *plink = NULL;
    if(event->procName)
        plink=(struct usm_process_link *)hashmap_get(usm_process_linking, &(struct usm_process_link) {
            .procName=event->procName
        });
         strcpy(procDescList[event->origin].name,event->procName);
    if(plink) {
        if (plink->alloc_pol || plink->swp_pol) {
            if(plink->alloc_pol) {
                usmSetAllocPolicy(event->origin,(struct usm_alloc_policy_ops *)plink->alloc_pol);
#ifdef DEBUG
                printf("Affected config. defined alloc policy!\n");
                getchar();
#endif
            }
            if(plink->swp_pol) {
                usmSetSwapPolicy(event->origin,(struct usm_swap_policy_ops *)plink->swp_pol);
#ifdef DEBUG
                printf("Affected config. defined swap policy!\n");
                getchar();
#endif
            }
        }
#ifdef DEBUG
        else {
            printf("Applied default policy..\n");
            getchar();
        }
#endif
        usmSetProcessPriority(event,plink->prio);       // TODO Debug message compliance.. can still be about default prio. application
    }
#ifdef DEBUG
    else {
        printf("Applied default policy and priority..\n");
        getchar();
    }
#endif
    /*  legacy
    time_t timestamp = time( NULL );
    struct tm * pTime = localtime( & timestamp );
    char * swapID = (char *) malloc(80);
    strftime(swapID, 80, "%d%m%Y%H%M%S", pTime);      // TODO add proc. FD val.
    char * swapFileName = (char *) malloc (85);
    strcat(swapFileName, "/tmp/");
    strcat(swapFileName,swapID);
    procDescList[event->origin].backfile=fopen(swapFileName,"wb+");     // TODO per proc. backend... names provided in cfg (new swapBackend arg.), code provided in dev.'s swapOps ; this here'll be default        |   let dev. create it...       swapFile.. and swapOff
    if(!procDescList[event->origin].backfile) {                         // care about FOPEN_MAX... open and close each time? Freak's sake... might as well get the inode in kern. and just make the copies there (in case of obvious swap_ins, to alleviate it all)
#ifdef DEBUG
        printf("Swap file opening failed! Aborting! #atID%d", event->origin);
#endif
        abort();
    }
    */
}

static inline struct usm_worker * usmChooseWorker() {
    struct usm_worker *workersIterator = &usmWorkers;
    int chosenWorker=rand()%workersNumber;
#ifdef DEBUG
    printf("Chosen worker : %d\n", chosenWorker);
#endif
    while (chosenWorker>0) {
        workersIterator=workersIterator->next;
        chosenWorker--;
    }
    return workersIterator;
}

static inline int usm_set_pfn(int process, unsigned long addr, unsigned long pfn, int write, int channelType) {				// channelType to be used later, when there will be other ways of doing things than ioctl

    struct uffdio_palloc uffdio_palloc= {.addr=addr,.arg.uaddr=pfn, .write=write, .opt=4};
    if (ioctl(process, UFFDIO_PALLOC, &uffdio_palloc) != 1) {
    #ifdef DEBUG
            perror("\t\t[Mayday] Allocation failed!");
    #endif
            return 1;
    } 
            return 0;
}

static inline unsigned long usm_get_pfn(int process, unsigned long addr, unsigned long nr, int channelType) {
    struct uffdio_palloc uffdio_palloc= {.addr=addr,.arg.uaddr=nr,.opt=3};
    if (ioctl(process, UFFDIO_PALLOC, &uffdio_palloc) < 0) {
#ifdef DEBUG
        perror("\t\t[Mayday] PFN retrieval failed!");
#endif
        return 1;
    }
    return uffdio_palloc.arg.uaddr;
}

static inline int usm_clear_pfn(int process, unsigned long addr, int channelType) {
    struct uffdio_palloc uffdio_palloc= {.addr=addr,.arg.uaddr=0,.opt=2};           // opt 5 being recursive, with uaddr containing the length..
    if (ioctl(process, UFFDIO_PALLOC, &uffdio_palloc) != 1) {
#ifdef DEBUG
        perror("\t\t[Mayday] Eviction failed");
#endif
        return 1;
    }
    return 0;
}

static inline int usm_clear_and_set(int process, unsigned long addr, unsigned long offst, int channelType) { 
          
    struct uffdio_palloc uffdio_palloc= {.addr=addr,.arg.uaddr=offst,.opt=5};
    if (ioctl(process, UFFDIO_PALLOC, &uffdio_palloc) != 1) {
#ifdef DEBUG
        perror("\t\t[Mayday] Eviction failed and value posing failed");
#endif
        return 1;
    }
    printf("valeur de process : %d\n", process);  
    return 0;
}

static inline int usm_continue(struct usm_event * event) {
    struct uffdio_continue uffdio_continue= {.range= {.len=SYS_PAGE_SIZE,.start=event->vaddr}};
    if (ioctl(event->origin, UFFDIO_CONTINUE, &uffdio_continue) != 0) {
#ifdef DEBUG
        perror("\t\t[Mayday] Continuation failed!");
#endif
        return 1;
    }
    return 0;
}

static inline void insertPage (struct page ** pagesList, unsigned long physicalAddress, intptr_t data, int pos) {
    ((*pagesList)+pos)->data = data;
    ((*pagesList)+pos)->physicalAddress = physicalAddress;
    ((*pagesList)+pos)->virtualAddress = 0;
    ((*pagesList)+pos)->process = 0;
#ifdef PLock
    pthread_mutex_init(&((*pagesList)+pos)->lock,NULL);
#endif
}

extern int usm_handle_events(struct usm_event *event);
extern int usm_sort_events(struct list_head *events);


// extern void (*usm_restore_fallback)(struct usm_event *event);
static inline int usmPolicyDefinedSwapIn(struct usm_event * event) {
    
    if(procDescList[event->origin].swap_policy->usm_swap(event)) {   
        if(procDescList[event->origin].swap_policy->cond_swap_out)
            procDescList[event->origin].swap_policy->cond_swap_out(event);
        return 1;
    }

   
    return 0;
}

#endif