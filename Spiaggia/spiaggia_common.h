#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>   //standard C
#include <strings.h> //per usare bzero
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>

typedef enum{false,true} bool;

#define N_OMBRELLONI 30 
#define BUFF_SIZE 64

//DEBUG
#define MAX_CONN 2000 //massimo numero di thread worker
#define VERBOSE_PRINT true //attiva/disattiva stampe a video dei messaggi di debug
#define FORCE_MESSAGE_PRINT true //mostra il contenuto dei messaggi scambiati coi socket

typedef struct data {int giorno; int mese; int anno;} data;
typedef struct {
    //int numero;
    enum stato {LIBERO,NONLIBERO,OCCUPATO} stato;
    int fila;
    data datainizio; 
    data datafine;
} prenotazione;

data datazero;

//WRAPPER UTILI
void error(char *message,int code) {
    perror(message);
    exit(code);
}
int send_message(int socket_descriptor,char* message) {
    int retval;
    retval = send(socket_descriptor,message,BUFF_SIZE,0);
    if (FORCE_MESSAGE_PRINT) printf("(%d)> Messaggio inviato: %s\n",socket_descriptor,message);
    return retval;
}
int receive_message(int socket_descriptor,char* message) {
    int retval;
    bzero(message,BUFF_SIZE); //svuotare il buffer evita errori quando qualcuno termina durante una recv()
    retval = recv(socket_descriptor,message,BUFF_SIZE,0);
    if (FORCE_MESSAGE_PRINT) printf("(%d)> Messaggio ricevuto: %s\n",socket_descriptor,message);
    if (strcmp(message,"NOK") == 0) {printf("Il server e' momentaneamente occupato. Riprova più tardi\n"); exit(-2);} //per il client (il server non potrà mai ricevere NOK)
    return retval;
}


//FUNZIONI UTILI
data str2data(char *date) {
    data d = datazero;
    char *gg,*mm,*aa;
	char datestring[20];
	strcpy(datestring,date); //pe evitare corruzzzzzzzione di date
    
    gg = strtok(datestring,"-");
    mm = strtok(NULL,"-");
    aa = strtok(NULL,"-");
    if (aa != NULL && mm != NULL && gg != NULL) {
        d.anno = atoi(aa);
        d.mese = atoi(mm);
        d.giorno = atoi(gg);
    }
    return d;
}

int dataValida(data d) {
    if (VERBOSE_PRINT) printf("%d %d %d\n",d.anno,d.mese,d.giorno);
    //if (d.anno == NULL || d.mese == NULL || d.giorno == NULL) return false;
    if (d.mese < 0 || d.giorno < 0 || d.anno < 0) return false;
    if (d.mese > 12 || d.giorno > 31) return false;

    //mesi di 30 giorni
    if (d.mese == 11 || d.mese == 4 || d.mese == 6 || d.mese == 9)
        if (d.giorno > 30) return false;
    //anni bisestili
    if (d.mese == 2) {//mese di febbraio
        if ((d.anno % 4 == 0 && d.anno % 100 != 0) || d.anno % 400 == 0) { //anno bisestile
            if (d.giorno > 29) return false;
        } 
        else if (d.giorno > 28) return false;
    }
        
    /*
    if (d.mese == 2 && d.giorno > 28)
        if ((d.anno % 4 == 0 && d.anno % 100 != 0) || d.anno % 4 == 0)
            if (d.giorno > 29) return false;
    */

    return true;
}

int datacmp(data d1, data d2) { //-1 se d1<d2, +1 se d1>d2, 0 se d1==d2

    if (d1.anno > d2.anno) return 1;
    if (d1.anno < d2.anno) return -1;

    if (d1.anno ==  d2.anno) {
        if (d1.mese > d2.mese) return 1;
        if (d1.mese < d2.mese) return -1;

        if (d1.mese == d2.mese) {
            if (d1.giorno > d2.giorno) return 1;
            if (d1.giorno < d2.giorno) return -1;

            if (d1.giorno == d2.giorno) return 0;
        }
    }
    return 0;
}
