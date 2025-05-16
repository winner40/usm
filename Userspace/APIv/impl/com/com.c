// #include "include/com/com.h"
#include "../../include/usm/usm.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "../../../ums_bibiotheque/usm_entete.h"
#include <poll.h>

#include <arpa/inet.h>

//#define PORT 8090

#define SHM_SIZE 1024


#define CONFIG_FILE "/home/jocelyn/Bureau/flusm-devBugs-SwpMods/flusm-devBugs/Userspace/APIv/examples/project-2/alloc/src/config"
#define BUFFER_SIZE 1024



int find_gedit_index(struct processDesc *procDescList, char *nom_process)
{
    int i;

    for (i = 0; i < MAX_PROC; i++)
    {

        printf("les noms des processus : %s\n", procDescList[i].name);
        if (strcmp(procDescList[i].name, nom_process) == 0)
        {
            printf("valeur de indice : %d\n", i);
            printf("les noms des processus a interieure : %s\n", procDescList[i].name);
            return i;
        }
    }

    // printf("Aucune correspondance");
    return -1;
}

void *usm_handle_evt_poll(void* wk_args) {                              // no need anymore of "poll" and opposed concepts.. they'll be in usm_check..
    struct usm_worker* wk=(struct usm_worker*)wk_args;
    struct list_head  *tempChnItr, *eventsListIterator, *tempEvtItr;
    struct usm_channels_node *channelsListIterator;
    struct usm_event * event;
    while(1) {
        list_for_each_entry(channelsListIterator,&(wk->usm_channels),iulist) {
            int notfn=channelsListIterator->usm_channel->usm_cnl_ops->usm_check(channelsListIterator->usm_channel->fd,0);
            if(notfn) {
#ifdef DEBUG
                printf("Event up!\t\t%s\n", wk->thread_name);
#endif
ralloc:
                event=malloc(sizeof(struct usm_event));
                if (!event) {
#ifdef DEBUG
                    printf("Failed malloc in usm_handle_evt_poll\n");
                    getchar();
#endif
                    goto ralloc;
                }
                event->origin=channelsListIterator->usm_channel->fd;
                if(likely(notfn>0)) {
                    if(likely(channelsListIterator->usm_channel->usm_cnl_ops->usm_retrieve_evt(event))) {
                       
                       if (event->data != NULL)
                       {
                            free(event->data);
                       }

                       if (event->procName != NULL)
                       {
                            free(event->procName);
                       }
                       free(event);
                        continue;
                    }
                } else {
                    event->type=PROC_DTH;
                    event->channelNode=&channelsListIterator->iulist;
                }
                struct usm_events_node *eventListNode=(usm_events_node *)malloc(sizeof(usm_events_node));
                eventListNode->event=event;
                INIT_LIST_HEAD(&(eventListNode->iulist));
                list_add(&(eventListNode->iulist),&(wk->usm_current_events));
                wk->usm_current_events_length++;
            }
        }
        // wk->usm_wk_ops->usm_sort_evts(&wk->usm_channels);
        if(!(wk->usm_current_events_length))
            continue;
#ifdef DEBUG
        printf("Events' number:%d\t%s\n",wk->usm_current_events_length, wk->thread_name);
#endif
        list_for_each_safe(eventsListIterator,tempEvtItr,&(wk->usm_current_events)) {
            struct usm_events_node * eventsIterator=list_entry(eventsListIterator, usm_events_node, iulist);
            if (!(wk->usm_wk_ops->usm_handle_evt(eventsIterator->event))) {
                wk->usm_current_events_length--;
                list_del(eventsListIterator);
                if (eventsIterator->event->procName != NULL)
                    {
                        free(eventsIterator->event->procName);
                    }
                free(eventsIterator->event);
                free(eventsIterator);
            } else {
                //raise(SIGINT);
#ifdef DEBUG
                printf("Event not treated... dropping for now!\n");
                getchar();
#endif
                // list_move_tail(eventsListIterator,&(wk->usm_current_events));    | move it.. drop it... or wut?
                wk->usm_current_events_length--;
                list_del(eventsListIterator);
                if (eventsIterator->event->data != NULL)
                       {
                            free(eventsIterator->event->data);
                       }
                if (eventsIterator->event->procName != NULL)
                       {
                            free(eventsIterator->event->procName);
                       }
                free(eventsIterator->event);
                free(eventsIterator);
                continue;
            }
#ifdef DEBUG
            printf("\nEvent treated..\n");
#endif
        }
    }
}


