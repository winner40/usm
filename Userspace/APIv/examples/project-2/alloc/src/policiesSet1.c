#include "../incl/policiesSet1.h"

// #include "../../evict/incl/policiesSet1.h"

// #include "../../../../include/policies/evict/swap.h"


#include "../../../../../ums_bibiotheque/usm_entete.h"
#include <arpa/inet.h> 

#define PORT 8090

#define SEUIL_MAX_PAGES 5
 

#define FHP_SIGNATURE 'F'
#define FHP_SIZE_BYTES (10*4096) // 40 ko
#define FHP_NUM_PAGES (FHP_SIZE_BYTES / SYS_PAGE_SIZE) // 10 pages



/*
    Basic allocation functions serving as examples. One is required to take a lock whe(r|n)ever touching structures, as those are shared between threads.
        Per page locking proposed through -DPLock compilation option, but really not recommended.
    optEludeList can be changed to any type one'd like, but the related placeholder's quite needed (or any other method one deems wanted) to efficiently mark freed pages as such.
    The same goes for any related point. One has unlimited freedom, just with some logic in linked matters in the API we recommend to read.
*/

/*
    pthread_t internalLocks; -> to initialize in defined usm_setup....
*/

/*
    If you really don't want to use the position pointer, or add more in the case of multiple levels implied, feel free to modify usm's proposed page structure in usm.h.
    Just try not to heavify it too much.
*/




/*
    One dumb idea : reserving pools through defined functions appointed to different policies mapped to processes through the config. file (x set of functions treats of one portion of USM's memory and the other ones the rest of the latter)...

*/

pthread_mutex_t policiesSet1lock;


struct optEludeList *firstPage;
struct optAdrrList *adrr_find ;



struct list_head freeListNonZero;

LIST_HEAD(usedList);
LIST_HEAD(freeList);

LIST_HEAD(freeListNonZero);
LIST_HEAD(mylist_Adrr);

struct usm_process_swap *process_swap = NULL;

struct to_swap *toSwapr;
struct optAdrrList
{
    unsigned long adrr;
    struct list_head adrrList;
};

extern struct list_head mylist_Adrr;


LIST_HEAD(processList);


// Marqueurs FHP
struct fhp_marker {
    unsigned long base;
    int pages;
    int process;
    int allocated; // flag pour éviter les allocations multiples
    int swap_device_number;
    struct list_head list;
};

LIST_HEAD(fhp_candidates);


// struct usm_ops dev_usm_ops;         // just one for now but can be greater, i.e. multiple USM instances per arbitrary policies....


////////////////////////////////////////////////////////////////////////////////////

struct fhp_marker *find_fhp_marker(unsigned long addr, int process) {
    struct fhp_marker *mark;
    list_for_each_entry(mark, &fhp_candidates, list) {
        if (mark->process == process &&
            addr >= mark->base &&
            addr < mark->base + mark->pages * SYS_PAGE_SIZE &&
            mark->allocated == 0)
            return mark;
    }
    return NULL;
}





void *zero_out_pages_thread() {

    struct optEludeList *entry, *tmp;

    list_for_each_entry_safe(entry, tmp, &freeListNonZero, iulist) {
         
        memset((void*)entry->usmPage->data, 0, 4096);
        list_move(&entry->iulist, &freeList);
    }
  
  printf("le nombre element dans la liste est actuel dans le thread : %ld\n", list_count_nodes(&freeListNonZero));
   

}


// Fonction pour vérifier si un élément est présent dans la liste
int element_in_list(struct list_head *head, struct optAdrrList *element_to_find) {

    struct optAdrrList* current;

    list_for_each_entry(current, head, adrrList)
    {
        // Vérification si l'élément est présent
        if (current->adrr == element_to_find->adrr)
        {
            printf("L'élément est présent.\n");
            return 1;
        }
    
    }
    
    // L'élément n'est pas trouvé dans la liste
    return 0;
}


// Fonction pour vérifier si un élément est présent dans la liste
int page_in_list(struct list_head *head, struct optEludeList *page_to_find) {
    
    struct optEludeList* current;

    list_for_each_entry(current, head, iulist)
    {
        // Vérification si l'élément est présent
        if (current->usmPage == page_to_find->usmPage)
        {
            printf("La page est présente.\n");
            return 1;
        }
    
    }
    
    // L'élément n'est pas trouvé dans la liste
    return 0;
}


