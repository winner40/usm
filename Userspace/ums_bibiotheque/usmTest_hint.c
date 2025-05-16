#include "usm_entete.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/mman.h>


int main(int argc, char *argv[]) {



    const char* content = "salut carlos";


    
    // Connexion au serveur
    struct usm_return_connect *connecter = usm_connect(SOCKET, 8095);


    if (connecter == NULL) {
        fprintf(stderr, "Error : connexion au serveur\n");
        return -1;
    }

    size_t size = 4096; // Taille de chaque allocation en octets
    int num_allocations = 10;

    char *request; // Allocate on the stack instead of using malloc
    request = (char*)malloc(1024);

    bzero(request, 1024);

    getchar();
    void *addr = mmap(NULL, size*num_allocations, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    sprintf(request, "%c;%d;%p;%s", 'A', num_allocations, (void *)addr, "tmp");
    

    char *data = usm_send(request, strlen(request), *connecter);

    if (data == NULL) {
        perror("usm_send");
        return -1;
    }
  
    printf("Valeur retour est : %s\n", data);
    
    getchar();

    for (int i = 0; i < num_allocations; i++)
    { 
        sprintf(addr, "P%p-%d\n", addr, i);
        printf("%p\n", addr);
        addr = addr+4096;
         getchar();
    }


    getchar();


    for (int i = 0; i < num_allocations; i++)
    {    
            if (munmap(addr, size) == -1) {
                perror("munmap");
                return -1;
            }
    }


    free(request);
    free(data);
    usm_close(connecter);


    return 0;
}




