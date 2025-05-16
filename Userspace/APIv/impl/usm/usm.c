

unsigned long uintr_received;
unsigned int uvec_fd;

#include"../../../ums_bibiotheque/usm_entete.h"

#include "../../include/usm/usm.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <arpa/inet.h>
#include <string.h>


void * usmZone = NULL;
struct page * pagesList = NULL;
int usmCMAFd, usmNewProcFd, usmFreePagesFd = 0;
int usm_com_end_point, usm_com_hint_userI = 0;

struct processDesc *procDescList;

struct usm_global_ops global_ops;



unsigned long poolSize = 0;
unsigned int globalPagesNumber = 0;
unsigned int globalPageSize = 0;
unsigned int workersNumber = 0;
unsigned short policiesNumber = 0;
unsigned short allocPoliciesNumber = 0;
unsigned short swapPoliciesNumber = 0;
struct hashmap *usm_alloc_policies;
struct hashmap *usm_swap_policies;
char * allocAssignmentStrategy;
struct hashmap *usm_process_linking;
unsigned long allocPolicyThresholds [100];
int initThreshold = 0;

int inithint = 0;

char *type;
int port;
int fd;

int init = 0;

pthread_t policiesWatcher;

pthread_t hintWatcherInterrupt;



#define CONFIG_FILE "/home/usm/usm/Userspace/APIv/examples/project-2/alloc/src/config"
#define BUFFER_SIZE 1024


// Fonction pour lire le fichier de configuration et extraire l'adresse IP et le port du serveur
void read_server_config() {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (file == NULL) {
        perror("Erreur lors de l'ouverture du fichier de configuration");
        exit(EXIT_FAILURE);
    }
   

    printf("bien carlos\n");

    char line[BUFFER_SIZE];
     type  = (char*)malloc(BUFFER_SIZE);

    while (fgets(line, sizeof(line), file)) {
        char *key = strtok(line, "=");
       
        char *value = strtok(NULL, "=");
        
        if (strcmp(key, "port") == 0) {
            port = atoi(value);
            printf("valeur du port : %d\n", port);
        } else if (strcmp(key, "type") == 0) {
            strcpy(type, value);
            // Supprimer le caractère de nouvelle ligne
            //type[strcspn(type, "\n")] = 0;
        }

    }

    fclose(file);
    if (port == -1) {
        fprintf(stderr, "Port non trouvé dans le fichier de configuration\n");
        exit(EXIT_FAILURE);
    }
}


// Fonction pour mettre à jour le fichier de configuration avec la valeur du socket
void update_config_with_socket(int fd_int) {
    FILE *file = fopen(CONFIG_FILE, "a");
    if (file == NULL) {
        perror("Erreur lors de l'ouverture du fichier de configuration");
        exit(EXIT_FAILURE);
    }

    fprintf(file, "fd_int=%d\n", fd_int);

    fclose(file);
}


int usm_create_hints_com_end_point()
{

    read_server_config();

    
    if (strcmp(type, "SOCKET") >= 0)
    {
        int server_fd;
        
        struct sockaddr_in address;
        int addrlen = sizeof(address);
    
        // Crée le descripteur de fichier de socket
        server_fd = socket(AF_INET, SOCK_STREAM, 0); 
        
        //memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = INADDR_ANY;
    
        
    
        // pour pouvoir lié avec le meme port sur differente connexion
        int reuse = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));


        // Attache le socket à l'adresse et au port spécifiés
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("ERROR : echec d'attachement\n");
            return -1;
        }

        //Commence à écouter les connexions entrantes
        if (listen(server_fd, 20) < 0) {
            perror("ERROR : aucune connexion entrante\n");
            return -1;
        }

        printf("Le serveur ecoute sur le port %d...\n", port);



        return server_fd;
    }
    else
    {
        printf("ERROR : sur le type de communication\n");
        return -1;
    }
    
}