static inline int usm_zeroize_alloc(struct usm_event *event) {

   pthread_mutex_lock(&policiesSet1lock);
    if (list_empty(&freeList)) {
        pthread_mutex_unlock(&policiesSet1lock);       // func...
        return 1;
    }

    adrr_find = malloc(sizeof(struct optAdrrList));
    
    if (adrr_find == NULL)
    {
        printf("Erreur lors de l'allocation de mémoire.\n");
        return -1;
    }
    
    adrr_find->adrr = event->vaddr;


    if (element_in_list(&mylist_Adrr, adrr_find))
    {
        event->write = 1;
        printf("il ya adresse : oui\n");

    struct optEludeList * freeListNode = list_first_entry(&freeList, struct optEludeList, iulist);
    usedMemory+=globalPageSize;         // though.. if we just simply let the leveraging of differently defined page sizes to developers... it shouldn't be gud.. so we'll just call their functions a number globalPageSize/SYS_PAGE_SIZE of times from USM.. TODO
    list_move(&(freeListNode->iulist),&usedList);       // hold before moving... swap works open eyes.. -_-' TODO
    pthread_mutex_unlock(&policiesSet1lock);
    if(freeListNode->usmPage == NULL) {
#ifdef DEBUG
        printf("[devPolicies/Mayday] NULL page..! Aborting.\n");
        getchar();
        exit(1);
#endif
        return 1;
    }
    usmSetUsedListNode(freeListNode->usmPage, freeListNode); // freeListNode->usmPage->usedListPositionPointer=freeListNode;        // this might be why you did that.. but look, put NULL inside, do the alloc., then in free add some wait time if NULL found.. extreme cases though..
    event->paddr=freeListNode->usmPage->physicalAddress;            // we'd need the last one... buffer adding?
#ifdef DEBUG
    printf("[devPolicies] Chosen addr.:%lu\n",event->paddr);
    if (event->paddr==0)
        getchar();
#endif
    //retry:
    if(usmSubmitAllocEvent(event)) {                // will be multiple, i.e. multiplicity proposed by event, and directly applied by usmSubmitAllocEvent
#ifdef DEBUG
        printf("[devPolicies/Mayday] Unapplied allocation\n");
        //getchar();
#endif
        pthread_mutex_lock(&policiesSet1lock);
        list_move(&(freeListNode->iulist),&freeList);
        usedMemory-=globalPageSize;         // though.. if we just simply let the leveraging of differently defined page sizes to developers... it shouldn't be gud.. so we'll just call their functions a number globalPageSize/SYS_PAGE_SIZE of times from USM.. TODO
        pthread_mutex_unlock(&policiesSet1lock);
        /*pthread_mutex_lock(&procDescList[event->origin].alcLock);       // per node lock man!
        list_del(&freeListNode->proclist);
        pthread_mutex_unlock(&procDescList[event->origin].alcLock); .. now done after.. */
        return 1;
        //goto retry; // some number of times...
    }
    usmLinkPage(freeListNode->usmPage,event);
    /*freeListNode->usmPage->virtualAddress=event->vaddr;
    freeListNode->usmPage->process=event->origin;*/
    increaseProcessAllocatedMemory(event->origin, globalPageSize);

    pthread_mutex_lock(&procDescList[event->origin].alcLock);       // per node lock man!
    list_add(&freeListNode->proclist,&procDescList[event->origin].usedList);                // HUGE TODO hey, this is outrageous... do what you said and add to_swap in event, then a helper to take care of in proc. struct. list, and use the latter after basic_alloc.. i.e. in event.c and freaking take care of it there.
    freeListNode->usmPage->processUsedListPointer=&freeListNode->proclist;
    pthread_mutex_unlock(&procDescList[event->origin].alcLock);

    // ... if recognized pattern.. multiple submits.. and so on....             + VMAs list and size on the way! -> no needless long checks if applicable!
    return 0;

}
else
{
    //pthread_mutex_unlock(&policiesSet1lock);
        printf("Element pas trouve\n");
       
        usmSetUsedListNode(firstPage->usmPage, firstPage); 
        event->paddr = firstPage->usmPage->physicalAddress;
        event->write = 0;
        usedMemory+=globalPageSize;

        #ifdef DEBUG
            printf("[devPolicies] Chosen addr.:%lu\n",event->paddr);
            if (event->paddr==0)
                getchar();
        #endif
        //retry:
        if(usmSubmitAllocEvent(event)) {                // will be multiple, i.e. multiplicity proposed by event, and directly applied by usmSubmitAllocEvent

    #ifdef DEBUG
            printf("[devPolicies/Mayday] Unapplied allocation\n");
            getchar();
    #endif
           
            usedMemory-=globalPageSize;         // though.. if we just simply let the leveraging of differently defined page sizes to developers... it shouldn't be gud.. so we'll just call their functions a number globalPageSize/SYS_PAGE_SIZE of times from USM.. TODO
            return 1;
            //goto retry; // some number of times...
        }
       
        usmLinkPage(firstPage->usmPage,event);
        /*freeListNode->usmPage->virtualAddress=event->vaddr;
        freeListNode->usmPage->process=event->origin;*/
        increaseProcessAllocatedMemory(event->origin, globalPageSize);
        // ... if recognized pattern.. multiple submits.. and so on....             + VMAs list and size on the way! -> no needless long checks if applicable!          
        //pthread_mutex_lock(&policiesSet1lock);

        // Si l'élément n'est pas présent, l'ajouter à la liste
        list_add(&adrr_find->adrrList, &mylist_Adrr);
        printf("valeur de la liste : %ld\n", list_count_nodes(&mylist_Adrr));
        //printf("valeur de la newNOde de la liste : %ld\n", newNode->data.adrr);

        pthread_mutex_unlock(&policiesSet1lock);

        return 0;
}

}


