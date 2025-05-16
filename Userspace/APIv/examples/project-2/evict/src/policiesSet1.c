
#include "../incl/policiesSet1.h"

struct to_swap * toSwapNode;

//struct to_swap_time *toSwapTime;

struct usm_swap_dev *selectedDeviceOut;

char * swapFileTmpName;
char * swapFileRamFsName;
char * swapFileName;
// First things first, shared pages -> out... we'll tag them in usm's page struct, I guess... temporarily.. TODO
// Maybe the separation of select_pages (and a mode, toEvict, toRegive) makes some sense... i.e. the complexification with descriptions (spatial distrib.s)...

pthread_mutex_t policiesSet1Swaplock;

struct evict_worker evictWorkers;
LIST_HEAD(swapList);
LIST_HEAD(swapListTime);
LIST_HEAD(timeList);
//struct usm_process_swap *process_swap;


// Déclarations des fonctions numberToSwapDev et numberToSwapDev_two
struct usm_swap_dev *numberToSwapDev(int n);
struct usm_swap_dev *numberToSwapDev_two(int n);

struct usm_swap_dev *pick_swap_device(int proc);
struct usm_swap_dev *pick_swap_device_two(int proc);

struct list_head * getSwapNode(struct usm_swap_dev * swapDevice){
    struct list_head * swap_node=NULL;
    pthread_mutex_lock(&swapDevice->devListLock);
    if (likely(!list_empty(&swapDevice->free_list))) {
        swap_node=swapDevice->free_list.next;
        list_del(swap_node);     // some .._init if some new list to be created later..
    }
    else
        printf("\tMayday, empty!\n");
    pthread_mutex_unlock(&swapDevice->devListLock);
    return swap_node;
}

void putSwapNode(struct usm_swap_dev * swapDevice, struct list_head * swapNode){
    pthread_mutex_lock(&swapDevice->devListLock);
    list_add(swapNode,&swapDevice->free_list);
    pthread_mutex_unlock(&swapDevice->devListLock);
}


// Fonction pour vérifier et déplacer les pages swappées
void *check_and_move_swapped_page()
{
    time_t current_time_check = time(NULL);
    struct to_swap *swap_move;
    struct to_swap_time *swap_time_entry;
    int count = 0;

    printf("temps actuelle pour swapper out : %ld\n", list_count_nodes(&swapListTime));

    if (!list_empty(&swapList) && !list_empty(&swapListTime))
    {
        pthread_mutex_lock(&policiesSet1Swaplock);
        list_for_each_entry(swap_move, &swapList, globlist)
        {
            printf("swapped_address list node : %ld\n", list_count_nodes(&swapListTime));

            if(count>= list_count_nodes(&swapListTime))
                break;

            swap_time_entry = list_entry((&swapListTime)->next, struct to_swap_time, glolist);

            unsigned long diff_secondes = difftime(current_time_check, swap_time_entry->swap_time_out);

            printf("valeur de la difference : %ld\n", diff_secondes);

            if((long)diff_secondes > 3  && swap_move->swapDevice->number == 2) {

                struct usm_swap_dev *selectedDevices = pick_swap_device_two(swap_move->proc);
                if (fseeko((FILE *)selectedDevices->backend,swap_move->snode->offset, SEEK_SET)) {
                    perror("[swapDevPols/sp_out/Mayday] Échec du déplacement du curseur de fichier\n");
                    return NULL;
                }

                char empty_data[4096] = {0}; // Données vides
                if (fwrite(empty_data, SYS_PAGE_SIZE, 1, (FILE *)selectedDevices->backend) != 1){
                    perror("[swapDevPols/sp_out/Mayday] Échec de l'écriture de données vides\n");
                    return NULL;
                }

                // Créer une nouvelle instance de struct usm_swap_dev
                struct usm_swap_dev *selectedDeviceOne = malloc(sizeof(struct usm_swap_dev));
                memcpy(selectedDeviceOne, pick_swap_device(swap_move->proc), sizeof(struct usm_swap_dev));

                // Affecter la nouvelle instance à swap_move
                swap_move->swapDevice = selectedDeviceOne;

                swap_time_entry->to_swap = swap_move;

                list_del(&swap_move->globlist); // Supprimer l'ancienne instance
                list_add(&swap_time_entry->to_swap->globlist, &swapList);

                printf(" swap_move number ici : %d\n",swap_move->swapDevice->number);

                if (unlikely(swap_move->swapDevice->swap_dev_ops.swap_time(swap_time_entry)))
                    {
                        #ifdef DEBUG
                                printf("Swapped out move failled!!\n");
                        #endif
                    }
            }

            list_del(&swap_time_entry->glolist);
        }

        pthread_mutex_unlock(&policiesSet1Swaplock);
    }
    else
    {
        printf("liste vide check and move\n");
    }

    list_for_each_entry(swap_move, &swapList, globlist)
    {
        printf("swapped_address to_swap : %ld\n", swap_move->swapped_address);
        printf("swapped_address number : %d\n",swap_move->swapDevice->number);
    }

    return NULL;
}


int swap_out(struct to_swap * victimNode) {        // promotion / demotion to be done "in line".. i.e. the backfile'd be replaced while copying..all.. the... content.... o_o' TODO further diggin'..

    //printf("Insiiide, hehehe!\n");

    // if backendIndex...
    // off_t chosenOffset = 0;      already inside to_swap at this point
    /*if(!list_empty(&procDescList[victimNode->proc].swapList))           // in usmChooseSwapIndex later
        chosenOffset = list_entry(&procDescList[victimNode->proc].swapList.prev, struct to_swap, iulist)->offset+SYS_PAGE_SIZE;    // some iulisttwo later..
    usmAddProcSwapNode(victimNode);         // add now in global swap list */


    struct usm_swap_dev *selectedDevice;

    // Sélection du périphérique de swap en fonction de l'espace de swap et du numéro du périphérique

    if (victimNode->swapDevice->number == 1)
    {
        selectedDevice = pick_swap_device(victimNode->proc);
        printf("chemin du fichier 1 est : %p\n", (void*)selectedDevice->backend);
    } else if (victimNode->swapDevice->number == 2) {
        selectedDevice = pick_swap_device_two(victimNode->proc);
        printf("valeur de number : %d\n", selectedDevice->number);
        printf("chemin du fichier 2 est : %p\n", (void*)selectedDevice->backend);
    }

    #ifdef DEBUG
        printf("Valeur de l'espace swap disponible sur le swap out est : %d\n", victimNode->swapDevice->number);
    #endif

    if (selectedDevice == NULL) {
        // Périphérique de swap non trouvé
        #ifdef DEBUG
            printf("Périphérique de swap non trouvé pour l'espace de swap : %s\n", victimNode->swapDevice->espace_swap);
        #endif
        return 1;
    }


    pthread_mutex_lock(&policiesSet1Swaplock);      // replace with procLock..
    list_add(&victimNode->iulist, &procDescList[victimNode->proc].swapCache);
    pthread_mutex_unlock(&policiesSet1Swaplock);

    /*printf("Base: %lu + %lu = %lu\n",victimNode->snode->offset,((unsigned long)victimNode->swapDevice->number<<(sizeof(unsigned long)*8-3)), victimNode->snode->offset+((unsigned long)victimNode->swapDevice->number<<(sizeof(unsigned long)*8-3)));
    
    // ~victimNode->snode->offset << 9+SWAP_NR >> 3 | ((unsigned long)victimNode->swapDevice->number<<(sizeof(unsigned long)*8-SWAP_NR));

    printf("B1: %lu\n", ~victimNode->snode->offset);
    printf("B2: %lu\n", (unsigned long) victimNode->swapDevice->number<<(sizeof(unsigned long)*8-SWAP_NR));
    printf("B3: %lu\n", ~victimNode->snode->offset << 9+SWAP_NR >> 3);

    */
    printf("\n\tswap proc: %d ^^'\n\n", victimNode->proc);
    printf("\n\tswap adresse virtuelle de la page swap out: %lu ^^'\n\n", victimNode->swapped_address);
    if(usm_clear_and_set(victimNode->proc,victimNode->swapped_address,swap_value(victimNode),0))      // some clearance first..? Of 3 bits...
        return 1;
    printf("Seeking...\n");

    if(fseeko((FILE *)selectedDevice->backend, victimNode->snode->offset, SEEK_SET)){            // work this out -> no more backfile per proc., he can just put things differently from the same proc.'s address space.. now that's interesting!   | the backend giving's probably not that stylish... -_-'
    #ifdef DEBUG
            perror("[swapDevPols/sp_out/Mayday] Fseeko failed\n");
            getchar();
    #endif
            usm_set_pfn(victimNode->proc,victimNode->swapped_address,victimNode->page->physicalAddress,1 ,0);     // drop_swp_out/*_and_free*/...
            restore_used_page(&((struct optEludeList *)(victimNode->page->usedListPositionPointer))->iulist);
            putSwapNode(selectedDevice,&victimNode->snode->iulist);
            free(victimNode);
            return 1;
        }
        //printf("Writing..\n");
        if (unlikely(fwrite((void*)(victimNode->page->data),SYS_PAGE_SIZE/*ToBeComplxfd*/,1,(FILE *)selectedDevice->backend)!=1)) {
    #ifdef DEBUG
                    perror("[swapDevPols/sp_out/Mayday] Swap out writing failed!\n");
                    getchar();
    #endif //swap_dev
            usm_set_pfn(victimNode->proc,victimNode->swapped_address,victimNode->page->physicalAddress, 1,0);
            restore_used_page(&((struct optEludeList *)(victimNode->page->usedListPositionPointer))->iulist);
            putSwapNode(selectedDevice,&victimNode->snode->iulist);
            free(victimNode);
            return 1;
        }


    printf("Deleting.. from sc..\n");
    // victimNode->offset=chosenOffset;     filled now before getting inside this func.
    __list_del(victimNode->iulist.prev,victimNode->iulist.next);        // out from swap_cache.. we could differ this..
    if (!victimNode->retaken) {     // add some lock on this..
        usmAddProcSwapNode(victimNode);
        pthread_mutex_lock(&policiesSet1Swaplock);
        list_add(&victimNode->globlist,&swapList);
        swappedMemory+=SYS_PAGE_SIZE;

        
        printf("Deleting..  sc.. : %ld\n", swappedMemory);
        
        pthread_mutex_unlock(&policiesSet1Swaplock);

         printf("Deleting..  sc..\n");
    }
    else   {
        putSwapNode(selectedDevice,&victimNode->snode->iulist);
        free(victimNode);
        return 0;           // retaken error to set to know on the other side
    }
    //printf("Giving back the page..\n");
    put_pages(&((struct optEludeList *)(victimNode->page->usedListPositionPointer))->iulist);
    printf("All good bruh..\n");
    //getchar();
    return 0;
}


