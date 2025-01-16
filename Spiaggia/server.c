#include "spiaggia_common.h"

/* Questi define sono stati spostati
#define MAX_CONN 1      --->    spiaggia_common.h
#define N_OMBRELLONI 10 --->    spiaggia_common.h
#define DB_PATH         --->    server-gestione-disco.c
*/

int master_socketd; 
int connected = 0;  //numero di client connessi
prenotazione* ombrellone = NULL;
int postiliberi = 0;


pthread_mutex_t client_connection; //usato con condvar client_disconnected per sincronizzare accesso alla struttura dati dei thread 
pthread_mutex_t modifica_ombrelloni; //sincronizza l'accesso alla struttura dati delle prenotazioni
pthread_cond_t client_disconnected; //fa dormire master thread quando ci sono troppi client connessi

#include "threadlist.h"
thread_list workers;
pthread_t master_thread,disk_updater; //devono essere globali per poterli controllare con signal handler

//RESERVATION FUNCTIONS (THREAD-SAFE)
int book(int ID, char *data1, char *data2) {
    data  datainizio,datafine;
    if (VERBOSE_PRINT) printf("PRENOTIAMO PURE! :)\n");
    datainizio = str2data(data1);
    datafine = str2data(data2);
    if (ID >= 0 && ID < N_OMBRELLONI) {
        pthread_mutex_lock(&modifica_ombrelloni);
            if (ombrellone[ID].stato == LIBERO) postiliberi--; //per evitare problemi quando si ripristina un log (il file non può contenere ombrelloni con stato "TEMPORANEAMENTE PRENOTATO")
            ombrellone[ID].stato = OCCUPATO;
            ombrellone[ID].datainizio = datainizio;
            ombrellone[ID].datafine = datafine;                 
        pthread_mutex_unlock(&modifica_ombrelloni);
    }
    return 0;
}
int cancel(int num) {
    if (num >= 0 && num < N_OMBRELLONI) {
        pthread_mutex_lock(&modifica_ombrelloni);
            if (ombrellone[num].stato != LIBERO) postiliberi++;
            ombrellone[num].stato = LIBERO;
            ombrellone[num].datainizio = datazero;
            ombrellone[num].datafine = datazero;
        pthread_mutex_unlock(&modifica_ombrelloni);
        if (VERBOSE_PRINT) printf(" > Prenotazione n.%d cancellata!",num);
    }
    else if (VERBOSE_PRINT) printf("WARNING ombrellone %d non esistente, non faccio niente\n",num);
    return 0;
}


#include "server-gestione-disco.c"

//SIGNAL HANDLER
void sig_handler(int signum) {
    if (signum == SIGINT) {if (VERBOSE_PRINT) printf("\nRichiesta terminazione da tastiera (SIGINT)\n");}
    else if (signum == SIGQUIT) {if (VERBOSE_PRINT) printf("\nRicevuto segnale SIGQUIT\n");}
    else if (signum == SIGTERM) {if (VERBOSE_PRINT) printf("\nRicevuto segnale SIGTERM\n");}

    if (VERBOSE_PRINT) printf("Uccido il master server... ");
    //pthread_kill(master_thread,0); //funziona su macOS ma non su Ubuntu :/
    pthread_cancel(master_thread);
    server_running = false; //rompe il while(go) di disk_updater e master_thread
    close(master_socketd);
    if (VERBOSE_PRINT) printf("OK\n");


    if (VERBOSE_PRINT) printf("Aspetto che il thread disk_updater finisca di aggiornare il disco... ");
    //pending_updates = MAXPENDING; //forza scrittura sul disco
    pthread_cond_signal(&disk_update_required);
    pthread_join(disk_updater,NULL);
    if (VERBOSE_PRINT) printf("OK\n");
    
    exit(0);
}


//THREAD FUNCTIONS
void disk_updater_thread(void);
void client_thread_worker (void* a);
void disk_updater_thread(void);