static inline int basic_alloc(struct usm_event *event) {

    pthread_mutex_lock(&policiesSet1lock);
    if (list_empty(&freeList)) {
        pthread_mutex_unlock(&policiesSet1lock);       // func...
        return 1;
    }
    struct optEludeList * freeListNode = list_first_entry(&freeList, struct optEludeList, iulist);
    usedMemory+=globalPageSize;         // though.. if we just simply let the leveraging of differently defined page sizes to developers... it shouldn't be gud.. so we'll just call their functions a number globalPageSize/SYS_PAGE_SIZE of times from USM.. TODO
    list_move(&(freeListNode->iulist),&usedList);       // hold before moving... swap works open eyes.. -_-' TODO
    pthread_mutex_unlock(&policiesSet1lock);
    if(freeListNode->usmPage == NULL) {
#ifdef DEBUG
        printf("[devPolicies/Mayday] NULL page..! Aborting.\n");
        getchar();
        exit(1);
#endif
        return 1;
    }
    event->write = 1;
    //Mettre à zéro les données de la page
    //memset((void*)(freeListNode->usmPage->data), 0, 4096);
    usmSetUsedListNode(freeListNode->usmPage, freeListNode); // freeListNode->usmPage->usedListPositionPointer=freeListNode;        // this might be why you did that.. but look, put NULL inside, do the alloc., then in free add some wait time if NULL found.. extreme cases though..
    event->paddr=freeListNode->usmPage->physicalAddress;            // we'd need the last one... buffer adding?
#ifdef DEBUG
    printf("[devPolicies] Chosen addr.:%lu\n",event->paddr);
     printf("[devPolicies] Chosen vaddr.:%lu\n",event->vaddr);
    if (event->paddr==0)
        getchar();
#endif
    //retry:
    if(usmSubmitAllocEvent(event)) {                // will be multiple, i.e. multiplicity proposed by event, and directly applied by usmSubmitAllocEvent
#ifdef DEBUG
        printf("[devPolicies/Mayday] Unapplied allocation\n");
        //getchar();
#endif
        pthread_mutex_lock(&policiesSet1lock);
        list_move(&(freeListNode->iulist),&freeList);
        usedMemory-=globalPageSize;         // though.. if we just simply let the leveraging of differently defined page sizes to developers... it shouldn't be gud.. so we'll just call their functions a number globalPageSize/SYS_PAGE_SIZE of times from USM.. TODO
        pthread_mutex_unlock(&policiesSet1lock);
        /*pthread_mutex_lock(&procDescList[event->origin].alcLock);       // per node lock man!
        list_del(&freeListNode->proclist);
        pthread_mutex_unlock(&procDescList[event->origin].alcLock); .. now done after.. */
        return 1;
        //goto retry; // some number of times...
    }
    usmLinkPage(freeListNode->usmPage,event);
    /*freeListNode->usmPage->virtualAddress=event->vaddr;
    freeListNode->usmPage->process=event->origin;*/
    increaseProcessAllocatedMemory(event->origin, globalPageSize);

    pthread_mutex_lock(&procDescList[event->origin].alcLock);       // per node lock man!
    list_add(&freeListNode->proclist,&procDescList[event->origin].usedList);                // HUGE TODO hey, this is outrageous... do what you said and add to_swap in event, then a helper to take care of in proc. struct. list, and use the latter after basic_alloc.. i.e. in event.c and freaking take care of it there.
    freeListNode->usmPage->processUsedListPointer=&freeListNode->proclist;
    pthread_mutex_unlock(&procDescList[event->origin].alcLock);

    // ... if recognized pattern.. multiple submits.. and so on....             + VMAs list and size on the way! -> no needless long checks if applicable!
    return 0;

}



static inline int usm_alloc_zeroize(struct usm_event *event) {

    pthread_mutex_lock(&policiesSet1lock);
    if (list_empty(&freeList)) {
        if(list_count_nodes(&freeListNonZero) > 0)
        {
            pthread_t tid;
            pthread_create(&tid, NULL, zero_out_pages_thread, NULL);
            pthread_join(tid, NULL);

        } else
        {
            pthread_mutex_unlock(&policiesSet1lock);
            return 1;
        }  
    }
    printf("%ld\n", event->vaddr);
    struct optEludeList * freeListNode = list_first_entry(&freeList, struct optEludeList, iulist);
    usedMemory+=globalPageSize;         // though.. if we just simply let the leveraging of differently defined page sizes to developers... it shouldn't be gud.. so we'll just call their functions a number globalPageSize/SYS_PAGE_SIZE of times from USM.. TODO
    list_move(&(freeListNode->iulist),&usedList);       // hold before moving... swap works open eyes.. -_-' TODO
    pthread_mutex_unlock(&policiesSet1lock);
    if(freeListNode->usmPage == NULL) {
#ifdef DEBUG
        printf("[devPolicies/Mayday] NULL page..! Aborting.\n");
        getchar();
        exit(1);
#endif
        return 1;
    }
    usmSetUsedListNode(freeListNode->usmPage, freeListNode); // freeListNode->usmPage->usedListPositionPointer=freeListNode;        // this might be why you did that.. but look, put NULL inside, do the alloc., then in free add some wait time if NULL found.. extreme cases though..
    event->paddr=freeListNode->usmPage->physicalAddress;            // we'd need the last one... buffer adding?
#ifdef DEBUG
    printf("[devPolicies] Chosen addr.:%lu\n",event->paddr);
    if (event->paddr==0)
        getchar();
#endif
    //retry:
    if(usmSubmitAllocEvent(event)) {                // will be multiple, i.e. multiplicity proposed by event, and directly applied by usmSubmitAllocEvent
#ifdef DEBUG
        printf("[devPolicies/Mayday] Unapplied allocation\n");
        //getchar();
#endif
        pthread_mutex_lock(&policiesSet1lock);
        list_move(&(freeListNode->iulist),&freeList);
        usedMemory-=globalPageSize;         // though.. if we just simply let the leveraging of differently defined page sizes to developers... it shouldn't be gud.. so we'll just call their functions a number globalPageSize/SYS_PAGE_SIZE of times from USM.. TODO
        pthread_mutex_unlock(&policiesSet1lock);
        /*pthread_mutex_lock(&procDescList[event->origin].alcLock);       // per node lock man!
        list_del(&freeListNode->proclist);
        pthread_mutex_unlock(&procDescList[event->origin].alcLock); .. now done after.. */
        return 1;
        //goto retry; // some number of times...
    }
    usmLinkPage(freeListNode->usmPage,event);
    /*freeListNode->usmPage->virtualAddress=event->vaddr;
    freeListNode->usmPage->process=event->origin;*/
    increaseProcessAllocatedMemory(event->origin, globalPageSize);

    pthread_mutex_lock(&procDescList[event->origin].alcLock);       // per node lock man!
    list_add(&freeListNode->proclist,&procDescList[event->origin].usedList);                // HUGE TODO hey, this is outrageous... do what you said and add to_swap in event, then a helper to take care of in proc. struct. list, and use the latter after basic_alloc.. i.e. in event.c and freaking take care of it there.
    freeListNode->usmPage->processUsedListPointer=&freeListNode->proclist;
    pthread_mutex_unlock(&procDescList[event->origin].alcLock);

    // ... if recognized pattern.. multiple submits.. and so on....             + VMAs list and size on the way! -> no needless long checks if applicable!
    return 0;
  
}


