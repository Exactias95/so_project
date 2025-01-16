#pragma once
// NON COMPILARMI!

#include "spiaggia_common.h"

#define DBPATH "spiaggiadb.txt"
#define LOGPATH "sessionlog.txt"
#define MAXPENDING 5

bool updating_disk = false; //per controllare se update thread sta aggiornando il disco o no
int pending_updates = 0;
bool server_running = true;

pthread_mutex_t updatecheckmutex; //sincronizza acecsso alla var. pending_updates
pthread_cond_t disk_update_required; //dormi-sveglia disk_updater

//GESTIONE INFO SU DISCO (FILE DI TESTO)
void disk_log(char *operation) { //aggiunge operazione al file di log
    pthread_mutex_lock(&updatecheckmutex); //aspetto che disk_updater finisca. Altrimenti c'è il rischio di scrivere il log dopo il salvataggio ma prima che venga cancellato -> Errori!
        while (updating_disk) pthread_cond_wait(&disk_update_required,&updatecheckmutex);
        FILE* logfile = fopen(LOGPATH,"a"); //apro il file solo quando è sicuro farlo
        fprintf(logfile,"%s\n",operation);
        fclose(logfile);
        pending_updates++;
    pthread_mutex_unlock(&updatecheckmutex);
}

void disk_log_merge(void) { //carica file di log e scrive cambiamenti in memoria
    //NB. niente mutex perchè viene eseguita una volta sola quando ancora non c'è nessuna concorrenza
    //Non può modificare il disco perchè le informazioni sono in un file di testo e ogni riga ha grandezza variabile

    FILE *logfile = fopen(LOGPATH,"rt");

    if (logfile != NULL) {
        char cmd[11],numstr[11],data1[11],data2[11];
        while (!feof(logfile)) {
            fscanf(logfile,"%s",cmd);
            fscanf(logfile,"%s",numstr);
            if (VERBOSE_PRINT) printf("cmd = %s, numstr = %s\n",cmd,numstr);
            int num = atoi(numstr);
            if (strcmp(cmd,"BOOK") == 0) {
                if (VERBOSE_PRINT) printf("Richiesta di book\n");
                if (VERBOSE_PRINT) fprintf(stdout,"%d %d %d %s %s\n",num,OCCUPATO,ombrellone[num].fila,data1,data2);
                fscanf(logfile,"%s",data1);
                fscanf(logfile,"%s",data2);
                book(num,data1,data2);

            }
            else if (strcmp(cmd,"CANCEL") == 0) {
                if (VERBOSE_PRINT) printf("Richiesta di cancel\n");
                cancel(num);
                pending_updates--;
            }
            if (VERBOSE_PRINT) printf("OK\n");
        }
        fclose(logfile);
        char deletelog[128];
        sprintf(deletelog,"rm \"%s\" 2> /dev/null",LOGPATH);
        system(deletelog);
    }
    else if (VERBOSE_PRINT) printf("non trovo file di log, niente da aggiornare... \n");
    pending_updates = 0; //giusto per essere sicuri
}

void disk_write(prenotazione *o, int n) { //scrive n ombrelloni sul disco
    //o = array di ombrelloni
    //n = numero di elementi nell'array che verranno scritti sul file
    FILE *spiaggiadb = fopen(DBPATH,"wt");
    if (spiaggiadb == NULL) error("ERRORE di accesso al disco",-10);
         
    char datainizio[11],datafine[11];
           
    if (VERBOSE_PRINT) printf("Salvo i dati sul disco...\n");
    for (int i=0; i < n; i++) {
        if (o[i].stato != OCCUPATO) o[i].stato = LIBERO; //annulla le prenotazioni non completate
        sprintf(datainizio,"%d-%d-%d",o[i].datainizio.anno,o[i].datainizio.mese,o[i].datainizio.giorno);
        sprintf(datafine,"%d-%d-%d",o[i].datafine.anno,o[i].datafine.mese,o[i].datafine.giorno);
        fprintf(spiaggiadb,"%d %d %d %s %s\n",i,o[i].stato,o[i].fila,datainizio,datafine);
    }
    fclose(spiaggiadb);
    if (VERBOSE_PRINT) printf("Salvataggio completato!\n");
}