void *usm_handle_evt_userint(void *wk_args)
{ 
    struct usm_worker *wk = (struct usm_worker *)wk_args;
    struct usm_event *event;
    struct usm_channel *channel;
    
    printf("Event up!\n");

    event = malloc(sizeof(struct usm_event));

    if (event == NULL)
    {
        printf("Échec de malloc dans usm_handle_evt_userint\n");
        exit(1);
    }

    event->origin = channel->fd;

    #ifdef DEBUG
            printf("Origin:%d\n\n", event->origin);
    #endif


    if(likely(channel->usm_cnl_ops->usm_retrieve_evt(event)))
        {

            if (!(wk->usm_wk_ops->usm_handle_evt(event)))
            {
                #ifdef DEBUG
                    printf("\nEvent treated..\n");
                #endif
            }
            
            else
            {
                printf("Event not treated... dropping for now!\n");
            }
                       
            if (event->data != NULL)
                {
                    free(event->data);
                }

            if (event->procName != NULL)
                {
                    free(event->procName);
                }
                                     
                free(event);
           
        }


     else
     {
        printf("Aucun evenement actuellement\n");
     }
    
}




int usm_hints_ret_ev(struct usm_event *event)
{

    struct usm_hint_event *receive_hint = (struct usm_hint_event *)malloc(sizeof(struct usm_hint_event));

    
    if (receive_hint == NULL)
    {
            perror("Error: allocation de la memoire receive_hint\n");
            return -1;
    }

    int receivedSize = 0;

    // Recevoir la taille en premier
    int sizeResult = recv(event->origin, &receivedSize, sizeof(receivedSize), 0);


    if (sizeResult < 0)
    {
        perror("**Erreur réception taille**");
        close(event->origin);
        return -1;
    }
    else if (sizeResult == 0)
    {
        //printf("l'envoie des donnees en attente\n");
        close(event->origin);
        return -1;
    }
    else
    {

        int dataResult = recv(event->origin, receive_hint, receivedSize, 0);
        

        if (dataResult < 0)
        {
            perror("**Erreur réception données**");
            return -1;
        }
        else if (dataResult == 0)
        {
            printf("La connexion est fermée par le client\n");
            close(event->origin);
            return -1;
        }
        else
        {

            //printf("taille de la requete: %d\n",receive_hint->length);
            //printf("Taille des donnees recus du client : %d\n", dataResult);
            //printf("pid du processus : %d\n", receive_hint->process_id);
            //printf("nom du processus: %s\n", receive_hint->nom_process);
            //printf("data requeste: %s\n", receive_hint->request);     

            event->origine_end_point = event->origin;      

            int index = find_gedit_index(procDescList, receive_hint->nom_process);
            if (index != -1)
            {
                event->origin = index;
            }
            else
            {
                perror("Aucun processus ne correspond a ce nom\n");
                return -1;
            }

            //event->length = 4096;

            //printf("l'index du processus est : %d\n", index);
            event->procName = (char*)malloc(sizeof(receive_hint->nom_process));
            memcpy(event->procName, receive_hint->nom_process, sizeof(receive_hint->nom_process));
            event->taille = receive_hint->length;
            event->type = HINTS;

            event->data=(char*)malloc(receive_hint->length);
         
            memcpy(event->data,receive_hint->request,receive_hint->length);

            event->donne=(char*)malloc(receive_hint->length);
         
            memcpy(event->donne,receive_hint->request,receive_hint->length);
            
            //printf("le nom du processus est : %s\n", event->procName);
            //printf("type du processus : %d\n", event->type);
            //printf("le nrequest : %s\n", event->data);                       
            
        }

            //printf("Tout c'est bien passe\n");  

           
    }         

     free(receive_hint);
    
    return 0;
}