static inline int basic_c_alloc(struct usm_event *event) {
    if (basic_c_alloc(event))
        return 1;
    while (1) {
        event->vaddr+=SYS_PAGE_SIZE;
        printf("Trying again..\n");
        //getchar();
        if (basic_c_alloc(event)) {
            printf("Out!\n");
            break;
        }
    }
    return 0;
}

static inline int double_alloc(struct usm_event *event) {
    if (basic_alloc(event))
        return 1;
    event->vaddr+=SYS_PAGE_SIZE;        // spatial distrib. file could tell the next vaddr to allocate!
    if (basic_alloc(event)) {         // second one's not obliged to work... i.e. VMA's full..&Co.
        /*
        pthread_mutex_lock(&policiesSet1lock);
        list_move(&((struct optEludeList *)usmPfnToUsedListNode(usmPfnToPageIndex(event->paddr)))->iulist,&freeList); //list_move(&((struct optEludeList *)usmPageToUsedListNode(usmEventToPage(event)))->iulist,&freeList);
        pthread_mutex_unlock(&policiesSet1lock);
        usmResetUsedListNode(usmEventToPage(event)); //pagesList[usmPfnToPageIndex(event->paddr)].usedListPositionPointer=NULL;

        ..doesn't make sense..
        */
#ifdef DEBUG
        printf("[devPolicies/Sys] Unapplied allocation.. though not mandatory.\n");
        getchar();
#endif
    }
    return 0;
}

/*static inline struct page * pair_pages_alloc() {
    pthread_mutex_lock(&policiesSet1lock);
    if (list_empty(&freeList)){
        pthread_mutex_unlock(&policiesSet1lock);
        return NULL;
    }
    struct optEludeList * freeListNode = list_first_entry(&freeList, struct optEludeList, iulist);
    while ((freeListNode->usmPage)->physicalAddress%2!=0) {
        if (list_empty(&(freeListNode)->iulist)){
            pthread_mutex_unlock(&policiesSet1lock);
            return NULL;
        }
        freeListNode=list_first_entry(&(freeListNode)->iulist, struct optEludeList, iulist);
    }
    usedMemory+=SYS_PAGE_SIZE;
    freeListNode->usmPage->usedListPositionPointer=freeListNode;    // toMove....
    list_move(&(freeListNode->iulist),&usedList);
    pthread_mutex_unlock(&policiesSet1lock);

    //ret->virtualAddress=va;

    return freeListNode->usmPage;
}*/

int reverse_alloc(struct usm_event *event) {
#ifdef DEBUG
    printf("[devPolicies]Reversing!\n");
    //getchar();
#endif
    pthread_mutex_lock(&policiesSet1lock);
    if (list_empty(&freeList)) {
        pthread_mutex_unlock(&policiesSet1lock);
        return 1;
    }
    struct optEludeList * freeListNode = list_entry(freeList.prev, struct optEludeList, iulist);
    usedMemory+=globalPageSize;
    freeListNode->usmPage->usedListPositionPointer=freeListNode;
    list_move(&(freeListNode->iulist),&usedList);
    pthread_mutex_unlock(&policiesSet1lock);
    event->paddr=freeListNode->usmPage->physicalAddress;
#ifdef DEBUG
    printf("[devPolicies]Chosen addr.:%lu\n",event->paddr);
    if(event->paddr==0)
        getchar();
#endif
    if(usmSubmitAllocEvent(event)) {
#ifdef DEBUG
        printf("[devPolicies/Mayday] Unapplied allocation\n");
        getchar();
#endif
        pthread_mutex_lock(&policiesSet1lock);
        list_move(&(freeListNode->iulist),&freeList);
        usedMemory-=globalPageSize;
        pthread_mutex_unlock(&policiesSet1lock);
        return 1;
    }
    increaseProcessAllocatedMemory(event->origin, globalPageSize);
    freeListNode->usmPage->virtualAddress=event->vaddr;
    freeListNode->usmPage->process=event->origin;
    return 0;
}

static inline int basic_free(struct usm_event * event) {
    int ret = 0;
    struct optEludeList * tempEntry;
    list_for_each_entry(tempEntry,&usedList,iulist) {                       // per process.... DI.
        if (tempEntry->usmPage->virtualAddress==event->vaddr)
            break;
    }
    if(&tempEntry->iulist==&usedList) {
        printf("[devPolicies/Mayday] Corresponding page not found!\n");
        ret=1;
        goto out;
    }
    pthread_mutex_lock(&policiesSet1lock);
    list_move(&tempEntry->iulist,&freeList);
    usedMemory-=SYS_PAGE_SIZE;
    pthread_mutex_unlock(&policiesSet1lock);
    // list_del and free of per proc usedList.. (basic_free's essentially not used.., so later..)
    memset((void*)((tempEntry->usmPage)->data), 0, 4096);
#ifdef DEBUG
    printf("\t%lu | %lu\n", event->vaddr, ((struct page *) (tempEntry->usmPage))->physicalAddress);
#endif
    tempEntry->usmPage->virtualAddress=0;
    decreaseProcessAllocatedMemory(tempEntry->usmPage->process, SYS_PAGE_SIZE);
    tempEntry->usmPage->process=0;
    tempEntry->usmPage->usedListPositionPointer=NULL;
out:
    return ret;
}