int swap_time(struct to_swap_time * victimNode) {        // promotion / demotion to be done "in line".. i.e. the backfile'd be replaced while copying..all.. the... content.... o_o' TODO further diggin'..

    struct usm_swap_dev *selectedDeviceOut = NULL;

    if (victimNode->to_swap->swapDevice->number == 1)
    {   
            selectedDeviceOut = pick_swap_device(victimNode->to_swap->proc);
            printf("chemin du fichier 1 est : %p\n", (void*)selectedDeviceOut->backend);
            printf("\n\tswap proc: %d ^^'\n\n", victimNode->to_swap->proc);
            printf("\n\tswap adresse virtuelle de la page swap out: %lu ^^'\n\n", victimNode->to_swap->swapped_address);
            
            printf("Seeking...\n");

            if(fseeko((FILE *)selectedDeviceOut->backend, victimNode->to_swap->snode->offset, SEEK_SET)){            // work this out -> no more backfile per proc., he can just put things differently from the same proc.'s address space.. now that's interesting!   | the backend giving's probably not that stylish... -_-'
            #ifdef DEBUG
                    perror("[swapDevPols/sp_out/Mayday] Fseeko failed\n");
                    getchar();
            #endif
                    usm_set_pfn(victimNode->to_swap->proc,victimNode->to_swap->swapped_address,victimNode->to_swap->page->physicalAddress,1 ,0);     // drop_swp_out/*_and_free*/...
                    restore_used_page(&((struct optEludeList *)(victimNode->to_swap->page->usedListPositionPointer))->iulist);
                    putSwapNode(selectedDeviceOut,&victimNode->to_swap->snode->iulist);
                    free(victimNode);
                    return 1;
                }
                printf("Writing..\n");
                if (unlikely(fwrite((void*)(victimNode->to_swap->page->data),SYS_PAGE_SIZE/*ToBeComplxfd*/,1,(FILE *)selectedDeviceOut->backend)!=1)) {
            #ifdef DEBUG
                            perror("[swapDevPols/sp_out/Mayday] Swap out writing failed!\n");
                            getchar();
            #endif //swap_dev
                    usm_set_pfn(victimNode->to_swap->proc,victimNode->to_swap->swapped_address,victimNode->to_swap->page->physicalAddress, 1,0);
                    restore_used_page(&((struct optEludeList *)(victimNode->to_swap->page->usedListPositionPointer))->iulist);
                    putSwapNode(selectedDeviceOut,&victimNode->to_swap->snode->iulist);
                    free(victimNode);
                    return 1;
                }

                return 0;
    }else{

    selectedDeviceOut = pick_swap_device_two(victimNode->to_swap->proc);
    printf("valeur de number : %d\n", selectedDeviceOut->number);
    printf("chemin du fichier 2 est : %p\n", (void*)selectedDeviceOut->backend);


    pthread_mutex_lock(&policiesSet1Swaplock);      // replace with procLock..
    list_add(&victimNode->to_swap->iulist, &procDescList[victimNode->to_swap->proc].swapCache);
    pthread_mutex_unlock(&policiesSet1Swaplock);


    printf("\n\tswap proc: %d ^^'\n\n", victimNode->to_swap->proc);
    printf("\n\tswap adresse virtuelle de la page swap out: %lu ^^'\n\n", victimNode->to_swap->swapped_address);
    if(usm_clear_and_set(victimNode->to_swap->proc,victimNode->to_swap->swapped_address,swap_value(victimNode->to_swap),0))      // some clearance first..? Of 3 bits...
        return 1;
    printf("Seeking...\n");


    if(fseeko((FILE *)selectedDeviceOut->backend, victimNode->to_swap->snode->offset, SEEK_SET)){     // work this out -> no more backfile per proc., he can just put things differently from the same proc.'s address space.. now that's interesting!   | the backend giving's probably not that stylish... -_-'
    #ifdef DEBUG
            perror("[swapDevPols/sp_out/Mayday] Fseeko failed\n");
            getchar();
    #endif
  
            usm_set_pfn(victimNode->to_swap->proc,victimNode->to_swap->swapped_address,victimNode->to_swap->page->physicalAddress,1 ,0);     // drop_swp_out/*_and_free*/...
            restore_used_page(&((struct optEludeList *)(victimNode->to_swap->page->usedListPositionPointer))->iulist);
            putSwapNode(selectedDeviceOut,&victimNode->to_swap->snode->iulist);
            free(victimNode);
            return 1;
        }
        
        printf("Writing..\n");
        if (unlikely(fwrite((void*)(victimNode->to_swap->page->data),SYS_PAGE_SIZE/*ToBeComplxfd*/,1,(FILE *)selectedDeviceOut->backend)!=1)) {
    #ifdef DEBUG
                    perror("[swapDevPols/sp_out/Mayday] Swap out writing failed!\n");
                    getchar();
    #endif //swap_dev
            usm_set_pfn(victimNode->to_swap->proc,victimNode->to_swap->swapped_address,victimNode->to_swap->page->physicalAddress, 1,0);
            restore_used_page(&((struct optEludeList *)(victimNode->to_swap->page->usedListPositionPointer))->iulist);
            putSwapNode(selectedDeviceOut,&victimNode->to_swap->snode->iulist);
            free(victimNode);
            return 1;
        }    

    //printf("Deleting.. from sc..\n");
    // victimNode->offset=chosenOffset;     filled now before getting inside this func.
    __list_del(victimNode->to_swap->iulist.prev,victimNode->to_swap->iulist.next);        // out from swap_cache.. we could differ this..
    if (!victimNode->to_swap->retaken) {     // add some lock on this..
        usmAddProcSwapNode(victimNode->to_swap);
        pthread_mutex_lock(&policiesSet1Swaplock);
        list_add(&victimNode->to_swap->globlist,&swapList);
        list_add(&victimNode->glolist,&swapListTime);
        swappedMemory+=SYS_PAGE_SIZE;
        pthread_mutex_unlock(&policiesSet1Swaplock);
    }
    else   {
        putSwapNode(selectedDeviceOut,&victimNode->to_swap->snode->iulist);
        free(victimNode);
        return 0;           // retaken error to set to know on the other side
    }
    //printf("Giving back the page..\n");
    put_pages(&((struct optEludeList *)(victimNode->to_swap->page->usedListPositionPointer))->iulist);

    //printf("All good bruh..\n");
    //getchar();


    return 0;
}

}

/*usmPolicyDefinedAlloc should provide a way of not applying thingies.. and let the swap module do it.... in like giving the "ahead" pages.. or would it be simpler to just "duplicate" it here or provide another trace of purely simultaneous swap ins?
    we could simply just take from freeList either way... should be simpler, and temporarily what's done.

    So the check_in thingie should verify free mem.. hmm... ye.... some further crazy cases man...
    ..and man, give the locked PTE in msg... should save us some huge troubles.... or enough info. to do so, without going around searching for a freaking addr..
        but for the freaking ahead swap ins... we'll need that.. holy molly... just do something generic at first.


    ..important : new ioctl for  differred waking..
    .. plus mark the shared pages SHARED... later on separate them clearly or just check on them page lists... #polDev
*/