void *usm_handle_policies_watcher() {
    while (1) {
        default_alloc_policy=*(struct usm_alloc_policy_ops *)allocPolicyThresholds[(usedMemory/poolSize)*100];
#ifdef DEBUG
        printf("Applied policy at threshold %ld\n",(usedMemory/poolSize)*100);
#endif
        sleep(15);
    }
}

int usm_policy_compare(const void *a, const void *b, void *udata) {
    const struct usm_policy *pa=a;
    const struct usm_policy *pb=b;
    return strcmp(pa->name,pb->name);
}

uint64_t usm_policy_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct usm_policy *policy= item;
    return hashmap_sip(policy->name, strlen(policy->name), seed0, seed1);
}

int usm_plink_compare(const void *a, const void *b, void *udata) {
    const struct usm_process_link *pa=a;
    const struct usm_process_link *pb=b;
    return strcmp(pa->procName,pb->procName);
}

uint64_t usm_plink_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const struct usm_process_link *plink= item;
    return hashmap_sip(plink->procName, strlen(plink->procName), seed0, seed1);
}


volatile unsigned long usedMemory = 0;
volatile unsigned long swappedMemory = 0;
unsigned long basePFN;

struct usm_worker usmWorkers;

void appendUSMChannel(struct usm_channel_ops *ops, int channelFd, struct list_head *workersChannelList) {           // prio. toBeAdded!
    int retries=0;
retryChn:
    struct usm_channels_node *tempChannel = malloc(sizeof(struct usm_channels_node));
    if (tempChannel==NULL) {
        retries++;
        if(retries<3)
            goto retryChn;
        printf("[Sys] Couldn't allocate memory for a new channel node! Aborting.\n");
        exit(1);
    }
    INIT_LIST_HEAD(&(tempChannel->iulist));
    retries=0;
retryUsmChn:
    tempChannel->usm_channel=malloc(sizeof(struct usm_channel));            // mutex to be used...
    if (tempChannel->usm_channel==NULL) {
        retries++;
        if(retries<3)
            goto retryUsmChn;
        printf("[Sys] Couldn't allocate memory for a new channel! Aborting.\n");
        exit(1);
    }
    pthread_mutex_init(&(tempChannel->chnmutex),NULL);
    tempChannel->usm_channel->usm_cnl_ops=ops;
    tempChannel->usm_channel->fd=channelFd;
    if (init!=0) {
        struct usm_channels_node *channelToLock = NULL;
        if (!list_empty(workersChannelList))
            channelToLock = list_first_entry(workersChannelList,usm_channels_node,iulist);
        if(channelToLock!=NULL)
            pthread_mutex_lock(&(channelToLock->chnmutex));
        list_add(&(tempChannel->iulist),workersChannelList);
        if(channelToLock!=NULL)
            pthread_mutex_unlock(&(channelToLock->chnmutex));
    } else
        list_add(&(tempChannel->iulist),workersChannelList);
#ifdef DEBUG
    printf("[Sys] Channel appended!\n");
#endif
}

int usm_init(unsigned long poolSize, unsigned int pageSize) {
    srand(time(NULL));
    procDescList=malloc(MAX_PROC*sizeof(struct processDesc));
    int uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    int res = 0;


    usm_com_end_point=usm_create_hints_com_end_point();

    if (usm_com_end_point == -1)
    {
       perror("Aucune connexion est attachée");
       return errno;
    }
    

    if (uffd == -1) {
        perror("syscall/userfaultfd");
        return errno;
    }
    struct uffdio_api uffdio_api;
    uffdio_api.api = UFFD_API;
    uffdio_api.features = 0;
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1) {
        perror("\t\t[Mayday] UFFD_API ioctl failed! Exiting.");
        return errno;
    }
usmCMAinit:
    usmCMAFd=open("/dev/USMMcd", O_RDWR);
    if(usmCMAFd<0) {
        printf("[Mayday] CMA untakable! Check USM's module. Try again?\n");
        getchar();
        goto usmCMAinit;
    }
usmFPInit:
    usmFreePagesFd=open("/proc/usmPgs", O_RDWR);
    if(usmFreePagesFd<0) {
        printf("[Mayday] Free pages unseeable! Check USM's module. Try again?\n");
        getchar();
        goto usmFPInit;
    }
