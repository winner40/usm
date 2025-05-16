#ifndef USMENTETE_H
#define USMENTETE_H

// Enumeration du type de communication qui sera établie avec USM
enum usm_connect_type {SOCKET, INTERRUPT};


// Structure de données de la donnée qui sera transmise via la connexion établie
struct usm_hint_event
{
    int process_id;
    int length;
    char nom_process[30];
    char request[1024];

};

// structure pour savoir quelle connexion a ete etarblie 
struct usm_return_connect
{
    enum usm_connect_type type;
    int usm_port;
    int usm_fd;
};

// fonctions sur pour creer une connexion
int create_and_connect_socket(int port);


// Etablir une connexion avec un thread serveur dans USM
struct usm_return_connect *usm_connect(enum usm_connect_type connect_type, int usm_port);

// Envoyer une requête de traitement par le thread dans USM
char* usm_send(char *request, int length, struct usm_return_connect usm_type_connect);

// fonction qui retourne le nom du processus a partir de son pid
char* getProcessNameFromPID(int pid);

//structure de donnee contenant la reponse serveur
struct usm_hint_receive {
    int length;
    char reponse[100];
    
};

// fonction charge de contenir la reponse du serveur au client
struct usm_hint_receive *usm_receive(struct usm_return_connect *ret_connect);

// fermer la connexion

void usm_close(struct usm_return_connect *usm_type_connect);


#endif