int /*generic_*/swap_in(struct to_swap * luckyNode) {        // ..activeProcList | the things about page caching and all.. could create some different thingies than memcpy.... guess we'll be using SPDK anyway.. though o_o' and/or hacking* swp_entry_t
    // take from procDesc in_list       for check_in.. do_cond_swap...
    //printf("Swappin' iiiin! \n");

    struct usm_swap_dev *selectedDeviceIn = NULL;

    // Sélection du périphérique de swap en fonction de l'espace de swap et du numéro du périphérique
    if (luckyNode->swapDevice->number == 1) {
        selectedDeviceIn = pick_swap_device(luckyNode->proc);
        printf("valeur de number : %d\n", selectedDeviceIn->number);
    } else if (luckyNode->swapDevice->number == 2) {
        selectedDeviceIn = pick_swap_device_two(luckyNode->proc);
        printf("valeur de number : %d\n", selectedDeviceIn->number);
    }
    else
    {
        printf("Fin programm\n");
        return 0;
    }
    

    //printf("Trimmed adresse swapper IN  : %ld!\n", luckyNode->swapped_address);

    #ifdef DEBUG
        printf("Valeur de l'espace swap disponible sur le swap out est : %d\n", luckyNode->swapDevice->number);
    #endif

    printf("chemin du fichier est : %p\n", (void*)selectedDeviceIn->backend);


    printf("\n\tswap adresse virtuelle de la page swap in: %lu ^^'\n\n", luckyNode->swapped_address);

    if (fseeko((FILE *)selectedDeviceIn->backend, luckyNode->snode->offset, SEEK_SET)) {
#ifdef DEBUG
        perror("[swapDevPols/sp_in/Mayday] Fseeko failed\n");
        getchar();
#endif
        // yeah.. put back in here, makes more sense.. and better/best avoids different threads trying to swap the same entries.. let's delete from start. | yup, TODO.. analogy to swap_out's double thingy.. 
        return 1;
    }
    LIST_HEAD(pageToSwapInsideOf);
    if (!get_pages(&pageToSwapInsideOf,1))
        return 1;
    struct optEludeList * chosenPage = list_first_entry(&pageToSwapInsideOf,struct optEludeList, iulist);
    if (unlikely(fread((void*)chosenPage->usmPage->data,SYS_PAGE_SIZE,1,(FILE *)selectedDeviceIn->backend)!=1)) {
#ifdef DEBUG
                printf("[swapDevPols/sp_in/Mayday] Reading failed!\n");
                getchar();
#endif
        put_pages(&pageToSwapInsideOf);
        usmAddProcSwapNode(luckyNode);              // fails ordering.... might get ordered back later... but still, this all simply comes from an error too... 
        pthread_mutex_lock(&policiesSet1Swaplock);
        list_add(&luckyNode->globlist,&swapList);         // __
        pthread_mutex_unlock(&policiesSet1Swaplock);
        return 1;
    }
    usmSetUsedListNode(chosenPage->usmPage,chosenPage);

    printf("valeur de adrr virt   tac tac : %ld\n", luckyNode->swapped_address);

    printf("valeur de adrr phy   tac tac : %ld\n", chosenPage->usmPage->physicalAddress);

    //getchar();
   
    if (usmSubmitAllocEvent(&(struct usm_event){.origin=luckyNode->proc, .vaddr=luckyNode->swapped_address, .paddr = chosenPage->usmPage->physicalAddress})){ 
#ifdef DEBUG
        printf("[devPolicies/swp/Mayday] Unapplied allocation\n");
        getchar();
#endif
        put_pages(&pageToSwapInsideOf);
        usmAddProcSwapNode(luckyNode);              // fails ordering.... might get ordered back later... but still, this all simply comes from an error too... 
        pthread_mutex_lock(&policiesSet1Swaplock);
        list_add(&luckyNode->globlist,&swapList);         // __
        pthread_mutex_unlock(&policiesSet1Swaplock);
        return 1;
    }
    /* usmLinkPage.. */ /* put this all in helpers that make total sense.. */
    chosenPage->usmPage->virtualAddress=luckyNode->swapped_address;
    chosenPage->usmPage->process=luckyNode->proc;
    increaseProcessAllocatedMemory(luckyNode->proc, globalPageSize);
    pthread_mutex_lock(&procDescList[luckyNode->proc].alcLock);       // per node lock man!
    list_add(&chosenPage->proclist,&procDescList[luckyNode->proc].usedList);                // HUGE TODO hey, this is outrageous... do what you said and add to_swap in event, then a helper to take care of in proc. struct. list, and use the latter after basic_alloc.. i.e. in event.c and freaking take care of it there.
    chosenPage->usmPage->processUsedListPointer=&chosenPage->proclist;
    pthread_mutex_unlock(&procDescList[luckyNode->proc].alcLock);

    pthread_mutex_lock(&policiesSet1Swaplock);
    printf("valeur de swapperMemory  tac tac : %ld\n", swappedMemory);

    printf("valeur de usedList  tac tac : %ld\n", list_count_nodes(&usedList));
    swappedMemory-=SYS_PAGE_SIZE;
    printf("valeur de swapperMemory  toc toc : %ld\n", swappedMemory);
    pthread_mutex_unlock(&policiesSet1Swaplock);

    restore_used_page(&chosenPage->iulist);     //  name to be changed..

    // page updating..
    luckyNode->page->process=luckyNode->proc;                       // chosenPage.. and get rid of "page"..
    luckyNode->page->virtualAddress=luckyNode->swapped_address;     // hmm..
    // luckyNode->page->usedListPositionPointer=..   // shouldn't ever change actually
    putSwapNode(selectedDeviceIn,&luckyNode->snode->iulist);
    free(luckyNode);
    getchar();
    printf("Swappin' in done!\n");
   // getchar();
    return 0;
}

void *swapper_routine_out_dummy_periodic(void* swp_args) {
    // struct usm_worker* wk=(struct usm_worker*)swp_args;
    // struct usm_event * event;
    while(1) {
        sleep(5);
        if(!list_empty(&usedList)) {
#ifdef SpDEBUG
            printf("[devPol.] Preemptive out swapping!\n");
#endif
            struct to_swap *toSwap = (struct to_swap *) malloc (sizeof(toSwap));
            // pthread_mutex_lock(&policiesSet1lock);       no real need.. o_o'                         | rah, could get freed while we're doing our thingies... some check, and new slot "freed"..? No, some action from freePages..
            struct optEludeList * usedNode = list_entry(usedList.prev, struct optEludeList, iulist);

            toSwap->swapped_address=usedNode->usmPage->virtualAddress;
            toSwap->retaken=0;
            toSwap->page=usedNode->usmPage;
            toSwap->proc=usedNode->usmPage->process;        /* do this all in some better place.. */
            toSwap->swapDevice=choose_swap_device(toSwap->proc);
            toSwap->snode=list_entry(getSwapNode(toSwap->swapDevice), struct swap_node, iulist);
            toSwap->snode->toSwapHolder=(void *) toSwap;

            if(unlikely(toSwap->swapDevice->swap_dev_ops.swap_out(toSwap))) {         // #FIFO
#ifdef SpDEBUG
                printf("[devPol.] Failed preemptive out swapping!\n");
#endif
                ;
            }
        }
    }
}

void *swapper_routine_in_dummy_periodic(void* swp_args) {
    // struct usm_worker* wk=(struct usm_worker*)swp_args;
    // struct usm_event * event;
    while(1) {
        sleep(5);
        if(!list_empty(&swapList))
            continue;
#ifdef SpDEBUG
            printf("[devPol.] Preemptive in swapping!\n");
#endif
            struct to_swap *toSwap;
            pthread_mutex_lock(&policiesSet1Swaplock);
            toSwap = list_entry(&swapList, struct to_swap, globlist);
            list_del(&toSwap->iulist);         // __
            list_del(&toSwap->globlist);       // __
            pthread_mutex_unlock(&policiesSet1Swaplock);  
            if(unlikely(toSwap->swapDevice->swap_dev_ops.swap_in(toSwap))) {
#ifdef SpDEBUG
                printf("[devPol.] Failed reemptive in swapping!\n");
#endif 
        }
    }
}

/*void *swapper_routine_threshold(void* swp_args) {
    // struct usm_worker* wk=(struct usm_worker*)swp_args;
    // struct usm_event * event;
    while(1) {

        //pthread_mutex_lock();
        //while (usedMemory>poolSize/2);
    }
}*/

void *swapper_routine_out_commit(void* swp_args) {              // coming from usedList... | check/choosePagesToEvict
    struct list_head * toSwapList=(struct list_head*)swp_args, * swapListIterator, *tempSwpItr;
    list_for_each_safe(swapListIterator,tempSwpItr,toSwapList) {        // ver. not needed now.. although.. list_add destroying thingies.. (wanted)
        struct to_swap * swapsIterator=list_entry(swapListIterator, struct to_swap, iulist);
        if(strcmp(swapsIterator->swapDevice->espace_swap, "tmpfs") == 0)
            swapsIterator->swapDevice->number = 2;
        else
            swapsIterator->swapDevice->number = 1;
        if (likely(!swapsIterator->swapDevice->swap_dev_ops.swap_out(swapsIterator)))        // swap_out needn't a return value..
            /*list_add(&swapsIterator->iulist,&procDescList[swapsIterator->proc].swapList)*/;
        else {                               // add some list_empty check and try again...
#ifdef DEBUG
            printf("[swapDevPols/sroutine_out/Mayday] Swapping failed!\n");
            getchar();
#endif
            ; //free(swapsIterator);    | as said, already done inside swap_out.. which shouldn't need a return value
        }
    }
}

void *swapper_routine_in_commit(void* swp_args) {       // put in usm while passing the locks...
    struct list_head * toSwapList=(struct list_head*)swp_args;
    struct list_head *swapListIterator, *tempSwpItr;
    
    if (!list_empty(toSwapList))
    {

    list_for_each_safe(swapListIterator,tempSwpItr,toSwapList) {
            struct to_swap * swapsIterator=list_entry(swapListIterator, struct to_swap, iulist);
            list_del(swapListIterator);
            // list_del(&swapsIterator->globlist);      should already be boomed out
            if (unlikely(swapsIterator->swapDevice->swap_dev_ops.swap_in(swapsIterator))){  // add some list_empty check and try again maybe..            
    #ifdef DEBUG
                printf("[swapDevPols/sroutine_in/Mayday] Swapping failed!\n");
                getchar();
    #endif
                ;
            }
       }
}

}