usmNPInit:
    usmNewProcFd=open("/proc/usm", O_RDWR);
    if(usmNewProcFd<0) {
        printf("[Mayday] New processes unreceivable! Check USM's module. Try again?\n");
        getchar();
        goto usmNPInit;
    }
    globalPagesNumber = poolSize/SYS_PAGE_SIZE; // globalPageSize somehow later on for different sized pages' forming..
#ifdef DEBUG
    printf("[Sys] Pool size : %ldB | %.2fKB | %.2fMB\n", poolSize, (float)poolSize/1024, (float)poolSize/1024/1024);
    printf("[Sys] Global pages' number : %u\n", globalPagesNumber);
#endif
    usmZone = mmap(NULL, poolSize, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED, usmCMAFd, 0);
    if(usmZone==MAP_FAILED) {
#ifdef DEBUG
        perror("[Sys] mmap failed!");
#endif
        return errno;
    }
#ifdef DEBUG
    else
        printf("[Sys] Mapped!\n");
#endif
    memset(usmZone, 0, poolSize);   //toComment
#ifdef DEBUG
    printf("[Sys] Zeroed!\n");
#endif
    basePFN = usm_get_pfn(uffd,(unsigned long)usmZone,(unsigned long)globalPagesNumber, 0);
    if (basePFN==1) {
#ifdef DEBUG
        printf("[Sys] Couldn't get first PFN!\n");
#endif
        return 1;
    }
    pagesList=(struct page *) malloc (sizeof(struct page)*globalPagesNumber);
    for (unsigned int i = 0; i<globalPagesNumber; i++)
        insertPage(&pagesList, basePFN+i, (intptr_t)(usmZone+(i*SYS_PAGE_SIZE)), i);
/*
    unsigned int chunksNumber = poolSize/USM_CHUNK_SIZE;
    usmZone=mmap(NULL, chunksNumber?USM_CHUNK_SIZE:poolSize, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED, usmCMAFd, 0);              // usmZone & poolSize needn't be global.
    if(usmZone==MAP_FAILED) {
        perror("[Sys] mmap failed! Aborting.");
        exit(1);
    }
    memset(usmZone, 0, chunksNumber?USM_CHUNK_SIZE:poolSize);
    basePFN = usm_get_pfn(uffd,(unsigned long)usmZone,(unsigned long)globalPagesNumber);
    if (basePFN==1) {
#ifdef DEBUG
        printf("[Sys] Couldn't get first PFN!\n");
#endif
        return 1;
    }
    unsigned int tempPos = 0;
nextChunk:
    for (unsigned int i = tempPos; i<(chunksNumber?USM_CHUNK_SIZE/SYS_PAGE_SIZE:(poolSize%USM_CHUNK_SIZE)/SYS_PAGE_SIZE); i++)
        insertPage(&pagesList, basePFN+i, (intptr_t)(usmZone+(i*SYS_PAGE_SIZE)), i);
    if (--chunksNumber>0 || (--chunksNumber==0 && poolSize%USM_CHUNK_SIZE)) {
        usmZone=mmap(NULL, chunksNumber?USM_CHUNK_SIZE:poolSize%USM_CHUNK_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED, usmCMAFd, 0);
        if(usmZone==MAP_FAILED) {
            perror("[Sys] Subsequent mmap failed! Aborting.");
            exit(1);
        }
        memset(usmZone, 0, chunksNumber?USM_CHUNK_SIZE:poolSize%USM_CHUNK_SIZE);
        tempPos+=USM_CHUNK_SIZE/SYS_PAGE_SIZE;
        goto nextChunk;
    }
*/
#ifdef DEBUG
    printf("[Sys] Inserted PFNs range : %lu | %lu \n", pagesList->physicalAddress, (pagesList+globalPagesNumber-1)->physicalAddress);
#endif
#ifdef DEBUG
    printf("[Sys] USM page table initialized!\n");