void master_conn_manager_thread() {
    workers = NULL; //lista di thread worker
    ombrellone = disk_load(); //carica il file degli ombrelloni da disco
    if (VERBOSE_PRINT) printf("Ripristino sessione non salvata da file di log...\n");
    disk_log_merge(); //merge dell'eventuale file di log (sessione terminata male, ripristino)
    pending_updates = 0;

    pthread_create(&disk_updater,NULL,(void*)disk_updater_thread,NULL);

    while(server_running) {
        if (VERBOSE_PRINT) printf("\n(master) In attesa di conessioni...\n");
        listen(master_socketd,5);
        int connect_socketd = accept(master_socketd,NULL,0);
        if (VERBOSE_PRINT) printf("(master) > connect_socketd = %d\n",connect_socketd);
        if (connect_socketd < 0) pthread_exit(NULL); //Impossibile accettare connessioni (quando arriva SIGTERM o il socket ha problemi)
        
        if (connected < MAX_CONN) {
            if (VERBOSE_PRINT) printf("(master) > Connessione accettata!\n");  
            pthread_mutex_lock(&client_connection);
                while (connected >= MAX_CONN) pthread_cond_wait(&client_disconnected,&client_connection); //dormi quando ci sono troppi client connessi
                new_client(workers,connect_socketd);
                connected++;
                //if (VERBOSE_PRINT) printf(" > connected = %d\n",connected);
            pthread_mutex_unlock(&client_connection);
        }
        else {
            if (VERBOSE_PRINT) printf("(master) Troppe connessioni!\n");
            send_message(connect_socketd,"NOK");
            close(connect_socketd);
        }
    }
    if (VERBOSE_PRINT) printf("(master) server_running=false, addio.\n");
    pthread_exit(NULL);
}

void client_thread_worker (void* a) {
    struct args* mydata = (struct args*) a;

    nodo* me = mydata->pos; //posizione del thread nella lista client per poter usare destroy_client
    int connect_socketd = mydata->socket;
    //if (VERBOSE_PRINT) printf("  > usando il socket descriptor %d\n",connect_socketd);

    //ricezione messaggi
    char buffer[BUFF_SIZE];
    int prec_numero = -1; //usato per CANCEL (numero precedente)
    prenotazione *o = ombrellone;
    
    while(server_running) {
        bzero(buffer,BUFF_SIZE);
        if (VERBOSE_PRINT) for (int i=0;i<N_OMBRELLONI;i++) printf("%d %d %d %d-%d-%d %d-%d-%d\n",i,o[i].stato,o[i].fila,o[i].datainizio.anno,o[i].datainizio.mese,o[i].datainizio.giorno,o[i].datafine.anno,o[i].datafine.mese,o[i].datafine.giorno);
        if (VERBOSE_PRINT) printf("(worker %d) >> In attesa di input dal client...\n",connect_socketd);
        receive_message(connect_socketd,buffer);

        //messaggio di TERMINAZIONE CONNESSIONE
        //può darsi che server_running venga cambiato mentre il worker è bloccato sulla receive. In questo caso, il comando ricevuto non deve essere eseguito
        if (server_running == false || strcmp(buffer,"") == 0 || strcmp(buffer,"exit") == 0) {
            //controllo se qualche furbino ha terminato prima di confermare la prenotazione
            pthread_mutex_lock(&modifica_ombrelloni);
                if (prec_numero != -1 && ombrellone[prec_numero].stato == NONLIBERO) {
                    postiliberi++;
                    ombrellone[prec_numero].stato = LIBERO;
                }
            pthread_mutex_unlock(&modifica_ombrelloni);
           
            pthread_mutex_lock(&client_connection);
                connected--;
                destroy_client(workers,me); //ci possono essere errori se master thread e un worker thread modificano la lista in contemporanea
                close(connect_socketd);
                if (VERBOSE_PRINT) printf("(worker %d) > Client disconnesso!\n",connect_socketd);
                pthread_cond_signal(&client_disconnected); //sveglia il master thread o un altro client che vuole disconnettersi
            pthread_mutex_unlock(&client_connection);

            //chiamo thread di aggiornamento disco (torna subito a dormire se non si sono accumulate abbastanza modifiche)
            pthread_cond_signal(&disk_update_required);

            pthread_exit(NULL);
        }



        //PARSING MESSAGGIO
        char *cmd = strtok(buffer," "); //ottieni la prima parola messaggio
        char *arg1 = strtok(NULL," ");
        char *data1 = strtok(NULL," "); //arg2 (data1)
        char *data2 = strtok(NULL," "); //arg3
        
        //Messaggi di BOOK o BOOK _numero_ ecc...
        if (strcmp(cmd,"BOOK") == 0) {
            if (VERBOSE_PRINT) printf(" DEBUG | buffer = %s ; cmd = %s ; arg1 = %s ; data1 = %s ; data2 = %s\n",buffer,cmd,arg1,data1,data2);
            
            if (arg1 == NULL) {// BOOK
                if (updating_disk) send_message(connect_socketd,"NOK");
                else if (postiliberi == 0) send_message(connect_socketd,"NAVAILABLE");
                else send_message(connect_socketd,"OK");
            }

            if (arg1 != NULL && data1 == NULL && data2 == NULL) {// BOOK $NUMERO
                int ID = atoi(arg1); prec_numero = ID; //per fare CANCEL
                if (ID < 0 || ID >= N_OMBRELLONI) send_message(connect_socketd,"NAVAILABLE");
                else {
                    int stato;
                    pthread_mutex_lock(&modifica_ombrelloni);
                        stato = ombrellone[ID].stato;
                        if (ombrellone[ID].stato == LIBERO) {
                            ombrellone[ID].stato = NONLIBERO; //NONLIBERO = temporaneamente occupato
                            postiliberi--;
                            prec_numero = ID; //per poter fare l'annullamento
                        }
                    pthread_mutex_unlock(&modifica_ombrelloni);
                    if (stato == LIBERO) {
                        send_message(connect_socketd,"AVAILABLE");
                        if (VERBOSE_PRINT) printf(" DEBUG | buffer = %s ; cmd = %s ; arg1 = %s ; data1 = %s ; data2 = %s\n",buffer,cmd,arg1,data1,data2);
                    }
                    else { //ho richiesto di prenotare un ombrellone occupato o in prenotazione
                        prec_numero = -1; //se l'ombrellone richiesto era NONLIBERO, bisogna invalidare prec_numero o verrà liberato quando il client termina
                        send_message(connect_socketd,"NAVAILABLE");
                    }
                }
            }
            if (arg1 != NULL && data1!= NULL && data2 == NULL) { //BOOK $NUMERO $DATA ;
                book(atoi(arg1),"0-0-0",data1);
                char logmessage[64];
                sprintf(logmessage,"BOOK %d 0-0-0 %s",atoi(arg1), data1);
                disk_log(logmessage);
            }
            if (arg1 != NULL && data1!= NULL && data2 != NULL){ //BOOK $NUMERO $DATAINIZIO $DATAFINE ;
                book(atoi(arg1),data1,data2);
                char logmessage[64];
                sprintf(logmessage,"BOOK %d %s %s",atoi(arg1), data1, data2);
                disk_log(logmessage);
            }
        }
        //Messaggio di CANCEL $NUMERO
        else if (strcmp(cmd,"CANCEL") == 0) {
            if (updating_disk) send_message(connect_socketd,"NOK");
            else {
                int ID = -1;
                if (arg1 != NULL) { //atoi fa segfault se gli si passa NULL
                        ID = atoi(arg1);
                        char logmessage[64];
                        sprintf(logmessage,"CANCEL %d",ID);
                        disk_log(logmessage);
                }
                else if (prec_numero != -1) { //annulla prenotazione in corso
                        ID = prec_numero;
                        prec_numero = -1;
                        //non segno nel log l'annullamento di un tentativo di prenotazione
                    }
                cancel(ID);
                send_message(connect_socketd,"CANCEL OK");
            }
        }
        //Messaggi di AVAILABLE
        else if (strcmp(cmd,"AVAILABLE") == 0) {
            int count = 0;
            if (arg1 != NULL && atoi(arg1) == 1) { //AVAILABLE 1 (il primo controllo serve ad evitare il seg fault provocato dalla chiamata atoi(NULL))
                int i= 0; 
                //mi aspetto che gli ombrelloni della prima fila abbiano numeri consecutivi, e non messi a caso
                for (i = 0; o[i].fila == 1; i++) if (o[i].stato == LIBERO) count++;
            }
            else count = postiliberi; //AVAILABLE 

            if (count > 0) sprintf(buffer,"AVAILABLE %d",count);
            else sprintf(buffer,"NAVAILABLE");
            send_message(connect_socketd,buffer);
            }

        else {if (VERBOSE_PRINT) printf("WARNING Messaggio non valido: %s\n",arg1); send_message(connect_socketd,"INVALIDMESSAGE");}
        
    }

}