void *swap_out_list_and_commit(void * arg) {
    int nr = *(int *)arg;
    LIST_HEAD(toSwapOutList);
    while(nr>0){
        if(list_empty(&usedList))
            break;
        struct to_swap *toSwapNode= (struct to_swap *)malloc(sizeof(struct to_swap));
        struct optEludeList * victimPageNode = list_entry(&usedList,struct optEludeList,iulist);    // alloc. module should later provide a flexible way to pick victim nodes...
        hold_used_page(&victimPageNode->iulist);

        pthread_mutex_lock(&procDescList[victimPageNode->usmPage->process].alcLock);            // toMove
        list_del(&victimPageNode->proclist);
        pthread_mutex_unlock(&procDescList[victimPageNode->usmPage->process].alcLock);

        toSwapNode->page=victimPageNode->usmPage;
        toSwapNode->proc=victimPageNode->usmPage->process;      // TODO get rid of this..
        toSwapNode->swapped_address=victimPageNode->usmPage->virtualAddress;    // TODO get rid of this... -_-
        toSwapNode->swapDevice=choose_swap_device(toSwapNode->proc);
        toSwapNode->snode=list_entry(getSwapNode(toSwapNode->swapDevice), struct swap_node, iulist);
        toSwapNode->snode->toSwapHolder=toSwapNode;
        list_add(&toSwapNode->iulist,&toSwapOutList);
        nr--;
    }
    swapper_routine_out_commit(&toSwapOutList);         // no thread creation needed here..
}

void *swap_out_list_and_commit_proc(void * arg) {
    struct proc_swp_create_list_arg nr = *(struct proc_swp_create_list_arg *)arg;
    LIST_HEAD(toSwapOutList);
    while(nr.nr>0){
        if(list_empty(&usedList))
            break;
        struct to_swap *toSwapNode= (struct to_swap *)malloc(sizeof(struct to_swap));
        struct optEludeList * victimPageNode = list_entry(nr.procSwpLst,struct optEludeList,iulist);
        hold_used_page(&victimPageNode->iulist);

        pthread_mutex_lock(&procDescList[victimPageNode->usmPage->process].alcLock);            // toMove
        list_del(&victimPageNode->proclist);
        pthread_mutex_unlock(&procDescList[victimPageNode->usmPage->process].alcLock);

        toSwapNode->page=victimPageNode->usmPage;
        toSwapNode->proc=victimPageNode->usmPage->process;      // TODO get rid of this..
        toSwapNode->swapped_address=victimPageNode->usmPage->virtualAddress;    // TODO get rid of this... -_-
        toSwapNode->swapDevice=choose_swap_device(toSwapNode->proc);
        toSwapNode->snode=list_entry(getSwapNode(toSwapNode->swapDevice), struct swap_node, iulist);
        toSwapNode->snode->toSwapHolder=toSwapNode;
        list_add(&toSwapNode->iulist,&toSwapOutList);
        nr.nr--;
    }
    swapper_routine_out_commit(&toSwapOutList);         // no thread creation needed here..
}

void *swap_in_list_and_commit(void * arg) { ///
    int nr = *(int *)arg;
    LIST_HEAD(toSwapInList);
    printf(" nombre swapList ici : %ld\n", list_count_nodes(&swapList));    
    while(nr>0){
        if(list_empty(&swapList))
            break;
        struct to_swap *luckyNode = list_entry(swapList.next,struct to_swap,globlist);
        usmHoldProcSwapNode(luckyNode);
        pthread_mutex_lock(&policiesSet1Swaplock);
        list_del(&luckyNode->globlist);
        pthread_mutex_unlock(&policiesSet1Swaplock);
        list_add(&luckyNode->iulist,&toSwapInList);
        printf(" nombre number ici : %d\n",luckyNode->swapDevice->number);
        printf(" nombre toswapInList ici : %ld\n", list_count_nodes(&toSwapInList));    
        nr--; 
    }

    printf("je swaplist memory : %ld\n", swappedMemory);
    printf(" nombre toswapInList fin : %ld\n", list_count_nodes(&toSwapInList));    
    swapper_routine_in_commit(&toSwapInList);         // no thread creation needed here..
}







/* void *find_and_delete_swap_entry(void * arg) {                   hopefully no need anymore, with proposed method...
    //struct usm_event * event = (struct usm_event*)arg;
    unsigned long offset = ((struct usm_event *)arg)->offst;
    int proc = ((struct usm_event *)arg)->origin;
    struct to_swap *swapListIterator;
    list_for_each_entry(swapListIterator,&procDescList[proc].swapList,iulist) {
        if(swapListIterator->offset==offset) {
            pthread_mutex_lock(&policiesSet1Swaplock);
            list_del(&swapListIterator->iulist);
            list_del(&swapListIterator->globlist);
            pthread_mutex_unlock(&policiesSet1Swaplock);
            break;
        }
    }
}*/

void *delete_swap_entry(void * arg) {
    struct to_swap * toDelete = (struct to_swap *)arg;
    pthread_mutex_lock(&policiesSet1Swaplock);
    list_del(&toDelete->iulist);
    list_del(&toDelete->globlist);
    pthread_mutex_unlock(&policiesSet1Swaplock);
}

/*
    Could be named basic_swap_in... but for USM's internals, basic_swap_out doesn't make sense.. so no real normal need
*/
int basic_swap(struct usm_event *event) {               // we could already put one per swapping backend... they'll just be mapped around with kinda pointers ready to be used      |       DAX though we could need some little USM pool for page caching on slow memory, and put back thingies if accessed right after... #hotCopyBack
    /* Take from file at pos. &allCo., put on chosenPages' data, call ioctl.. mostly on lock. */
    // we get the offset directly.. so this should be specialized compared to swap_in.. #..generic
    /*pthread_t culler;
    pthread_create(&culler, NULL, find_and_delete_swap_entry, event);   ref. up*/
    
    // printf("Swapping in! | swap_value: %lu ^^'\n", event->offst);
    // struct to_swap *neededNode = event_to_swap_entry(event);
   
    // if(!neededNode) {
    //     printf("Come on man..\n");
    //     getchar();
    // }

    // //  printf("Swapping in! | swap_value: %lu ^^'\n", event->offst);
    // // /*pthread_t culler;
    // // pthread_create(&culler, NULL, delete_swap_entry, neededNode);*/   // rid this off (in swap_in..)... TODO
    // if(unlikely(neededNode->swapDevice->swap_dev_ops.swap_in(neededNode))) {        // return this directly... if not for maybe what's inside..
    //     /*pthread_join(culler, NULL);
    //     list_add(event->);  add back.. culler'll put the found node in some part of event... */
    //     return 1;
    // }
    //  printf("Swapping in! | swap_value: %lu ^^'\n", event->offst);
    // return 0;

    if(usedMemory<poolSize/2.5){    // swappedMemory against some threshold maybe..
        pthread_t worker;
        int nr = (swappedMemory / SYS_PAGE_SIZE);
        printf("valeur de nr : %d\n", nr);
        if (nr>0){
            pthread_create(&worker, NULL, swap_in_list_and_commit, &nr);
        }
    } 
 }

int double_swap(struct usm_event *event) {
    /* basic_swap but doubled basing ourselves on whether we found some thingies... or procDesc->swapped%SYS_PAGE_SIZE >= 2 */
    // specialized (basic_swap) plus the other defined onies...
    if (unlikely(!basic_swap(event)))
        return 0;
    //pthread_mutex_lock(&policiesSet1Swaplock);        // the thread culler idea doesn't seem like a good one now..
    if(list_empty(&procDescList[event->origin].swapList)) {
        //pthread_mutex_unlock(&policiesSet1Swaplock);
        return 1;
    }
    struct to_swap * luckyNode = list_entry(&procDescList[event->origin].swapList, struct to_swap, iulist);
    pthread_t culler;
    pthread_create(&culler, NULL, delete_swap_entry, luckyNode);        // should be done inside swap_in..
    if(!luckyNode->swapDevice->swap_dev_ops.swap_in(luckyNode)) {      // not a failing condition..
#ifdef DEBUG        // SpDEBUG
        printf("[devPol.] Second swapping failed.\n");
#endif
    }
    return 1;
}

void cond_swap_out(struct usm_event *event){
    if(procDescList[event->origin].allocated>9*SYS_PAGE_SIZE) {     // again, #define..
        printf("swap out\n");
        struct proc_swp_create_list_arg arg = {.nr=3,.procSwpLst=&procDescList[event->origin].usedList};
        pthread_t worker;
        pthread_create(&worker, NULL, swap_out_list_and_commit_proc, &arg);
    }
}

int basic_free(struct usm_event *event) {
    /* Freeing of linked structures... we get the V.A through event, and act accordingly to get our node here IN here and get rid of it, alongside on other mem. content (e.g disk, nvme, or even.. some pages used as caches.. in USMv2 probably -_-) content */;
    // when a process ciaociaoes? Wut the...
    // the thing is that we don't.. hmm.. we could create a swapped page table with the same pfns.. but they'd be quite.. hmm... by offset! ralloc a basic table, then access it by index! Already much better.
}