#endif
    struct usm_worker *workersIterator = &usmWorkers;
    workersNumber+=3;
#ifdef DEBUG
    printf("[Sys] Number of workers : %d\n", workersNumber);
#endif
    for (int i = 0; i<workersNumber; i++) {
        INIT_LIST_HEAD(&(workersIterator->usm_channels));
        INIT_LIST_HEAD(&(workersIterator->usm_current_events));
        workersIterator->usm_current_events_length=0;
        workersIterator->usm_wk_ops=malloc(sizeof(struct usm_worker_ops));
        workersIterator->usm_wk_ops->usm_handle_evt=usm_handle_events;
        workersIterator->usm_wk_ops->usm_sort_evts=usm_sort_events;
        workersIterator->thread_name=(char*)malloc(26);
        bzero(workersIterator->thread_name, 26);
        sprintf(workersIterator->thread_name, "Worker-%d", i);
#ifdef DEBUG
        printf("[Sys] Init'ed worker %s!\n", workersIterator->thread_name);
#endif
        if(i==workersNumber-1) {
            sprintf(workersIterator->thread_name + strlen(workersIterator->thread_name), "/freePagesReceiver");
            appendUSMChannel(&usm_cnl_freed_ops, usmFreePagesFd, &(workersIterator->usm_channels));
        }
        if(i==workersNumber-2) {
            sprintf(workersIterator->thread_name + strlen(workersIterator->thread_name), "/processesReceiver");
            appendUSMChannel(&usm_cnl_new_process_ops, usmNewProcFd, &(workersIterator->usm_channels));
        }

        if(i==workersNumber-3){
          
            if (strcmp(type, "SOCKET") >= 0)
            {
                appendUSMChannel(&usm_cnl_hints_new_proc_ops, usm_com_end_point, &(workersIterator->usm_channels));
            }

            // else if (strcmp(type, "INTERRUPT") == 0)
            // {
            //     appendUSMChannel(&usm_cnl_hints_new_userI_ops, usm_com_end_point, &(workersIterator->usm_channels));
            // }
            }

        if(i<workersNumber-1) {
            workersIterator->next=malloc(sizeof(struct usm_worker));
            workersIterator=workersIterator->next;
        } else
            workersIterator->next=NULL;
#ifdef DEBUG
        printf("[Sys] One worker pushed!\n");
#endif
    }
    workersNumber-=3;       // meh..
    int procHolderIndex = 0;
    while(procHolderIndex<MAX_PROC) {
        procDescList[procHolderIndex].alloc_policy=&default_alloc_policy;
        procDescList[procHolderIndex].swap_policy=&default_swap_policy;
        procDescList[procHolderIndex].allocated=0;
        procDescList[procHolderIndex].swapped=0;
        INIT_LIST_HEAD(&(procDescList[procHolderIndex].swapList));
        INIT_LIST_HEAD(&(procDescList[procHolderIndex].usedList));
        INIT_LIST_HEAD(&(procDescList[procHolderIndex].swapCache));
        procDescList[procHolderIndex].prio=0;
        pthread_mutex_init(&procDescList[procHolderIndex].lock,NULL);
        pthread_mutex_init(&procDescList[procHolderIndex].alcLock,NULL);
        pthread_mutex_init(&procDescList[procHolderIndex].swpLock,NULL);
        procHolderIndex++;
    }
    usm_alloc_policies=hashmap_new(sizeof(struct usm_policy), 0, 0, 0, usm_policy_hash, usm_policy_compare, NULL, NULL);
    usm_swap_policies=hashmap_new(sizeof(struct usm_policy), 0, 0, 0, usm_policy_hash, usm_policy_compare, NULL, NULL);
    usm_process_linking=hashmap_new(sizeof(struct usm_process_link), 0, 0, 0, usm_plink_hash, usm_plink_compare, NULL, NULL);
    init=1;
    return 0;
}

