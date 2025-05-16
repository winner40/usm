#include "usm_entete.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096
#define FHP_PAGES 10
char swap_1[] = "tmpfs";
char swap_2[] = "tmp";

int main() {
    const size_t size = FHP_PAGES * PAGE_SIZE;


    // Connexion au serveur USM
    struct usm_return_connect *connecter = usm_connect(SOCKET, 8095);
    if (connecter == NULL) {
        perror("Erreur connexion serveur USM");
        return 1;
    }

    // Allocation mémoire
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // Création et envoi du hint de type FHP
    char request[128];
    snprintf(request, sizeof(request), "F;%p;%s", addr, swap_2);
    printf("Envoi du hint : %s\n", request);

    char *response = usm_send(request, strlen(request), *connecter);
    if (!response) {
        perror("usm_send");
        return 1;
    }

    printf("Réponse du serveur : %s\n", response);
    free(response);

    printf("Appuyez sur Entrée pour déclencher l’allocation (via page fault\n");
    getchar();

    // Écriture dans chaque page FHP (pour déclencher une seule fois l’alloc)
    for (int i = 0; i < FHP_PAGES; i++) {
        char *ptr = (char *)addr + i * PAGE_SIZE;
        snprintf(ptr, PAGE_SIZE, "Page %d: %p", i, ptr);
        printf("Écriture dans %p\n", ptr);
    }

    printf("Appuyez sur Entrée pour libérer la mémoire…\n");
    
    getchar();

    if (munmap(addr, size) != 0) {
        perror("munmap");
    } else {
        printf("Mémoire libérée avec succès\n");
    }

    usm_close(connecter);
    return 0;
}