void check_swap_in(struct usm_event * event) {          // this can take some node, be swapping it, while we receive a fault on said node.. which culler won't find, and usm_swap will try to swap in in duplicacy..

    if(usedMemory<poolSize/2.5){    // swappedMemory against some threshold maybe..
        pthread_t worker;
    
        int nr = (swappedMemory/ SYS_PAGE_SIZE);
        printf("valeur de nr : %d\n", nr);
        if (nr>0){

            pthread_create(&worker, NULL, swap_in_list_and_commit, &nr);
        }
    } 
          /* code */
          
    /*dude already swapped it in for us.. ? X'''''''''''D */
    
    if(event->type==FREE) {

        printf("Check_swap_in now! | %d\n", event->origin);
        pthread_mutex_lock(&procDescList[event->origin].swpLock);
        if (list_empty(&procDescList[event->origin].swapList)) {
            printf("Proc. used list empty man!\n");
            pthread_mutex_unlock(&procDescList[event->origin].swpLock);
            return;
        }
        else
            printf("Not empty!\n");
        pthread_mutex_unlock(&procDescList[event->origin].swpLock);
        struct to_swap *toSwapNode= (struct to_swap *)malloc(sizeof(struct to_swap));
        struct optEludeList * victimPageNode = list_first_entry(&procDescList[event->origin].usedList,struct optEludeList,proclist);
        hold_used_page(&victimPageNode->iulist);

        //pthread_mutex_lock(&procDescList[event->origin].alcLock);            // toMove
        //list_del(&victimPageNode->proclist);
        //pthread_mutex_unlock(&procDescList[event->origin].alcLock);
        toSwapNode->page=victimPageNode->usmPage;
        toSwapNode->proc=victimPageNode->usmPage->process;
        toSwapNode->swapped_address=victimPageNode->usmPage->virtualAddress;
        toSwapNode->swapDevice=choose_swap_device(toSwapNode->proc);
        toSwapNode->snode=list_entry(getSwapNode(toSwapNode->swapDevice), struct swap_node, iulist);        // getSwapNode can return NULL.. get the val. and check it....
        toSwapNode->snode->toSwapHolder=(void *)toSwapNode;
        if(toSwapNode->swapDevice->swap_dev_ops.swap_in(toSwapNode))
#ifdef DEBUG
            printf("\tHehehhe... failed.\n");
        else
            printf("Swapped in used page!\n");
#else
            ;
#endif
            // free.. and put back....
    }
    getchar();

    printf("Trimmed swappedMemory swap in : %ld!\n", swappedMemory);
    
}


struct usm_swap_policy_ops usm_basic_swap_policy= {.usm_swap=basic_swap,.usm_free=basic_free};
struct usm_swap_policy_ops swap_policy_one= {.usm_swap=double_swap, .usm_free=basic_free, /*.cond_swap_out=cond_swap_out*/};

// evict_handling function... some ideas of parameters : desired size... needed size, optimal size....spatial distrib. somehow...., it'd return some donezo value.. or simply some void thingy, which'll imply some next allocation trial failure (although some freeing might have happened at the same time...)       There should really be some digging to further do...
void handle_evict_request(struct usm_event *event) {        // #fallback    #basic_swap_out
    printf("[devPol/Mayday] Calling emergency evict. func.!\n");
        // simply *modifying* some used node... though, we're not sure if it is getting taken for freeing by someone or not.. hence the locking... but :
        //          if it is getting freed, as we'd just have changed the linked pages' usedListPosition (not perfectly relevant, but still rambling on a bit..), it'd not have any other impact than the freeing function saying "Hey man, ain't found it"... but we wouldn't want that too... so.....
        //          so, though, uhhh.. the clearance of the PTE resolves that... : we should never get that problem, so long as the locking of the PTE's done in time.... hence some "IN-USE" or similar return value needed in the ioctl...
        //          they just freaking return EINVAL... oh man! See, look, freak it, we'll SEE!
    /* Event could be used to further evict more pages... ; evicting multiple pages basically is the dev.'s decision, though... as this function should be extremely scarcely used, in normal conditions */
    /* We don't return the node, we just let him try again */
    // swap_out(1 /* this, gotta get it.. should be generated! */,pagesList /* just* rand. atm */); lol
    struct to_swap *toSwapNode= (struct to_swap *)malloc(sizeof(struct to_swap));
    struct optEludeList * victimPageNode = list_first_entry(&usedList,struct optEludeList,iulist);
    hold_used_page(&victimPageNode->iulist);

    pthread_mutex_lock(&procDescList[event->origin].alcLock);            // toMove
    list_del(&victimPageNode->proclist);
    pthread_mutex_unlock(&procDescList[event->origin].alcLock);

    toSwapNode->page=victimPageNode->usmPage;
    toSwapNode->proc=victimPageNode->usmPage->process;
    toSwapNode->swapped_address=victimPageNode->usmPage->virtualAddress;
    toSwapNode->swapDevice=choose_swap_device(toSwapNode->proc);
    toSwapNode->snode=list_entry(getSwapNode(toSwapNode->swapDevice), struct swap_node, iulist);
    toSwapNode->snode->toSwapHolder=toSwapNode;
    toSwapNode->swapDevice->swap_dev_ops.swap_out(toSwapNode);       // #FIFO
}




void check_swap_in_dummyLT(struct usm_event * event){

    printf("rien ce gar louis\n");
        
    if(usedMemory<poolSize/2.5) {    // swappedMemory against some threshold maybe..
        pthread_t worker;
        int nr = (swappedMemory/2)/SYS_PAGE_SIZE;
        if (nr>0)
            pthread_create(&worker, NULL, swap_in_list_and_commit, &nr);
    } /*dude already swapped it in for us.. ? X'''''''''''D */
    
    if(event->type==FREE) {
        printf("Check_swap_in now! | %d\n", event->origin);
        pthread_mutex_lock(&procDescList[event->origin].swpLock);
        if (list_empty(&procDescList[event->origin].swapList)) {
            printf("Proc. used list empty man!\n");
            pthread_mutex_unlock(&procDescList[event->origin].swpLock);
            return;
        }
        else
            printf("Not empty!\n");
        pthread_mutex_unlock(&procDescList[event->origin].swpLock);
        struct to_swap *toSwapNode= (struct to_swap *)malloc(sizeof(struct to_swap));
        struct optEludeList * victimPageNode = list_first_entry(&procDescList[event->origin].usedList,struct optEludeList,proclist);
        hold_used_page(&victimPageNode->iulist);

        //pthread_mutex_lock(&procDescList[event->origin].alcLock);            // toMove
        //list_del(&victimPageNode->proclist);
        //pthread_mutex_unlock(&procDescList[event->origin].alcLock);
        toSwapNode->page=victimPageNode->usmPage;
        toSwapNode->proc=victimPageNode->usmPage->process;
        toSwapNode->swapped_address=victimPageNode->usmPage->virtualAddress;
        toSwapNode->swapDevice=choose_swap_device(toSwapNode->proc);
        toSwapNode->snode=list_entry(getSwapNode(toSwapNode->swapDevice), struct swap_node, iulist);        // getSwapNode can return NULL.. get the val. and check it....
        toSwapNode->snode->toSwapHolder=(void *)toSwapNode;
        if(toSwapNode->swapDevice->swap_dev_ops.swap_in(toSwapNode))
#ifdef DEBUG
            printf("\tHehehhe... failed.\n");
        else
            printf("Swapped in used page!\n");
#else
            ;
#endif
            // free.. and put back....
    }
    getchar();

    
}

void check_swap_out_dummyLT(struct usm_event * event) {         // less concerning, but this can be swapping out a freed page.. | some freed slot / pagesFree further interaction digging..
    
    int n=0;
    char value;
    void *temp;
    char *espace_swap;

    printf("nombre de page usedList : %ld\n", list_count_nodes(&usedList));

    if(usedMemory>poolSize/1.2){        // poolSize/1.2  to const.
        printf("Used memory..  in the what the..: %lu\n", usedMemory);
        getchar();
        goto lol;
        pthread_t worker;
        int nr = (usedMemory/3)/SYS_PAGE_SIZE;
        if (nr>0)
            pthread_create(&worker, NULL, swap_out_list_and_commit, &nr);
    }
lol:
    if(event->type==ALLOC) {    // strcmp(event->procName,"littleTest") && ...
        sscanf(event->donne, "%c;%d;%p;%s", &value, &n, &temp,espace_swap);
        printf("In alloc event! With proc. being : %d\n", event->origin);
        pthread_mutex_lock(&procDescList[event->origin].alcLock);
        if (list_empty(&procDescList[event->origin].usedList)) {
            printf("Proc. used list empty man!\n");
            pthread_mutex_unlock(&procDescList[event->origin].alcLock);
            return ;
        }
        else
            printf("Not empty!\n");
        pthread_mutex_unlock(&procDescList[event->origin].alcLock);
            // locks..
        struct optEludeList * victimPageNode;

        pthread_mutex_lock(&procDescList[event->origin].alcLock);            // toMove
        victimPageNode = list_first_entry(&procDescList[event->origin].usedList,struct optEludeList, proclist);
        list_del_init(&victimPageNode->proclist);
        pthread_mutex_unlock(&procDescList[event->origin].alcLock);
        hold_used_page(&victimPageNode->iulist);        // pick and hold used page... this upper line inside, hence too the lock..

        struct to_swap *toSwapNode= (struct to_swap *)malloc(sizeof(struct to_swap));   
        toSwapNode->page=victimPageNode->usmPage;
        toSwapNode->proc=victimPageNode->usmPage->process;
       
        //printf("je test : %s\n", espace_swap);
        if(toSwapNode->proc<=0) {
            printf("\tHold up! %d\n", toSwapNode->proc);
            getchar();
        }
        toSwapNode->swapped_address=victimPageNode->usmPage->virtualAddress;
        toSwapNode->swapDevice=choose_swap_device(toSwapNode->proc);
        toSwapNode->snode=list_entry(getSwapNode(toSwapNode->swapDevice), struct swap_node, iulist);        // getSwapNode can return NULL.. get the val. and check it....
        toSwapNode->snode->toSwapHolder=toSwapNode;
        toSwapNode->swapDevice->espace_swap = espace_swap;
        printf("Chosen swap offset : %lu\n", toSwapNode->snode->offset);
        if(toSwapNode->swapDevice->swap_dev_ops.swap_out(toSwapNode))
#ifdef DEBUG
            printf("\tHehehhe... failed.\n");
        else
            printf("Swapped out used page!\n");
    printf("nombre de page apres swap out usedList : %ld\n", list_count_nodes(&usedList));
#else
            ;
#endif
            // free.. and put back....
    }
    getchar();

    return ;
}