int usm_set_alloc_policy_assignment_strategy(char *alloc_policy_assignment_strategy_cfg) {
    int ret = 0;
    FILE * fp = fopen(alloc_policy_assignment_strategy_cfg,"r");
    if(!fp) {
#ifdef DEBUG
        perror("[Sys] Cannot open specified file");
#endif
        ret=1;
    }
    char ln[1000];
    char arg1[20];
    while (fgets(ln,sizeof(ln),fp)) {
        if(ln[0]=='#' || sscanf(ln, "%s ", arg1)<=0)
            continue;
        if (!strcmp(arg1,"memory")) {
            char arg2 [20];
            sscanf(ln+strlen(arg1), "%s", arg2);
            poolSize=atoi(arg2);
            /*if(poolSize>MAX_USM_SIZE) {               TODO just communicate availabilities seamlessly... from the module
#ifdef DEBUG
                printf("[Sys] Maximum available memory : %uMB.\n[Sys] Aborting.\n",MAX_USM_SIZE);
#endif
                break;
            }*/
#ifdef DEBUG
            printf("[Sys] [Conf] Memory : %ldMB\n",poolSize);
#endif
            poolSize*=1024*1024;
            continue;
        }
        if (!strcmp(arg1,"workers")) {
            char arg2 [20];
            sscanf(ln+strlen(arg1), "%s", arg2);
            workersNumber=atoi(arg2);
#ifdef DEBUG
            printf("[Sys] [Conf] Workers : %d\n",workersNumber);
#endif
            continue;
        }
        if (!strcmp(arg1,"page")) {
            char arg2 [20];
            sscanf(ln+strlen(arg1), "%s", arg2);
            globalPageSize=atoi(arg2);
#ifdef DEBUG
            printf("[Sys] [Conf] Page : %dB\n",globalPageSize);
#endif
            if (!init)
                goto temp;
            continue;
        }
        if (!strcmp(arg1,"threshold")) {
            char arg2 [20], arg3 [20];
            struct usm_policy *alloc_policy;
            sscanf(ln+strlen(arg1), "%s %s", arg2, arg3);
#ifdef DEBUG
            printf("[Sys] threshold, policy : %s | %s\n",arg2,arg3);
#endif
            if (!initThreshold) {
                int idx = 0;
                while (idx<100)
                    allocPolicyThresholds[idx++]=(unsigned long)&default_alloc_policy;
                initThreshold++;
            }
            alloc_policy=(struct usm_policy *)hashmap_get(usm_alloc_policies, &(struct usm_policy) {
                .name=arg3
            });
            if (!alloc_policy) {
#ifdef DEBUG
                printf("[Sys] Couldn't find %s policy, please check provided configuration file.\n", arg3);
#endif
                break;
            }
#ifdef DEBUG
            else
                printf("[Sys] Found %s policy.\n", alloc_policy->name);
#endif
            int tempPerc = atoi(arg2);
            while(tempPerc<100)
                allocPolicyThresholds[tempPerc++]=alloc_policy->ops;
            continue;
        }
        if (!strcmp(arg1,"process")) {
            char arg2 [20];
            sscanf(ln+strlen(arg1), "%s", arg2);
            if(strcmp(arg2,"swap") && strcmp(arg2,"alloc")) {
#ifdef DEBUG
                printf("Unknown process configuration argument : %s\n", arg2);
#endif
                break;
            }
            char arg3 [20], arg4 [20];
            struct usm_policy *policy;
            sscanf(ln+strlen(arg1)+strlen(arg2)+1, "%s %s", arg3, arg4);
#ifdef DEBUG
            printf("[Sys] Process :  %s | %s\n", arg3, arg4);
#endif
            if(!strcmp(arg2,"alloc"))
                policy=(struct usm_policy *)hashmap_get(usm_alloc_policies, &(struct usm_policy) {
                    .name=arg4
                });
            else
                policy=(struct usm_policy *)hashmap_get(usm_swap_policies, &(struct usm_policy) {
                    .name=arg4
                });
            if (!policy) {
#ifdef DEBUG
                printf("[Sys] Couldn't find %s policy, please check provided configuration file.\n", arg4);
#endif
                break;
            }
#ifdef DEBUG
            else
                printf("[Sys] Found %s | %s policy #%s\n", arg3, policy->name, arg2);
#endif
            struct usm_process_link * plink= (struct usm_process_link *)hashmap_get(usm_process_linking,&(struct usm_process_link) {
                    .procName=arg3
            });
            if (plink) {
#ifdef DEBUG
                printf("[Sys] Appending to found policy for %s.\n", plink->procName);
#endif
            }
            else {
                plink=(struct usm_process_link *)malloc(sizeof(struct usm_process_link));
                plink->procName=malloc(strlen(arg3));
                strcpy(plink->procName,arg3);
                plink->prio=0;
            }
            if(!strcmp(arg2,"alloc"))
                plink->alloc_pol=policy->ops;
            else
                plink->swp_pol=policy->ops;
#ifdef DEBUG
            printf("Adding found policy definition to |%s|\n",(&(struct usm_process_link) {
                .procName=arg3,.alloc_pol=policy->ops
            })->procName);
#endif
            if(!hashmap_set(usm_process_linking,plink)) {
                if(hashmap_oom(usm_process_linking)) {
#ifdef DEBUG
                    printf("[Sys] Couldn't allocate new hashmap member's memory.\n");
#endif
                    break;
                }
                struct usm_process_link * ct;
                ct=(struct usm_process_link *)hashmap_get(usm_process_linking,&(struct usm_process_link) {
                    .procName=arg3
                });
                if (!ct) {
#ifdef DEBUG
                    printf("Wut in the..\n");
#endif
                    break;
                }
#ifdef DEBUG
                printf("Overwritten |%s|'s policy definition with %s %s policy\n",ct->procName, policy->name, arg2);
#endif
            }
            continue;
        }
        if (!strcmp(arg1,"priority")) {             // priorities should always come after the policies binding in the cfg file..
            char arg2 [20], arg3 [20];
            struct usm_process_link *plink;
            sscanf(ln+strlen(arg1), "%s %s", arg2, arg3);
#ifdef DEBUG
            printf("[Sys] Priority :  %s | %s\n", arg2, arg3);
#endif
            plink=(struct usm_process_link *)hashmap_get(usm_process_linking, &(struct usm_process_link) {
                .procName=arg2
            });
            if (plink) {
#ifdef DEBUG
                printf("[Sys] Found preexisting %s with %lu policy.\n", arg2, plink->alloc_pol);    // TODO ..hmm....
#endif
            } else {
                plink=malloc(sizeof(struct usm_process_link));
                plink->procName=malloc(strlen(arg2));
                plink->alloc_pol=0;
                plink->swp_pol=0;
                strcpy(plink->procName,arg2);
            }
            plink->prio=atoi(arg3);
#ifdef DEBUG
            printf("Adding : |%s|\t%s\n",arg2, arg3);
#endif
            if(!hashmap_set(usm_process_linking,plink)) {  //&(struct usm_process_link){.procName=arg2,.pol=/*alloc_policy->ops*/arg3})){
                if(hashmap_oom(usm_process_linking)) {
#ifdef DEBUG
                    printf("[Sys] Couldn't allocate new hashmap member's memory.\n");
#endif
                    break;
                }
                struct usm_process_link * ct;
                ct=(struct usm_process_link *)hashmap_get(usm_process_linking,plink);  // &(struct usm_process_link){.procName=arg2}
                if (!ct) {
#ifdef DEBUG
                    printf("Wut in the..\n");
#endif
                    break;
                }
#ifdef DEBUG
                printf("Overwritten |%s|'s policy definition with priority %d.\n",ct->procName, plink->prio);
#endif
            }
            continue;
        }
#ifdef DEBUG
        printf("Unknown configuration argument : %s\n", arg1);
#endif
        break;
        //memset(arg1, 0, sizeof(arg1));
    }
    if(!feof(fp)) {
#ifdef DEBUG
        printf("[Sys] Couldn't reach end of config. file.\n");
#endif
        ret=1;
    }
    if(globalPageSize==0)         // round it to the closest lower (bigger?) multiple.. TODO
        globalPageSize=SYS_PAGE_SIZE;
temp:
    fclose(fp);
    return ret;
}