void disk_new_spiaggia() { //inizializza nuova spiaggia di N_OMBRELLONI
    prenotazione *o = (prenotazione*)malloc(N_OMBRELLONI*sizeof(prenotazione));
    for (int i=0; i < N_OMBRELLONI; i++) {
        o[i].stato = LIBERO;
        o[i].fila = i / (N_OMBRELLONI/5) + 1;
        o[i].datainizio.giorno = 0;
        o[i].datainizio.mese = 0;
        o[i].datainizio.anno = 0;
        
        o[i].datafine.giorno = 0;
        o[i].datafine.mese = 0;
        o[i].datafine.anno = 0;
    }
    disk_write(o,N_OMBRELLONI);
    free(o);
}

prenotazione* disk_load() { //carica spiaggia da disco a memoria
    FILE *spiaggiadb = fopen(DBPATH,"rt");
    if (spiaggiadb == NULL) {error("ERRORE di accesso al disco",-10);}
    else if (VERBOSE_PRINT) printf("File aperto, inizio il caricamento dei dati...\n");
    
    prenotazione *o = (prenotazione*)malloc(N_OMBRELLONI*sizeof(prenotazione));
    int i;
    for (i=0; !feof(spiaggiadb) && i < N_OMBRELLONI; i++)  {
        int num,stato,fila,gg,mm,aa;

        fscanf(spiaggiadb, "%d %d %d",&num,&stato,&fila);
        o[i].fila = fila;
        o[i].stato = stato;
        if (stato == LIBERO) postiliberi++;
        fscanf(spiaggiadb, "%d-%d-%d",&aa,&mm,&gg);
        o[i].datainizio.anno = aa;
        o[i].datainizio.mese = mm;
        o[i].datainizio.giorno = gg;
        fscanf(spiaggiadb, "%d-%d-%d",&aa,&mm,&gg);
        o[i].datafine.anno = aa;
        o[i].datafine.mese = mm;
        o[i].datafine.giorno = gg;

        if (VERBOSE_PRINT) printf("%d %d %d %d-%d-%d %d-%d-%d\n",i,o[i].stato,o[i].fila,o[i].datainizio.anno,o[i].datainizio.mese,o[i].datainizio.giorno,o[i].datafine.anno,o[i].datafine.mese,o[i].datafine.giorno);
    }
    fclose(spiaggiadb);
    if (VERBOSE_PRINT) printf("Caricati %d elementi. %d posti liberi\n",i,postiliberi);
    return o;
}

//FUNZIONI THREAD
void disk_updater_thread(void) {
    while(server_running) {
        if (VERBOSE_PRINT) printf("(disk_updater) Cerco richieste di scrittura...\n");
        sleep(2);
        if (VERBOSE_PRINT) printf("(disk_updater) Nessuna richiesta di scrittura, vado a nanna...\n");
        pthread_mutex_lock(&updatecheckmutex);
            while(pending_updates < MAXPENDING && server_running) {
                if (VERBOSE_PRINT) printf("(disk_updater) pending updates %d/%d\n",pending_updates,MAXPENDING);
                pthread_cond_wait(&disk_update_required,&updatecheckmutex);
            }
            if (VERBOSE_PRINT) printf("(disk_updater) pending updates %d/%d; server_running = %s. \nOk sono sveglio, scrivo i cambiamenti sul disco. \nBlocco i thread worker...\n",pending_updates,MAXPENDING,(server_running == true)? "true" : "false");
            updating_disk = true;
        pthread_mutex_unlock(&updatecheckmutex);
        
        disk_write(ombrellone,N_OMBRELLONI);
        char deletelog[128];
        sprintf(deletelog,"rm \"%s\" 2> /dev/null",LOGPATH);
        //sleep(7); //simulo overhead scrittura
        system(deletelog);
        pending_updates = 0;
        
        pthread_mutex_lock(&updatecheckmutex);
            if (server_running) updating_disk = false; //non sblocco i thread worker se ho ricevuto un SIGQUALCOSA
            pthread_cond_signal(&disk_update_required); //sveglio client o logger in attesa
        pthread_mutex_unlock(&updatecheckmutex);
    }
    pthread_exit(NULL);
}