void check_swap_out8(struct usm_event * event) {

    int ret = 0;
    int n=0;
    char value;
    void *temp;
    char espace_swap[30];

    if(event->type == HINTS){

        sscanf(event->donne, "%c;%d;%p;%s", &value, &n, &temp,espace_swap);
      
        if(usedMemory>8*4096) {     // ye ye #..
                
            getchar();

            while(usedMemory>5*4096) {

                printf("nombre de page usedList : %ld\n", list_count_nodes(&usedList));

                printf("usedMemory before: %ld!\n", usedMemory);
               
                pthread_mutex_lock(&procDescList[event->origin].alcLock);
                if (list_empty(&procDescList[event->origin].usedList)) {
                    printf("Proc. used list empty man!\n");
                    pthread_mutex_unlock(&procDescList[event->origin].alcLock);
                    ret++;
                    goto out_hint;
                }
                pthread_mutex_unlock(&procDescList[event->origin].alcLock);

                    // locks..
                struct optEludeList * victimPageNode;
                
                pthread_mutex_lock(&procDescList[event->origin].alcLock);            // toMove
                victimPageNode = list_first_entry(&procDescList[event->origin].usedList,struct optEludeList, proclist);
                list_del_init(&victimPageNode->proclist);
                pthread_mutex_unlock(&procDescList[event->origin].alcLock);
                hold_used_page(&victimPageNode->iulist);        // pick and hold used page... this upper line inside, hence too the lock..
               toSwapNode = (struct to_swap *)malloc(sizeof(struct to_swap));
                
                if (toSwapNode == NULL) {
                    printf("\tDude!..\n");
                    getchar();
                }

                toSwapNode->page=victimPageNode->usmPage;
                toSwapNode->proc=victimPageNode->usmPage->process;
                printf("What the actual %d..\n", toSwapNode->proc);
                if(toSwapNode->proc<=0) {
                    printf("\tHold up! %d\n", toSwapNode->proc);
                    getchar();
                }
  
                toSwapNode->swapped_address=victimPageNode->usmPage->virtualAddress;
                toSwapNode->swapDevice=choose_swap_device(toSwapNode->proc);
                struct list_head * snode = getSwapNode(toSwapNode->swapDevice);

                printf("Trimmed adresse swapper : %ld!\n", toSwapNode->swapped_address);
                if(unlikely(!snode)) {
#ifdef DEBUG
                    printf("\tSwap file full (prePreparedNodes..).\n");
#endif
                    ret++;
                    goto out_hint;
                }
                toSwapNode->snode=list_entry(snode, struct swap_node, iulist);        // getSwapNode can return NULL.. get the val. and check it....
                toSwapNode->snode->toSwapHolder=toSwapNode;
                if(toSwapNode->snode->toSwapHolder==NULL) {
                    printf("What the actual..\n");
                    getchar();
                }
                
                printf("Chosen swap offset : %lu\n", toSwapNode->snode->offset);

                if(strcmp(espace_swap, "tmp")==0)
                    {
                        toSwapNode->swapDevice->number = 1;
                    }
                else if(strcmp(espace_swap, "tmpfs")==0)
                    {
                        toSwapNode->swapDevice->number = 2;
                    }   

                if(toSwapNode->swapDevice->swap_dev_ops.swap_out(toSwapNode)) {
#ifdef DEBUG
                    printf("\tHehehhe... one swapping out failed.\n");
#endif
                    ret++;
                    break;
                }
            }
        }
        else
            return;
out_hint:
        if (ret) {
#ifdef DEBUG
            printf("Something went wrong in the trimming process!\n");
#endif
            getchar();
        }
        else{
#ifdef DEBUG 
            printf("Trimmed used memory to 7KO!\n");
            printf("Trimmed swaplist : %ld!\n", list_count_nodes(&swapList));
            printf("Trimmed swappedMemory : %ld!\n", swappedMemory);
            printf("Trimmed usedMemory : %ld!\n", usedMemory);
            printf("Trimmed poolSize : %ld!\n", poolSize);
            printf("nombre de page apres swap out usedList : %ld\n", list_count_nodes(&usedList));
#endif
            // free.. and put back....
    }
}

    if(event->type==ALLOC) {
        if(usedMemory>8*4096) {     // ye ye #..
            printf("50MB used, trimming to do! Last allocated proc. : %d\n", event->origin);
            getchar();

            while(usedMemory>=5*4096) {

                pthread_mutex_lock(&procDescList[event->origin].alcLock);
                if (list_empty(&procDescList[event->origin].usedList)) {
                    printf("Proc. used list empty man!\n");
                    pthread_mutex_unlock(&procDescList[event->origin].alcLock);
                    ret++;
                    goto out;
                }
                pthread_mutex_unlock(&procDescList[event->origin].alcLock);

                    // locks..
                struct optEludeList * victimPageNode;

                pthread_mutex_lock(&procDescList[event->origin].alcLock);            // toMove
                victimPageNode = list_first_entry(&procDescList[event->origin].usedList,struct optEludeList, proclist);
                list_del_init(&victimPageNode->proclist);
                pthread_mutex_unlock(&procDescList[event->origin].alcLock);
                hold_used_page(&victimPageNode->iulist);        // pick and hold used page... this upper line inside, hence too the lock..

                struct to_swap * toSwapNode = (struct to_swap *)malloc(sizeof(struct to_swap));
                if (toSwapNode == NULL) {
                    printf("\tDude!..\n");
                    getchar();
                }
                toSwapNode->page=victimPageNode->usmPage;
                toSwapNode->proc=victimPageNode->usmPage->process;
                if(toSwapNode->proc<=0) {
                    printf("\tHold up! %d\n", toSwapNode->proc);
                    getchar();
                }
                toSwapNode->swapped_address=victimPageNode->usmPage->virtualAddress;
                toSwapNode->swapDevice=choose_swap_device(toSwapNode->proc);
                struct list_head * snode = getSwapNode(toSwapNode->swapDevice);
                if(unlikely(!snode)) {
#ifdef DEBUG
                    printf("\tSwap file full (prePreparedNodes..).\n");
#endif
                    ret++;
                    goto out;
                }
                toSwapNode->snode=list_entry(snode, struct swap_node, iulist);        // getSwapNode can return NULL.. get the val. and check it....
                toSwapNode->snode->toSwapHolder=toSwapNode;
                if(toSwapNode->snode->toSwapHolder==NULL) {
                    printf("What the actual..\n");
                    getchar();
                }
                printf("Chosen swap offset : %lu\n", toSwapNode->snode->offset);
                if(toSwapNode->swapDevice->swap_dev_ops.swap_out(toSwapNode)) {
#ifdef DEBUG
                    printf("\tHehehhe... one swapping out failed.\n");
#endif
                    ret++;
                    break;
                }
            }
        }
        else
            return;
out:
        if (ret) {
#ifdef DEBUG
            printf("Something went wrong in the trimming process!\n");
#endif
            getchar();
        }
        else
#ifdef DEBUG
            printf("Trimmed used memory to 30MB!\n");
#endif
            // free.. and put back....
    
    getchar();
    
    }

}