/////////////////////////////////////////////////////////////////////

int fhp_free(struct usm_event *event) {
    struct fhp_marker *fhp;

    list_for_each_entry(fhp, &fhp_candidates, list) {
        if (event->vaddr >= fhp->base &&
            event->vaddr < fhp->base + fhp->pages * SYS_PAGE_SIZE) {

            printf("[FHP] Libération de la FHP : base=%p, pages=%d\n", (void*)fhp->base, fhp->pages);

            for (int i = 0; i < fhp->pages; i++) {
                unsigned long vaddr = fhp->base + i * SYS_PAGE_SIZE;
                event->vaddr = vaddr;

                struct page *pg = usmEventToPage(event);
                if (!pg) continue;

                struct optEludeList *usedNode = usmPageToUsedListNode(pg);
                if (!usedNode) continue;

                if (pg->processUsedListPointer != NULL) {
                    pthread_mutex_lock(&procDescList[pg->process].alcLock);
                    list_del_init(pg->processUsedListPointer);
                    pthread_mutex_unlock(&procDescList[pg->process].alcLock);
                }

                pthread_mutex_lock(&policiesSet1lock);
                list_move_tail(&usedNode->iulist, &freeList);
                memset((void*)pg->data, 0, SYS_PAGE_SIZE);
                pg->virtualAddress = 0;
                pg->process = 0;
                pg->usedListPositionPointer = NULL;
                usedMemory -= SYS_PAGE_SIZE;
                decreaseProcessAllocatedMemory(fhp->process, SYS_PAGE_SIZE);
                pthread_mutex_unlock(&policiesSet1lock);

                printf("[FHP] Page libérée : vaddr=%p\n", (void*)vaddr);
            }

            // Supprimer le marqueur de la liste
            pthread_mutex_lock(&policiesSet1lock);
            list_del(&fhp->list);
            pthread_mutex_unlock(&policiesSet1lock);
            free(fhp);

            return 0;
        }
    }

    return 1; // Aucun FHP trouvé
}





static inline int pindex_free(struct usm_event *event) {
    struct page *pg = usmEventToPage(event);
    if (!pg) {
        printf("[FHP/pindex] Aucune page trouvée pour vaddr=%p\n", (void*)event->vaddr);
        return 1;
    }

    // Vérifie si la page appartient à une FHP
    struct fhp_marker *fhp = find_fhp_marker(pg->virtualAddress, pg->process);
    if (fhp) {
        printf("[FHP/pindex] Redirection vers fhp_free pour vaddr=%p\n", (void*)pg->virtualAddress);
        event->vaddr = fhp->base; // important !
        return fhp_free(event);
    }

    // Fallback classique
    if (!usmPageToUsedListNode(pg)) {
        printf("[pindex] No usedListNode → basic_free fallback pour vaddr=%p\n", (void*)event->vaddr);
        return basic_free(event);
    }

    pthread_mutex_lock(&procDescList[pg->process].alcLock);
    list_del_init(pg->processUsedListPointer);
    pthread_mutex_unlock(&procDescList[pg->process].alcLock);

    pthread_mutex_lock(&policiesSet1lock);
    struct optEludeList *node = usmPageToUsedListNode(pg);
    list_move_tail(&node->iulist, &freeList);
    memset((void*)pg->data, 0, SYS_PAGE_SIZE);
    pg->virtualAddress = 0;
    pg->process = 0;
    pg->usedListPositionPointer = NULL;
    usedMemory -= SYS_PAGE_SIZE;
    decreaseProcessAllocatedMemory(event->origin, SYS_PAGE_SIZE);
    pthread_mutex_unlock(&policiesSet1lock);

    return 0;
}




/* Returns the remaining pages that couldn't be taken */
int pick_pages(struct list_head * placeholder, int nr){
    int nbr = nr;
    pthread_mutex_lock(&policiesSet1lock);
    while (nr>0) {
        if (unlikely(list_empty(&freeList)))
            break; 
        struct optEludeList * chosenPage = list_first_entry(&freeList, struct optEludeList, iulist);    /* This can be further specialized */ // and man, no need to containerOut and containerIn... man...
        list_move(&chosenPage->iulist,placeholder);
        usedMemory+=SYS_PAGE_SIZE;          // some other "held" or "temporary" memory might be cool..
        nr--;
    }
    pthread_mutex_unlock(&policiesSet1lock);
    return nbr-nr;
}

void give_back_pages(struct list_head * pages) {        // given list_head should always be sanitized..
    struct list_head *pagesIterator, *tempPageItr;
    int count = 1;
    pthread_mutex_lock(&policiesSet1lock);
    list_for_each_safe(pagesIterator,tempPageItr,pages) {
        printf("Giving back one page..\n");
        //struct optEludeList * polPage = list_entry(pagesIterator, struct optEludeList, iulist);
        //memset((void*)(polPage->usmPage->data), 0, 4096);
        list_move_tail(pagesIterator,&freeList);                 // some unneeded list_del.. maybe some verification after the pages' putting...
        usedMemory-=SYS_PAGE_SIZE;                          // some special variable containing "will be used" pages that still aren't..?
        count--;
    }
    if (count){
        //struct optEludeList * polPage = list_entry(pages, struct optEludeList, iulist);
        //memset((void*)(polPage->usmPage->data), 0, 4096);
        list_add_tail(pages,&freeList);     // temp..
        usedMemory-=SYS_PAGE_SIZE;
    }
    pthread_mutex_unlock(&policiesSet1lock);
#ifdef DEBUG
        printf("Memory consumed : %.2f%s #swapPage\n", usedMemory/1024/1024>0?(float)usedMemory/1024/1024:(float)usedMemory/1024, usedMemory/1024/1024>0?"MB":"KB");    // should be done by dev.?
#endif


}