int main() {
    int port = 5001;
	struct sockaddr_in sa;

    //redirezione segnali
    signal(SIGINT,sig_handler);
    signal(SIGQUIT,sig_handler);
    signal(SIGTERM,sig_handler);

    //inizializzo robe pthread
    //creazione dei SEMAFORI MUTEX (default)
    pthread_mutex_init(&client_connection,NULL);
    pthread_mutex_init(&modifica_ombrelloni,NULL);
    pthread_mutex_init(&updatecheckmutex,NULL);
    //creazione delle CONDITION VARIABLES (default)
    pthread_cond_init(&client_disconnected,NULL);
    pthread_cond_init(&disk_update_required,NULL);
    

    //disk_new_spiaggia(); //crea una nuova spiaggia (elimina quella esistente)
	master_socketd = socket(AF_INET,SOCK_STREAM,0);

	bzero(&sa,sizeof(struct sockaddr_in));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(port);

	if (bind(master_socketd,(struct sockaddr*)&sa,sizeof(sa))<0) error("ERRORE di bind",-1);
	printf("Socket aperto sulla porta %d\n",port);
    
    datazero = str2data("0-0-0");

	pthread_create(&master_thread,NULL,(void*)master_conn_manager_thread,NULL);

    pthread_join(master_thread,NULL);
    pthread_join(disk_updater,NULL);
}