int usm_hint_subm_ev(struct usm_event *event) { 

    
    printf("ha\n");
    //printf("ici event data : %s\n", event->data);

    //printf("ici event origin : %d\n", event->origine_end_point);

    // Recevoir la taille en premier
    ssize_t sizeResult = send(event->origine_end_point, &event->taille, sizeof(event->taille), 0);

    // Vérifier s'il y a des erreurs dans l'opération d'envoi
    if (sizeResult == -1) {
        perror("send size");
        return -1;
    }

    // Envoie de la donnee 
    ssize_t dataSize = send(event->origine_end_point, event->data, event->taille, 0);

    // Vérifier s'il y a des erreurs dans l'opération d'envoi
    if (dataSize == -1) {
        perror("send data");
        return -1;
    }

    //printf("result size : %zd\n", sizeResult);
    //printf("result data : %zd\n", dataSize);

    return 0;
 
}


/*usm_handle_evt_periodic(struct usm_worker* wk){           //.. this type of worker should be specialized/special enough to be launched scarcily ; one example'd be the thresholds' policies applier...
    int budget=wk->usm_budget;
    while(1){
again:
        // À implémenter: parcourir wk->usm_channels et placer les événements dans wk->usm_current_events
        budget--;
        if(budget!=0)
            goto again;
        wk->usm_wk_ops->usm_sort_evts();
        for(int i=0;i<wk->usm_current_events_length;i++){//on traite les evenements
            wk->usm_wk_ops->usm_handle_evt(wk->usm_current_events[i]);
        }
        wk->usm_current_events_length=0;
        sleep(5);
    }
}*/                                                         // what could be interesting would be a ring buffer... with a max. number in the loop collecting the events upon which we'd break....

int usm_uffd_ret_ev(struct usm_event * event) {
    struct uffd_msg msg;
#ifdef DEBUG
    printf("Retrieving UFFD event\n");
#endif
    int readres = read(event->origin,&msg,sizeof(msg));
    if(readres==-1) {
#ifdef DEBUG
        printf("[Mayday] Failed to get event\n");
#endif
        return errno;
    }
    // event->process_pid, event->origin to comm_fd, once channels loosened from processes
    switch (msg.event) {                 // dyent..
    case UFFD_EVENT_PAGEFAULT:
        event->type=ALLOC;
        event->vaddr=msg.arg.pagefault.address;
        // event->length=globalPageSize;        | obvious in the case of a page fault... the size of it'd be gPS, but it's accessible everywhere, so... ; but this should be needed/used later on.
        event->flags=msg.arg.pagefault.flags;
        // if unlikely...
        event->offst=msg.arg.pagefault.entry_content*SYS_PAGE_SIZE;
        break;
    case UFFD_EVENT_FORK:
        event->type=NEW_PROC;
        event->origin=msg.arg.fork.ufd;
        break;
    case UFFD_EVENT_REMAP:
        event->type=VIRT_AS_CHANGE;
        // old..new...addrs
        break;
    case UFFD_EVENT_UNMAP:  // this is not specifically true (uffd's way of handling remap...).. | toMod.
    case UFFD_EVENT_REMOVE: // possibly done by madvise...
        event->type=FREE;
        event->vaddr=msg.arg.remove.start;
        event->length=msg.arg.remove.end-msg.arg.remove.start;
        break;
    /*case UFFD_EVENT_:     // probably new ones later..
        event->type=__;
        break;*/
    default:
#ifdef DEBUG
        printf("[Mayday] Unspecified event\n");
#endif
        return 1;
    }
    return 0;
}



int usm_uffd_subm_ev(struct usm_event * event) {
    event->write = 1;
    usm_set_pfn(event->origin,event->vaddr,event->paddr,event->write,0);
}

int usm_freed_page_ret_ev(struct usm_event * event) {
    unsigned long pfn = 0;
    int readres = read(usmFreePagesFd,&pfn,sizeof(pfn));
    if(readres==-1) {
#ifdef DEBUG
        printf("[Mayday] Failed to get freed page event\n");
#endif
        return errno;
    }
    
    event->paddr=pfn;
    event->type=PHY_AS_CHANGE;      // but free..? Wut the.... | dyend.
    // origin? Retrievable elsewhere too..
    return 0;
}