void give_back_page_used(struct list_head * page) {         // (plural) too version maybe
    pthread_mutex_lock(&policiesSet1lock);
    list_add(page,&usedList);
    pthread_mutex_unlock(&policiesSet1lock);
}

void hold_used_page_commit(struct list_head * page){
    pthread_mutex_lock(&policiesSet1lock);
    list_del_init(page);         // _init probably not needed.. | not so sure anymore... deffo needed! 
    pthread_mutex_unlock(&policiesSet1lock);
}


static inline int usm_pindex_free_zeroize (struct usm_event * event) {

    struct page * usmPage = usmEventToPage(event);
    if(unlikely(!usmPage)) {
#ifdef DEBUG
        printf("[devPolicies/Mayday] Event corresponding to no page\n");
#endif
        return 1;
    }
    if (unlikely(!usmPageToUsedListNode(usmPage))) {
        printf("[devPolicies/Sys] Calling basic free!\n");
        getchar();
        event->vaddr=usmPage->virtualAddress;
        return basic_free(event);
    }

    pthread_mutex_lock(&procDescList[usmPage->process].alcLock);
    list_del_init(usmPage->processUsedListPointer);                         // example of a simplification of need of use of usmPageToUsedListNode..
    pthread_mutex_unlock(&procDescList[usmPage->process].alcLock);

    //memset((void*)(usmPage->data), 0, 4096);
#ifdef DEBUG
    printf("\t[devPolicies]Collecting freed %lu\n", usmPage->physicalAddress);
#endif
    pthread_mutex_lock(&policiesSet1lock);


    if (list_count_nodes(&freeListNonZero) == SEUIL_MAX_PAGES)
    {
        pthread_t tid;
        pthread_create(&tid, NULL, zero_out_pages_thread, NULL);
        //pthread_detach(tid);
        pthread_join(tid, NULL);
    }

    list_move(&((struct optEludeList *)usmPageToUsedListNode(usmPage))->iulist,&freeListNonZero);

    usmPage->virtualAddress=0;
    decreaseProcessAllocatedMemory(usmPage->process, SYS_PAGE_SIZE);     // can and will always only be of SYS_PAGE_SIZE...
    usmPage->process=0;
    usedMemory-=SYS_PAGE_SIZE;
    pthread_mutex_unlock(&policiesSet1lock);
    printf("le nombre element dans la liste est actuel : %ld\n", list_count_nodes(&freeListNonZero));
    return 0;

}


int jocelyn_hints_alloc2(struct usm_event *event){

    int n=0;
    char value;
    void *temp;
    char espace_swap[30];
    char *content = "HA!";

    sscanf(event->data, "%c;%d;%p;%s", &value, &n, &temp,espace_swap);

    unsigned long adresse_virtuelle = (unsigned long)temp;

    // Utilisation de la valeur conve.rtie
    event->vaddr = adresse_virtuelle;

    int i;
    
    if (value == 'A')
    {
        for (int i = 0; i < n; i++)
        {
            if (basic_alloc(event)!=0) {
                //printf("Échec de l'allocation pour la page %d\n", i + 1);
                break;
                return -1;
            }

            event->vaddr = event->vaddr + 4096;
        }  

        char *donnee = strdup("Tout s'est bien passé");
        event->taille=strlen(donnee);
        bzero(event->data,event->taille);
        strcpy(event->data,donnee);
        printf("event data : %s\n", event->data);
        printf("event taille : %d\n", event->taille);
        
        if(usmSubmitHintReturnEvent(event)) {
            printf("Retour du hint à l'utilisateur success %s\n", event->data);
            free(donnee);

            return 0;
        }


    }


    return 0;

}

int jocelyn_hints_alloc(struct usm_event *event){

    int n=0;
    char value;
    void *temp;
    char espace_swap[30];
    char *content = "HA!";

    sscanf(event->data, "%c;%d;%p", &value, &n, &temp);

    unsigned long adresse_virtuelle = (unsigned long)temp;

   //event->type = ALLOC;

    // Utilisation de la valeur conve.rtie
    event->vaddr = adresse_virtuelle;

    int i;
    
    if (value == 'A')
    {
        for (int i = 0; i < n; i++)
        {
          
            if (basic_alloc(event)!=0) {
                //printf("Échec de l'allocation pour la page %d\n", i + 1);
                break;
                return -1;
            }
          
            event->vaddr = event->vaddr + 4096;

        }

        process_swap = (struct usm_process_swap *)malloc(sizeof(struct usm_process_swap));

        process_swap->swapNode = (struct to_swap *)malloc(sizeof(struct to_swap));

        // Allocation de mémoire pour swapDevice
        process_swap->swapNode->swapDevice = (struct usm_swap_dev *)malloc(sizeof(struct usm_swap_dev));

        process_swap->swapNode->swapDevice->number = 0;

        process_swap->swapNode->page = NULL;

        if (strcmp(espace_swap, "tmp")==0)
        {
           process_swap->process = event->origin;
           process_swap->swapNode->swapDevice->number = 1;
        }
        else if (strcmp(espace_swap, "tmpfs")==0)
        {
           process_swap->process = event->origin;
           process_swap->swapNode->swapDevice->number = 2;
        }
        else
        {
            printf("Espace de swap non disponible\n");
        }

        list_add(&process_swap->iulist, &processList);

        printf("list count process : %ld\n", list_count_nodes(&processList));
        struct usm_process_swap *swapProcess = list_first_entry(&processList, struct usm_process_swap, iulist);

         printf("list number process : %d\n", swapProcess->swapNode->swapDevice->number);

         

        char *donnee = strdup("Tout s'est bien passé");
        event->taille=strlen(donnee);
        bzero(event->data,event->taille);
        strcpy(event->data,donnee);
        printf("event data : %s\n", event->data);
        printf("event taille : %d\n", event->taille);
        
        if(usmSubmitHintReturnEvent(event)) {
            printf("Retour du hint à l'utilisateur success %s\n", event->data);
            free(donnee);

            return 0;
        }
    
    }


    return 0;

}