void check_swap_out(struct usm_event * event)
{

int ret =0;

if(event->type==HINTS ) {     
        
        if(usedMemory>8*4096) {     // ye ye #..
            getchar();

            while(usedMemory>3*4096) {

                printf("nombre de page usedList : %ld\n", list_count_nodes(&usedList));

                printf("usedMemory before alloc: %ld!\n", usedMemory);
               
                pthread_mutex_lock(&procDescList[event->origin].alcLock);
                if (list_empty(&procDescList[event->origin].usedList)) {
                    printf("Proc. used list empty man!\n");
                    pthread_mutex_unlock(&procDescList[event->origin].alcLock);
                    ret++;
                    goto out;
                }
                pthread_mutex_unlock(&procDescList[event->origin].alcLock);

                    // locks..
                struct optEludeList * victimPageNode;
                
                pthread_mutex_lock(&procDescList[event->origin].alcLock);            // toMove
                victimPageNode = list_first_entry(&procDescList[event->origin].usedList,struct optEludeList, proclist);
                list_del_init(&victimPageNode->proclist);
                pthread_mutex_unlock(&procDescList[event->origin].alcLock);
                hold_used_page(&victimPageNode->iulist);        // pick and hold used page... this upper line inside, hence too the lock..

               struct to_swap_time * toSwapTime = (struct to_swap_time *)malloc(sizeof(struct to_swap_time));
                
                if (toSwapTime == NULL) {
                    printf("\tDude!..\n");
                    getchar();
                    return;
                }

                //  Initiation de la structure to_swap de toSwapTime
                toSwapTime->to_swap = (struct to_swap *)malloc(sizeof(struct to_swap));
            
                if (toSwapTime->to_swap == NULL)
                {
                    printf("\t Erreur d'allocation de memoire pour toSwapTime->to_swap\n");
                    free(toSwapTime);
                    return;
                }
                
                toSwapTime->to_swap->page=victimPageNode->usmPage;
                toSwapTime->to_swap->proc=victimPageNode->usmPage->process;
                printf("What the actual %d..\n", toSwapTime->to_swap->proc);
                if(toSwapTime->to_swap->proc<=0) {
                    printf("\tHold up! %d\n", toSwapTime->to_swap->proc);
                    getchar();
                }
  
                toSwapTime->to_swap->swapped_address=victimPageNode->usmPage->virtualAddress;
                toSwapTime->to_swap->swapDevice=choose_swap_device(toSwapTime->to_swap->proc);
                struct list_head * snode = getSwapNode(toSwapTime->to_swap->swapDevice);

                printf("Trimmed adresse swapper : %ld!\n", toSwapTime->to_swap->swapped_address);
                if(unlikely(!snode)) {
#ifdef DEBUG
                    printf("\tSwap file full (prePreparedNodes..).\n");
#endif
                    ret++;
                    goto out;
                }

                toSwapTime->to_swap->snode=list_entry(snode, struct swap_node, iulist);        // getSwapNode can return NULL.. get the val. and check it....
                toSwapTime->to_swap->snode->toSwapHolder=toSwapTime->to_swap;
                if(toSwapTime->to_swap->snode->toSwapHolder==NULL) {
                    printf("What the actual..\n");
                    getchar();
                }
                
                printf("Chosen swap offset : %lu\n", toSwapTime->to_swap->snode->offset);
                
                printf("temps du swap out : %ld\n", toSwapTime->swap_time_out);
             
                toSwapTime->to_swap->swapDevice->number = 2;
                toSwapTime->swap_time_out = time(NULL);
                sleep(2);

                if(toSwapTime->to_swap->swapDevice->swap_dev_ops.swap_time(toSwapTime)) {
                #ifdef DEBUG
                    printf("\tHehehhe... one swapping out failed.\n");
                #endif
                    ret++;
                    break;
                }
            }
        }
        else
            return;
out:
        if (ret) {
#ifdef DEBUG
            printf("Something went wrong in the trimming process!\n");
#endif
            getchar();
        }
        else{
#ifdef DEBUG 
            printf("Trimmed used memory to 7KO!\n");
            printf("Trimmed swaplist : %ld!\n", list_count_nodes(&swapList));
            printf("Trimmed swappedMemory : %ld!\n", swappedMemory);
            printf("Trimmed usedMemory : %ld!\n", usedMemory);
            printf("Trimmed poolSize : %ld!\n", poolSize);
            printf("nombre de page apres swap out usedList : %ld\n", list_count_nodes(&usedList));
#endif
            // free.. and put back....
    }

        pthread_t tid;
        pthread_create(&tid, NULL, check_and_move_swapped_page, NULL);
        pthread_join(tid, NULL);

   
    }

}




void check_swap_out2(struct usm_event * event) {
    int ret = 0;
    int num = 0;

    if (event->type == HINTS) {
        if (usedMemory > 8 * 4096) {
            // Vérifier si process_swap est alloué
            if (!list_empty(&processList)) {
                // Vérifier si process_swap->swapNode est alloué
                        if (event->origin == process_swap->process) {
                            printf("usedMemory bprocess_swap->process: %d!\n", process_swap->process);

                            while (usedMemory > 6 * 4096) {
                                printf("nombre de page usedList : %ld\n", list_count_nodes(&usedList));
                                printf("usedMemory before alloc: %ld!\n", usedMemory);

                                pthread_mutex_lock(&procDescList[event->origin].alcLock);
                                if (list_empty(&procDescList[event->origin].usedList)) {
                                    printf("Proc. used list empty man!\n");
                                    pthread_mutex_unlock(&procDescList[event->origin].alcLock);
                                    ret++;
                                    goto out;
                                }
                                pthread_mutex_unlock(&procDescList[event->origin].alcLock);

                                struct optEludeList * victimPageNode;

                                pthread_mutex_lock(&procDescList[event->origin].alcLock);
                                victimPageNode = list_first_entry(&procDescList[event->origin].usedList, struct optEludeList, proclist);
                                list_del_init(&victimPageNode->proclist);
                                pthread_mutex_unlock(&procDescList[event->origin].alcLock);
                                hold_used_page(&victimPageNode->iulist);
                                
                                struct to_swap * toSwapNode = (struct to_swap *)malloc(sizeof(struct to_swap));

                                toSwapNode->page = victimPageNode->usmPage;
                                toSwapNode->proc = victimPageNode->usmPage->process;

                                toSwapNode->swapped_address = victimPageNode->usmPage->virtualAddress;
                                toSwapNode->swapDevice = choose_swap_device(process_swap->swapNode->proc);
                                struct list_head * snode = getSwapNode(toSwapNode->swapDevice);

                                printf("Trimmed adresse swapper : %ld!\n", toSwapNode->swapped_address);
                                if (unlikely(!snode)) {
                                    #ifdef DEBUG
                                    printf("\tSwap file full (prePreparedNodes..).\n");
                                    #endif
                                    ret++;
                                    goto out_hint;
                                }

                                toSwapNode->snode = list_entry(snode, struct swap_node, iulist);
                                toSwapNode->snode->toSwapHolder =toSwapNode;
                                if (toSwapNode->snode->toSwapHolder == NULL) {
                                    printf("What the actual..\n");
                                    getchar();
                                }

                                printf("Chosen swap offset : %lu\n", toSwapNode->snode->offset);

                                struct usm_process_swap *swapProcess = list_first_entry(&processList, struct usm_process_swap, iulist);

                                toSwapNode->swapDevice->number = swapProcess->swapNode->swapDevice->number;

                                printf("list count process : %ld\n", list_count_nodes(&processList));
                                printf("list count process : %d\n",  swapProcess->swapNode->swapDevice->number);

                                if (toSwapNode->swapDevice->swap_dev_ops.swap_out(toSwapNode)) {
                                #ifdef DEBUG
                                        printf("\tHehehhe... one swapping out failed.\n");
                                #endif
                                    ret++;
                                    break;
                                }
                            }
                        } else {
                            return;
                        }
            
                }else {

                printf("process_swap est NULL\n");
            }
        }
    }

out_hint:
    if (ret) {
#ifdef DEBUG
        printf("Something went wrong in the trimming process!\n");
#endif
        getchar();
    } else {
#ifdef DEBUG 
        printf("Trimmed used memory to 7KO!\n");
        printf("Trimmed usedMemory : %ld!\n", usedMemory);
        printf("Trimmed poolSize : %ld!\n", poolSize);
        printf("nombre de page apres swap out usedList : %ld\n", list_count_nodes(&usedList));
        printf("Trimmed swappedMemory : %ld!\n", swappedMemory);
        printf("Trimmed swaplist : %ld!\n", list_count_nodes(&swapList));
#endif
    }


    if(event->type==ALLOC) {
        if(usedMemory>50*1024*1024) {     // ye ye #..
            printf("50MB used, trimming to do! Last allocated proc. : %d\n", event->origin);
            getchar();

            while(usedMemory>=30*1024*1024) {

                pthread_mutex_lock(&procDescList[event->origin].alcLock);
                if (list_empty(&procDescList[event->origin].usedList)) {
                    printf("Proc. used list empty man!\n");
                    pthread_mutex_unlock(&procDescList[event->origin].alcLock);
                    ret++;
                    goto out;
                }
                pthread_mutex_unlock(&procDescList[event->origin].alcLock);

                    // locks..
                struct optEludeList * victimPageNode;

                pthread_mutex_lock(&procDescList[event->origin].alcLock);            // toMove
                victimPageNode = list_first_entry(&procDescList[event->origin].usedList,struct optEludeList, proclist);
                list_del_init(&victimPageNode->proclist);
                pthread_mutex_unlock(&procDescList[event->origin].alcLock);
                hold_used_page(&victimPageNode->iulist);        // pick and hold used page... this upper line inside, hence too the lock..

                struct to_swap * toSwapNode = (struct to_swap *)malloc(sizeof(struct to_swap));
                if (toSwapNode == NULL) {
                    printf("\tDude!..\n");
                    getchar();
                }
                toSwapNode->page=victimPageNode->usmPage;
                toSwapNode->proc=victimPageNode->usmPage->process;
                if(toSwapNode->proc<=0) {
                    printf("\tHold up! %d\n", toSwapNode->proc);
                    getchar();
                }
                toSwapNode->swapped_address=victimPageNode->usmPage->virtualAddress;
                toSwapNode->swapDevice=choose_swap_device(toSwapNode->proc);
                struct list_head * snode = getSwapNode(toSwapNode->swapDevice);
                if(unlikely(!snode)) {
#ifdef DEBUG
                    printf("\tSwap file full (prePreparedNodes..).\n");
#endif
                    ret++;
                    goto out;
                }
                toSwapNode->snode=list_entry(snode, struct swap_node, iulist);        // getSwapNode can return NULL.. get the val. and check it....
                toSwapNode->snode->toSwapHolder=toSwapNode;
                if(toSwapNode->snode->toSwapHolder==NULL) {
                    printf("What the actual..\n");
                    getchar();
                }
                printf("Chosen swap offset : %lu\n", toSwapNode->snode->offset);
                if(toSwapNode->swapDevice->swap_dev_ops.swap_out(toSwapNode)) {
#ifdef DEBUG
                    printf("\tHehehhe... one swapping out failed.\n");
#endif
                    ret++;
                    break;
                }
            }
        }
        else
            return;
out:
        if (ret) {
#ifdef DEBUG
            printf("Something went wrong in the trimming process!\n");
#endif
            getchar();
        }
        else
#ifdef DEBUG
            printf("Trimmed used memory to 30MB!\n");
#endif
            // free.. and put back....
    
    getchar();
    
    }


}