/*
    Parse les arguments et initialise des variables globales
*/
void usm_parse_args(char *argv[], int argc) {
    if (argc<2) {
        printf("[Sys] Please specify config. file location...\n");
        exit(1);
    }
    allocAssignmentStrategy=argv[1];
    if (usm_set_alloc_policy_assignment_strategy(allocAssignmentStrategy)) {
        printf("[Sys] Failed to assign strategy ; aborting.\n");
        exit(1);
    }
}

bool usm_policy_iter(const void *item, void *udata) {
    const struct usm_policy *a_usm_policy = item;
    printf("Initialisation de la politique  :  %s\n", a_usm_policy->name);

    if (strcmp("basic_policy", a_usm_policy->name)!=0)
    {
        ((struct usm_alloc_policy_ops*)(a_usm_policy->ops))->usm_init();
        return true;
    }
   
}


void usm_launch(struct usm_global_ops globOps) {
    struct usm_ops *alloc_usm=globOps.dev_usm_alloc_ops;
    struct usm_ops *swap_usm=globOps.dev_usm_swap_ops;
    global_ops=globOps;
    if (usm_init(poolSize,globalPageSize)) {
        printf("[Sys] Couldn't initialize USM.\n");
        exit(1);
    }
    // if not NULL... but dev. should know it can't..hmmm..
    if(!alloc_usm || !swap_usm) {
        printf("[Sys] Provided NULL setup(s).\n");
        exit(1);
    }
    if (alloc_usm->usm_setup(globalPagesNumber)) {
        printf("[Sys] Failed to apply provided alloc. setup.\n");
        exit(1);
    }
    if (swap_usm->usm_setup(globalPagesNumber)) {
        printf("[Sys] Failed to apply provided swap. setup.\n");
        exit(1);
    }
    if (usm_set_alloc_policy_assignment_strategy(allocAssignmentStrategy)) {
        printf("[Sys] Failed to assign strategy ; aborting.\n");
        exit(1);
    }


    //Appel de la fonction d'initialisation des politiques utilisateurs  
    hashmap_scan(usm_alloc_policies, usm_policy_iter, NULL);

    printf("tout va bien\n");

    struct usm_worker *workersIterator = &usmWorkers;
    int wk_nr = 0;

    while(workersIterator) {
            // if..type...poll....else.
            pthread_create(&(workersIterator->thread_id), NULL, usm_handle_evt_poll, (void*)workersIterator);
            workersIterator=workersIterator->next;
            #ifdef DEBUG
                    printf("Worker %d launched!\n", wk_nr++);
            #endif
    }
    

    // TODO : check on default policies if init'ed ... either here or in init (set_assignment_strat.)
    while(workersIterator) {
        // if..type...poll....else.
        pthread_create(&(workersIterator->thread_id), NULL, usm_handle_evt_poll, (void*)workersIterator);
        workersIterator=workersIterator->next;
#ifdef DEBUG
        printf("Worker %d launched!\n", wk_nr++);
#endif
    }
    if (initThreshold)
        pthread_create(&policiesWatcher,NULL,usm_handle_policies_watcher, NULL);
    workersIterator=&usmWorkers;
    while(workersIterator)
        pthread_join(workersIterator->thread_id,NULL);          // some pth_k|c there bruh...
    if (initThreshold)
        pthread_join(policiesWatcher,NULL);

    if (inithint==0)
        pthread_join(hintWatcherInterrupt,NULL);
    
    free(type);

    // cleaning...
    hashmap_free(usm_alloc_policies);
    hashmap_free(usm_process_linking);
    // if not NULL.. but he/she should've complied...
    //usm->usm_free();
    alloc_usm->usm_free();
    swap_usm->usm_free();
}


/*
    Helpers functions to transfer here later... or simply globally define them...
*/