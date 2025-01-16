//DO NOT COMPILE!
#pragma once
void client_thread_worker (void* arg);

typedef struct nodo {
	pthread_t thread;
	struct nodo* next;
} nodo;

typedef struct nodo* thread_list;

typedef struct args{
    int socket;
    nodo* pos;
} args;

void new_client(thread_list l,int socket) {
	nodo* new_elem = (nodo*)malloc(sizeof(nodo));
    new_elem->next = l; //inserimento in testa
	l = new_elem;

    args* ar = (args*)malloc(sizeof(args)); //Con una struttura in stack, pthread passava roba che veniva distrutta subito -> Errori!
    ar->socket = socket; 
    ar->pos = new_elem;

    //void* args[2] = {(void*)new_elem,(void*)socket};
	pthread_create(&(new_elem->thread),NULL,(void*)client_thread_worker,(void*)ar);     //Avvio il nuovo thread
}

void destroy_client(thread_list l, nodo* cli) {
    nodo* prec = NULL; nodo* att = l;

    while (att != NULL && att != cli) {
        prec = att;
        att = att->next;
    }

    if (prec == NULL) {
        //sto eliminando la testa!
    }
    else prec->next = cli->next;
    free(cli);
}