/////////////////////////////////////////////////////////////
// Hint FHP

static inline int usm_hints_fhp(struct usm_event *event) {
    char value;
    void *addr;
    char swap_type[32]; 

    // Parse : F;adresse;type_swap
    int matched = sscanf(event->data, "%c;%p;%s", &value, &addr, swap_type);
    if (matched != 3 || value != FHP_SIGNATURE) {
        printf("[FHP] Hint mal formé ou signature invalide : %s\n", event->data);
        return 1;
    }

    unsigned long vaddr = (unsigned long)addr;

    struct fhp_marker *zone = malloc(sizeof(struct fhp_marker));
    if (!zone) {
        perror("[FHP] malloc échoué");
        return 1;
    }

    zone->base = vaddr;
    zone->pages = FHP_NUM_PAGES;
    zone->process = event->origin;
    zone->allocated = 0;

    // Détermination du numéro de swap device selon le type
    if (strcmp(swap_type, "tmp") == 0) {
        zone->swap_device_number = 1;
    } else if (strcmp(swap_type, "tmpfs") == 0) {
        zone->swap_device_number = 2;
    } else {
        printf("[FHP] Type de swap inconnu : %s\n", swap_type);
        free(zone);
        return 1;
    }

    INIT_LIST_HEAD(&zone->list);
    list_add_tail(&zone->list, &fhp_candidates);

    printf("[FHP] Zone enregistrée: base=%p, pages=%d, swapDevice=%d, process=%d\n",
           (void *)zone->base, zone->pages, zone->swap_device_number, zone->process);

    // Répondre à l'utilisateur
    char *msg = strdup("Zone FHP enregistrée avec swap associé");
    event->taille = strlen(msg);
    bzero(event->data, event->taille);
    strcpy(event->data, msg);

    usmSubmitHintReturnEvent(event);
    free(msg);

    return 0;
}



static inline int basic_alloc_hint(struct usm_event *event) {
    pthread_mutex_lock(&policiesSet1lock);

    if (list_empty(&freeList)) {
        printf("[FHP] ❌ Échec : freeList vide\n");
        pthread_mutex_unlock(&policiesSet1lock);
        return 1;
    }

    struct fhp_marker *fhp = find_fhp_marker(event->vaddr, event->origin);

    // Cas 1 : Hint détecté et non encore alloué
    if (fhp && fhp->allocated == 0) {
        // Vérifier qu'on a assez de pages libres pour toute la FHP
        if (list_count_nodes(&freeList) < fhp->pages) {
            printf("[FHP] ❌ Pas assez de pages libres pour allouer FHP (disponibles=%ld, requis=%d)\n",
                   list_count_nodes(&freeList), fhp->pages);
            pthread_mutex_unlock(&policiesSet1lock);
            return 1;
        }

        printf("[FHP] ✅ Hint détecté : allocation groupée de %d pages pour le processus %d à partir de %p\n",
               fhp->pages, event->origin, (void*)fhp->base);

        for (int i = 0; i < fhp->pages; i++) {
            unsigned long vaddr = fhp->base + i * SYS_PAGE_SIZE;

            struct optEludeList *freeListNode = list_first_entry(&freeList, struct optEludeList, iulist);
            list_move(&freeListNode->iulist, &usedList);
            usedMemory += globalPageSize;

            if (freeListNode->usmPage == NULL) {
                printf("[FHP] ❌ Page NULL lors de l’allocation\n");
                pthread_mutex_unlock(&policiesSet1lock);
                return 1;
            }

            struct usm_event temp_evt = {
                .vaddr = vaddr,
                .origin = event->origin,
                .write = 1
            };

            usmSetUsedListNode(freeListNode->usmPage, freeListNode);
            temp_evt.paddr = freeListNode->usmPage->physicalAddress;

            usmLinkPage(freeListNode->usmPage, &temp_evt);
            increaseProcessAllocatedMemory(temp_evt.origin, globalPageSize);

            pthread_mutex_lock(&procDescList[temp_evt.origin].alcLock);
            list_add(&freeListNode->proclist, &procDescList[temp_evt.origin].usedList);
            freeListNode->usmPage->processUsedListPointer = &freeListNode->proclist;
            pthread_mutex_unlock(&procDescList[temp_evt.origin].alcLock);

            if (usmSubmitAllocEvent(&temp_evt)) {
                printf("[FHP] ❌ usmSubmitAllocEvent a échoué pour vaddr=%p\n", (void *)vaddr);
                pthread_mutex_unlock(&policiesSet1lock);
                return 1;
            }

            if (vaddr == event->vaddr) {
                event->paddr = temp_evt.paddr;
            }
        }

        fhp->allocated = 1;
        pthread_mutex_unlock(&policiesSet1lock);
        printf("[FHP] ✅ Allocation complète terminée pour FHP\n");
        return 0;
    }

    // Cas 2 : Pas de hint → comportement normal
    printf("[FHP] ℹ️ Aucun hint trouvé, fallback allocation classique\n");

    struct optEludeList *freeListNode = list_first_entry(&freeList, struct optEludeList, iulist);
    list_move(&freeListNode->iulist, &usedList);
    usedMemory += globalPageSize;

    if (freeListNode->usmPage == NULL) {
        pthread_mutex_unlock(&policiesSet1lock);
        return 1;
    }

    usmSetUsedListNode(freeListNode->usmPage, freeListNode);
    event->paddr = freeListNode->usmPage->physicalAddress;
    event->write = 1;
    usmLinkPage(freeListNode->usmPage, event);
    increaseProcessAllocatedMemory(event->origin, globalPageSize);

    pthread_mutex_lock(&procDescList[event->origin].alcLock);
    list_add(&freeListNode->proclist, &procDescList[event->origin].usedList);
    freeListNode->usmPage->processUsedListPointer = &freeListNode->proclist;
    pthread_mutex_unlock(&procDescList[event->origin].alcLock);

    pthread_mutex_unlock(&policiesSet1lock);

    if (usmSubmitAllocEvent(event)) {
        printf("[FHP] ❌ usmSubmitAllocEvent a échoué (fallback)\n");
        return 1;
    }

    printf("[FHP] ✅ Fallback allocation : vaddr=%p -> paddr=%lu\n", (void *)event->vaddr, event->paddr);
    return 0;
}