int usm_new_proc_ret_ev(struct usm_event * event) {
    char procName [16];
    int process = read(usmNewProcFd,&procName,sizeof(procName));
    if(process<=0) {
#ifdef DEBUG
        printf("[Mayday] Failed to get new process event\n");
#endif
        return errno;
    }
    event->procName=NULL;
#ifdef DEBUG
    printf("Received process' name : %s\n", procName);              /* TODO : investigate its disappearance in the cas of unnamed tasks (e.g. background saver of Redis).. just wth..*/
    getchar();
#endif
    event->origin=process;          // not really "origin" here but pfd.. meh...
    event->type=NEW_PROC;
    if(strlen(procName)!=0) {
        event->procName=malloc(sizeof(procName));
        strcpy(event->procName,procName);
    }
    write(usmNewProcFd,NULL,0);          // shan't be nec....
    return 0;
}

/*int usm_new_proc_subm_ev(struct usm_event * event, int comm_fd) {

}*/

int usm_poll_check(int channel_id, int timeout) {
    struct pollfd pollfd[1];
    pollfd[0].fd = channel_id;
    pollfd[0].events = POLLIN;
    int res=poll(pollfd,1,timeout);
    if(res<=0)       // basically undefined..
        return res;
    if(pollfd[0].events!=POLLIN || pollfd[0].revents!=POLLIN)
        return -1;
    return POLLIN;
}

int usm_poll_check_hints_new_connection(int channel_id, int timeout)
{   
    //printf("J'arrive ici\n");
    struct pollfd pollfd[1];
    pollfd[0].fd = channel_id;
    pollfd[0].events = POLLIN|POLLPRI;;
    int res = poll(pollfd, 1, timeout);

    if (res <= 0) // basically undefined..
        return res;

    if (!(pollfd[0].revents & POLLIN)) {

        printf("valeur du channel : %d\n", channel_id);
        printf("Ici c'est erreur\n");
        return -1;
    }
   
    return POLLIN;
}

int usm_hints_ret_ev_new_connection(struct usm_event *event)
{
    
    printf("voici bien le port cherche propre : %d\n", port);

    read_server_config();

    printf("voici bien le port cherche : %d\n", port);

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    int socket_client = accept(usm_com_end_point, (struct sockaddr *)&address, (socklen_t *)&addrlen);

    if (socket_client > -1)
    {
        printf("Connexion accepte!!!\n");
        printf("Serveur connected au client  : %d\n", socket_client);
        event->origin = socket_client;
        event->type = NEW_HINT_CONNECTION;
        return 0;
    }

    return -1;
}


struct usm_channel_ops usm_cnl_hints_new_proc_ops = {
    .usm_retrieve_evt = usm_hints_ret_ev_new_connection,
    .usm_submit_evt = usm_uffd_subm_ev,
    .usm_check = usm_poll_check_hints_new_connection};

struct usm_channel_ops usm_cnl_hints_ops = {
    .usm_retrieve_evt = usm_hints_ret_ev,
    .usm_submit_evt = usm_hint_subm_ev,
    .usm_check = usm_poll_check};




struct usm_channel_ops usm_cnl_userfaultfd_ops= {
    .usm_retrieve_evt=   usm_uffd_ret_ev,
    .usm_submit_evt=     usm_uffd_subm_ev,
    .usm_check=          usm_poll_check
};

struct usm_channel_ops usm_cnl_freed_ops= {
    .usm_retrieve_evt=   usm_freed_page_ret_ev,
    .usm_check=          usm_poll_check
};

struct usm_channel_ops usm_cnl_new_process_ops= {
    // usm_init :
usm_retrieve_evt:
    usm_new_proc_ret_ev,
usm_check:
    usm_poll_check
    //usm_submit_evt:     usm_new_proc_subm_ev
};

//struct usm_channel usm_cnl_userfaultfd= {.usm_cnl_ops=usm_cnl_userfaultfd_ops};

struct usm_channel usm_cnl_freed_pgs;
struct usm_channel usm_cnl_nproc;

struct usm_worker usm_wk_uffd;
struct usm_worker usm_wk_free_pgs;
struct usm_worker usm_wk_nproc;
