#include "spiaggia_common.h"

int connect_socketd;

//SIGNAL HANDLER
void sigint_handler(int signum) {
	if (signum == SIGINT) printf("\nRichiesta terminazione da tastiera\n");
    else if (signum == SIGQUIT) printf("\nRicevuto segnale SIGQUIT\n");
        else if (signum == SIGTERM) printf("\nRicevuto segnale SIGTERM\n");
	send(connect_socketd, "exit", 5, 0);
	close(connect_socketd);
	exit(0);
}

void do_client(int argc, char *argv[]) {
	int ombrelloneID;
	bool avail;

	char buffer[BUFF_SIZE];
	bzero(buffer, BUFF_SIZE);

	for (int i = 1; i < argc && i < 4; i++)
		sprintf(buffer, "%s %s", buffer, argv[i]); //buffer = buffer + " " + argv[i];

	if (strcmp(argv[1], "AVAILABLE") == 0) {
		int num = 0;
		send_message(connect_socketd, buffer);
		receive_message(connect_socketd, buffer);
		char *tok = strtok(buffer, " ");
		tok = strtok(NULL, " ");
		//printf("%s\n",tok);
		if (tok != NULL) num = atoi(tok);

		if (argc > 2) //AVAILABLE 1
			printf("Ci sono %d ombrelloni liberi in prima fila\n", num);
		else //AVAILABLE
			printf("Ci sono %d ombrelloni liberi\n", num);
	}

	if (strcmp(argv[1], "CANCEL") == 0) {
		if (argc > 2) {
			send_message(connect_socketd, buffer);
			receive_message(connect_socketd, buffer);
			if (strcmp(buffer, "CANCEL OK") == 0)
				printf("Prenotazione ANNULLATA!\n");
			else
				printf("ERRORE durante la cancellazione della prenotazione\n");
		} else printf("Mi serve il numero dell'ombrellone...\n");
	}


	if (strcmp(argv[1], "BOOK") == 0) { //messaggi che iniziano con BOOK
		send_message(connect_socketd, "BOOK");
		receive_message(connect_socketd, buffer);
		//attendo risposta
		if (strcmp(buffer, "NOK") == 0) { //server non pronto
			printf("Il server e' momentaneamente occupato. Riprova più tardi\n");
			close(connect_socketd);
			exit(-1);
		}
		if (strcmp(buffer, "NAVAILABLE") == 0) { //ombrelloni finiti
			printf("Non ci sono più ombrelloni liberi! Riprova più tardi\n");
			close(connect_socketd);
			exit(-2);
		}

		if (strcmp(buffer, "OK") == 0) {
			//codice interattivo di prenotazione
			avail = false;
			while (!avail) {
				printf("\nChe numero di ombrellone vuoi prenotare? ");
				scanf("%d", &ombrelloneID);
				sprintf(buffer, "BOOK %d", ombrelloneID);
				send_message(connect_socketd, buffer);
				receive_message(connect_socketd, buffer);
				if (strcmp(buffer, "NAVAILABLE") == 0){
					printf("L'ombrellone numero %d non è disponibile al momento\n", ombrelloneID);
				}
				else if (strcmp(buffer, "AVAILABLE") == 0) avail = true;
			}

			//continua la prenotazione
			sprintf(buffer, "BOOK %d ", ombrelloneID);
			int menu = 0;
			while (menu < 1 || menu > 2) {
				printf("\nVuoi prenotare a partire da...\n(1) OGGI (2) PRENOTA NEL FUTURO\n");
				scanf("%d",&menu);
			}

			char datainizio[11]; sprintf(datainizio,"0-0-0");
			char datafine[11]; strcpy(datafine,"0-0-0");
			do {
				sprintf(buffer,"BOOK %d ",ombrelloneID);
				if (menu == 2) { //prenotazione futura	
					printf("\nQuando vuoi iniziare a usare l'ombrellone? (gg-mm-aaaa) : ");
					scanf("%s", datainizio);
					struct data d = str2data(datainizio);
					printf("datainizio: %s\n",datainizio);
					while (!dataValida(d)) {
						printf("Data non valida!\n");
						printf("\nQuando vuoi iniziare a usare l'ombrellone? (gg-mm-aaaa) : ");
						scanf("%s",datainizio);
						d = str2data(datainizio);
					}
					sprintf(buffer, "BOOK %d %s ", ombrelloneID, datainizio);
					
				}
				printf("\nFino a quando vuoi prenotare? (gg-mm-aaaa) : ");
				scanf("%s", datafine);
				struct data d = str2data(datafine);
				while (!dataValida(d)) {
					printf("Data non valida!\n");
					printf("\nFino a quando vuoi prenotare? (gg-mm-aaaa) : ");
					scanf("%s",datafine);
					d = str2data(datafine);
				}
				strcat(buffer, datafine);
			} while (datacmp(str2data(datainizio),str2data(datafine)) == 1);

			//Conferma prenotazione
			printf("--------------------------------\nVuoi confermare la tua prenotazione per l'ombrellone %d?\n", ombrelloneID);
			printf("(1) CONFERMA \t (2) ANNULLA\n: ");
			int user_choice = 0;
			scanf("%d", &user_choice);
			while (user_choice < 1 || user_choice > 2) {
				printf("Scelta \"%d\" non valida.\n", user_choice);
				printf("(1) CONFERMA \t (2) ANNULLA\n: ");
				scanf("%d", &user_choice);
			}
			if (user_choice == 1) { //confermata
				printf("Invio %s \n",buffer);
				send_message(connect_socketd, buffer);
				printf("Prenotazione CONFERMATA!\n");
			}
			else if (user_choice == 2) { //annullata
				send_message(connect_socketd, "CANCEL");
				receive_message(connect_socketd, buffer);
				if (strcmp(buffer, "CANCEL OK") == 0) printf("Prenotazione ANNULLATA!\n");
				else printf("Errore. Il server non è riuscito ad annullare la presentazione\n");
			}
		}
	}

}

int main(int argc, char *argv[]) {

	int port = 5001;
	struct sockaddr_in sa;

	if (argc < 2) { //controllo degli argomenti da linea di comando
		printf("Non si usa cosi'.\nUtilizzo -> %s [MESSAGGIO]\n\n", argv[0]);
		printf("BOOK\t\tinizia a prenotare\n");
		printf("AVAILABLE\tMostra gli ombrelloni liberi\n");
		printf("AVAILABLE 1\tMostra gli ombrelloni liberi in prima fila\n");
		printf("CANCEL [$NUM]\tElimina prenotazione per l'ombrellone $NUM\n");
		printf("EXIT\t\tNon fa niente (termina la connessione)\n");
		exit(-1);
	}

	//Redirezione segnali
	signal(SIGINT, sigint_handler);

	//Preparazione
	connect_socketd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&sa, sizeof(struct sockaddr_in));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(port);

	if (connect(connect_socketd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
		error("ERRORE di connessione", -1);
	else
		printf("Connessione effettuata!\n");

	do_client(argc, argv);
	printf("Chiusura connessione... ");
	close(connect_socketd);
	printf("OK\n");
}