// Nouveau comportement de hints
int hints_alloc(struct usm_event *event) {
    printf("Hint reçu : %s\n", event->data);

    if (event->data[0] == FHP_SIGNATURE)
        return usm_hints_fhp(event);

    return 0;
}



int jocelyn_init (){
   
    //initialise ta variable zero_page
    //la zeroizer

    firstPage = list_first_entry(&freeList, struct optEludeList, iulist);
    list_del(&firstPage->iulist);

    printf("je suis ici : %ld\n", firstPage->usmPage->physicalAddress);
    memset((void*)firstPage->usmPage->data, 0, 4096);

    return 0;
}


struct usm_alloc_policy_ops usm_basic_alloc_policy= {.usm_zeroize_alloc=usm_zeroize_alloc,.usm_pindex_free_zeroize=usm_pindex_free_zeroize,.usm_alloc_zeroize= usm_alloc_zeroize, .usm_init=jocelyn_init, .usm_alloc=basic_alloc,.usm_pindex_free=pindex_free,.usm_free=basic_free, .usm_hints=jocelyn_hints_alloc};
struct usm_alloc_policy_ops alloc_policy_one= {.usm_zeroize_alloc=usm_zeroize_alloc,.usm_pindex_free_zeroize=usm_pindex_free_zeroize,.usm_alloc_zeroize= usm_alloc_zeroize,.usm_init=jocelyn_init,.usm_alloc=reverse_alloc, .usm_pindex_free=pindex_free,.usm_free=basic_free, .usm_hints=jocelyn_hints_alloc};
struct usm_alloc_policy_ops alloc_policy_double= {.usm_zeroize_alloc=usm_zeroize_alloc,.usm_pindex_free_zeroize=usm_pindex_free_zeroize, .usm_alloc_zeroize= usm_alloc_zeroize,.usm_init=jocelyn_init,.usm_alloc=double_alloc, .usm_pindex_free=pindex_free,.usm_free=basic_free, .usm_hints=jocelyn_hints_alloc};
struct usm_alloc_policy_ops alloc_policy_hint= {.usm_zeroize_alloc=usm_zeroize_alloc,.usm_pindex_free_zeroize=usm_pindex_free_zeroize, .usm_alloc_zeroize= usm_alloc_zeroize,.usm_init=jocelyn_init,.usm_alloc=basic_alloc_hint, .usm_pindex_free=pindex_free,.usm_free=fhp_free, .usm_hints=hints_alloc};


int policiesSet1_setup(unsigned int pagesNumber) {         // alloc.* setup..
    for (int i = 0; i<pagesNumber; i++) {
        struct optEludeList *freeListNode=(struct optEludeList *)malloc(sizeof(struct optEludeList));
        freeListNode->usmPage=pagesList+i;  // param.
        INIT_LIST_HEAD(&(freeListNode->iulist));
        list_add(&(freeListNode->iulist),&freeList);
    }
    if(usm_register_alloc_policy(&alloc_policy_one,"policyOne",false))
        return 1;
    if(usm_register_alloc_policy(&usm_basic_alloc_policy,"basicPolicy",false))
        return 1;
    if(usm_register_alloc_policy(&alloc_policy_double,"doublePolicy",false))
        return 1;
    if(usm_register_alloc_policy(&alloc_policy_hint,"hintPolicy",true))
        return 1;
    pthread_mutex_init(&policiesSet1lock,NULL);
    get_pages=&pick_pages;
    put_pages=&give_back_pages;     // retake/redeem/reclaim(nahh..)_'Pages
    restore_used_page=&give_back_page_used;
    hold_used_page=&hold_used_page_commit;
    // pthread_create... of any policy/locally defined thread doing anything upon live stat.s proposed by USM
    return 0;
}

void pol_structs_free() {
    struct list_head *listIterator, *tempLstIterator;
    list_for_each_safe(listIterator,tempLstIterator,&(freeList)) {
        struct optEludeList * listNode=list_entry(listIterator, struct optEludeList, iulist);
        list_del(listIterator);
        free(listNode->usmPage);
        free(listNode);
    }
    list_for_each_safe(listIterator,tempLstIterator,&(usedList)) {
        struct optEludeList * listNode=list_entry(listIterator, struct optEludeList, iulist);
        list_del(listIterator);
        free(listNode->usmPage);
        free(listNode);
        // free(process_swap);
        // free(process_swap->swapNode);
        // free(process_swap->swapNode->swapDevice);

        //free(adrr_find);
    }
    // pthread_join.. of the aforedefined ones.
}

struct usm_ops dev_usm_ops= {
usm_setup:
    policiesSet1_setup,
usm_free:
    pol_structs_free
};