struct usm_swap_dev_ops swap_device_one_ops = {.swap_in=swap_in, .swap_out=swap_out, .swap_time=swap_time};
struct usm_swap_dev swap_device_one = {.number=1};
struct usm_swap_dev swap_device_two = {.number=2};
struct usm_swap_dev swap_device_three = {.number=3};

struct usm_swap_dev *numberToSwapDev_three(int n)
{
    return &swap_device_three;
}

struct usm_swap_dev * pick_swap_device_three(int proc)
{
    return &swap_device_three;
}

struct usm_swap_dev * numberToSwapDev (int n) {  // a devices' table
    return &swap_device_one;
}

struct usm_swap_dev * pick_swap_device (int proc) {
    // if usedMemory&co
    // if procDesc[proc].name&co
    return &swap_device_one;
}


struct usm_swap_dev * numberToSwapDev_two (int n) {  // a devices' table
    return &swap_device_two;
}

struct usm_swap_dev * pick_swap_device_two (int proc) {
    // if usedMemory&co
    // if procDesc[proc].name&co
    return &swap_device_two;
}

int create_and_int_ramfs_device()
{
    struct stat st;

    choose_swap_device = &pick_swap_device_three;
    deviceNumberToSwapDevice = &numberToSwapDev_three;
    swap_device_three.swap_dev_ops = swap_device_one_ops;

    char mount_cmd_ramfs[100] = "sudo mount -t ramfs -o size=100M ramfs /mnt/";
    //char create_dir_cmd[] = "sudo mkdir /mnt/ramfs";
    int status;

    status = system(mount_cmd_ramfs);
    if (status != 0) {
        #ifdef DEBUG
            fprintf(stderr, "Erreur lors du montage de RamFS.\n");
        #endif
        return 1;
    }

    swapFileRamFsName = (char *) malloc (85);
    strcat(swapFileRamFsName, "/mnt/usmSwapFileRamFs");

    #ifdef DEBUG
        printf("Opening : %s\n", swapFileRamFsName);
    #endif

    swap_device_three.backend = fopen(swapFileRamFsName, "wb+");
    printf("chemin du fichier three est : %p\n", (void*)swap_device_three.backend);

    if (!swap_device_three.backend)
    {
        #ifdef DEBUG
            perror("Swap file opening failed!");
        #endif
        return 1;
    }

    if (init_swap_device_nodes(&swap_device_three, (unsigned long)100*1024*1024)) { 
    #ifdef DEBUG
            printf("Swap device's nodes couldn't be init'ed!\n");
    #endif
        return 1;
    }

    printf("swap device numbre : %d\n", swap_device_three.number);

    return 0;
}


int create_and_ini_device_tmpfs()
{

    // Vérifier si le répertoire existe déjà
    struct stat st;
    choose_swap_device = &pick_swap_device_two;
    deviceNumberToSwapDevice = &numberToSwapDev_two;
    swap_device_two.swap_dev_ops = swap_device_one_ops;

    /* rajout du systeme de fichier tmpfs jocelyn debut */

    char mount_cmd_tmps[100] =  "sudo mount -t tmpfs -o size=100M tmpfs /tmp/";

    int status;

    status = system(mount_cmd_tmps);
    if (status != 0) {
        #ifdef DEBUG
            fprintf(stderr, "Erreur lors du montage de tmpfs.\n");
        #endif
    }

    swapFileTmpName = (char *) malloc (100);
    strcat(swapFileTmpName, "/tmp/usmSwapFileTmpFs");

    #ifdef DEBUG
        printf("Opening : %s\n", swapFileTmpName);
    #endif

    swap_device_two.backend = fopen(swapFileTmpName, "wb+");

    printf("chemin du fichier two est : %p\n", (void*)swap_device_two.backend);


    if (!swap_device_two.backend)
    {
        #ifdef DEBUG
            perror("Swap file opening failed!");
        #endif
        return 1;
    }

    if (init_swap_device_nodes(&swap_device_two, (unsigned long)100*1024*1024)) { 
    #ifdef DEBUG
            printf("Swap device's nodes couldn't be init'ed!\n");
    #endif
        return 1;
    }
    

    printf("swap device numbre two : %d\n", swap_device_two.number);
    /* ajout jocelyn fin*/


    
    return 0;
}


int create_and_ini_device_tmp()
{
    choose_swap_device=&pick_swap_device;
    deviceNumberToSwapDevice=&numberToSwapDev;

    swap_device_one.swap_dev_ops=swap_device_one_ops;

    /*time_t timestamp = time( NULL );
    struct tm * pTime = localtime( & timestamp );
    char * swapID = (char *) malloc(80);
    strftime(swapID, 80, "%d%m%Y%H%M%S", pTime);*/
    swapFileName = (char *) malloc (85);
    strcat(swapFileName, "/tmp/usmSwapFile");
    //strcat(swapFileName,swapID);
    #ifdef DEBUG
            printf("Opening : %s\n", swapFileName);
    #endif
        swap_device_one.backend=fopen(swapFileName,"wb+");

        printf("chemin du fichier one est : %p\n", (void*)swap_device_one.backend);

        if(!swap_device_one.backend) {
    #ifdef DEBUG
            perror("Swap file opening failed!");
    #endif
            return 1;
        }
        if (init_swap_device_nodes(&swap_device_one, (unsigned long)100*1024*1024)) { 
    #ifdef DEBUG
            printf("Swap device's nodes couldn't be init'ed!\n");
    #endif
            return 1;
        }

        printf("swap device numbre one : %d\n", swap_device_one.number);

    return 0;
}


static int policiesSet1_setup(unsigned int pagesNumber) {       
    

     // swap.* setup..
    /*if (pagesNumber>workersByPagesThreshold) {
        (&evictWorkers)->next=malloc(sizeof(struct evict_worker));
        pthread_create(&((&evictWorkers)->next->thread_id), NULL, usm_handle_evt_poll, (void*)workersIterator);
    }*/
    if(usm_register_swap_policy(&swap_policy_one,"policyOne",false))
        return 1;                                                          // swap policies should only be about eviction... and it all is purely local to this code, so basically (some optim. poss.) no need of USM's registering for swap out cases in its internals.... he should do well with anything be it proc. names and so on, manually defined here... though we could help him out with a file... like the actual cfg... yeah that should be the related extent of what we should do ftm TODO
    if(usm_register_swap_policy(&usm_basic_swap_policy,"basic_policy",true))
        return 1;
    
    pthread_mutex_init(&policiesSet1Swaplock, NULL);
    //pthread_create(&((&evictWorkers)->thread_id), NULL, swapper_routine_out_dummy_periodic, NULL);
    //strcpy((&evictWorkers)->thread_name, "evictWorker-bravoOne");
    usm_evict_fallback = &handle_evict_request;     // register_evict_fallback..
    do_cond_swap_in = &check_swap_in;
    do_cond_swap_out = &check_swap_out8;
    
    int ret_tmpfs = create_and_ini_device_tmpfs();
    int ret_tmp = create_and_ini_device_tmp();
    int ret_ramfs = create_and_int_ramfs_device();

    if (!ret_tmp || !ret_tmpfs || ret_ramfs)
    {
      #ifdef DEBUG
        printf("initialisation de tmp, tmpfs et ramfs correcte\n");
      #endif
    }
    else
    {
        #ifdef DEBUG
        printf("Erreur de tmpfs ou tmp ou ramfs\n");
        #endif
    }


    return 0;

}

static void pol_exit() {

    fclose((FILE*)swap_device_two.backend);
    // structs ciao ciao here.
    //pthread_join((&evictWorkers)->thread_id,NULL);  // while nr_wk*...
    // some other pthread_join with any thread doing thingies (make a list and beef it up each time something's popped!)
    fclose((FILE *)swap_device_one.backend);          // some freeSwapDevice including this and the freeing of them nodes..

    fclose((FILE *)swap_device_three.backend);

    free(swapFileName);
    free(swapFileTmpName);
    free(swapFileRamFsName);

}

struct usm_ops dev_usm_swap_ops = {
usm_setup:
    policiesSet1_setup,
usm_free:
    pol_exit
};