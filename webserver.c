#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <malloc.h>
#include <fcntl.h>  
#include <sys/mman.h>      
#include <sys/stat.h> 
//#include <time.h>
#include <semaphore.h>
#define BUFSIZE 1024
#define MAXERS 16

#define OK_IMAGE    "HTTP/1.0 200 OK\nContent-Type:image/gif\n\n"
#define OK_TEXT     "HTTP/1.0 200 OK\nContent-Type:text/html\n\n"
#define NOTOK_404   "HTTP/1.0 404 Not Found\nContent-Type:text/html\n\n"
#define MESS_404    "<html><body><h1>ERROR 404, FILE NOT FOUND</h1></body></html>"

char *ROOT;
extern char **environ;

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t master_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;

struct richiesta
{
	int acceptfd;
	int size;
	char file_name[1024];
	int cli_ipaddr;
	char in_buf[2048];
	//struct richiesta *next;

};

void *datasend(void*conn)
{

	char out_buf[BUFSIZE];
	char *file_name;
	int acceptfd=*(int*)conn;                 
	//struct richiesta p;
	//p.acceptfd = acceptfd;


	//memset(out_buf, 0, sizeof(out_buf));

	pthread_mutex_unlock(&mutex);
	int re;
	//if (re=	recv(acceptfd, out_buf, BUFSIZE, 0)<0)
	//	printf("errore della receive\n");

	pthread_cond_wait(&cond_var, &mutex);
	pthread_mutex_lock(&mutex);



	pthread_cond_signal(&cond_var);


/*
	if (fd1 == -1)
    {
    	/* ERRORE 404 */
/*
		printf("Errore lettura del file %s \n", &file_name[1]);
		strcpy(out_buf, NOTOK_404);
		send(acceptfd, out_buf, strlen(out_buf), 0);
        strcpy(out_buf, MESS_404);
		send(acceptfd, out_buf, strlen(out_buf), 0);
	}
    else
    {
        printf("Il file %s e' stato inviato \n", &file_name[1]);
        if ((strstr(file_name, ".jpg") != NULL)||(strstr(file_name, ".gif") != NULL)) 
        { 
        	strcpy(out_buf, OK_IMAGE); 
        }

		else
        { 
        	strcpy(out_buf, OK_TEXT); 
        }
      	send(acceptfd, out_buf, strlen(out_buf), 0);
    }
*/

	//p.size= dimensione_file;
	//p.file_name= nome_file;
	//mutex unlock
	

	//signal
}

int main(int argc, char *args[])
{
	int port	=	8080;
	int threadnum = 1;
	int time_flag =	0;
	int sleep_time=	0;
	int sched_flag=	0;
	int connfd, acceptfd;
	int i;
	int is_static;
	char in_buffer[BUFSIZE];
	char out_buffer[BUFSIZE];
	char filename[BUFSIZE];
	char filetype[BUFSIZE];
	char cgiargs[BUFSIZE];
	char uri[BUFSIZE];
	char method[BUFSIZE];
	char version[BUFSIZE];
	char *p;
	struct sockaddr_in serveraddr, clientaddr;
	struct stat sbuf;
	pthread_t thread_id;


	for(i=0;i<argc;i++)
	{
		if(strcmp(args[i],"-n")==0)
		{
			threadnum=atoi(args[i+1]);
		}
		else if(strcmp(args[i],"-p")==0)
		{
			port=atoi(args[i+1]);
		}
		else if(strcmp(args[i],"-t")==0)
		{
			time_flag=1;		
		 	sleep_time=atoi(args[i+1]);
		}
		else if(strcmp(args[i],"-s")==0)
		{
			if (strcmp(args[i+1],"FCFS")==0)		
				sched_flag=0;
			else if(args[i+1],"SJF")
				sched_flag=1;
			else
				printf("Please enter a proper scheduling algorithm");
		}
	
	}


	/* Creazione Threads */
/*
	for (i=0;i<threadnum;i++)
		if(pthread_create(&thread_id, NULL, &datasend, (void*)&acceptfd)<0)
			printf("errore nella creazione del thread\n");
*/
	/* Connessione */
	connfd = socket(AF_INET, SOCK_STREAM,0);

	if (connfd<0)
		printf("errore di apertura socket\n");
	else
		printf("socket aperto\n");
	bzero((char*)&serveraddr,sizeof(serveraddr));

	serveraddr.sin_family = 	AF_INET;
	serveraddr.sin_addr.s_addr=	INADDR_ANY;
	serveraddr.sin_port = 		htons(port);

	if (bind(connfd, (struct sockaddr*)&serveraddr,sizeof(serveraddr))<0)
		printf("errore nella bind\n");
	else
		printf("bind effettuata\n");

	if (listen(connfd,5)<0)
		printf("errore nella listen\n");
	else
		printf("listen effettuata\n");

	int clientlen=sizeof(clientaddr);
	
	while (1)
	{
		//pthread_cond_wait(&cond_var, &master_mutex);
		acceptfd = accept(connfd, (struct sockaddr*)&clientaddr, &clientlen);
		if (acceptfd<0)
			printf("errore nell'accept\n");
		else
			printf("accept effettuata\n");

		//read(acceptfd, in_buffer, BUFSIZE);
		FILE *stream;
		if ((stream = fdopen(acceptfd, "r+")) == NULL)
      		error("ERROR on fdopen");
      	else
      		printf("stream aperto\n");


	    /* get the HTTP request line */
	    fgets(in_buffer, BUFSIZE, stream);
	    printf("%s", in_buffer);
	    sscanf(in_buffer, "%s %s %s\n", method, uri, version);

	    /* read (and ignore) the HTTP headers */
	    fgets(in_buffer, BUFSIZE, stream);
	    printf("%s", in_buffer);
	    while(strcmp(in_buffer, "\r\n")) {
	      fgets(in_buffer, BUFSIZE, stream);
	      printf("%s", in_buffer);
	    }

/* CODICE UFO */
 
	    if (!strstr(uri, "cgi-bin")) 
	    {
	      is_static = 1;
	      strcpy(cgiargs, "");
	      strcpy(filename, ".");
	      strcat(filename, uri);
	      if (uri[strlen(uri)-1] == '/') 
			strcat(filename, "diocaro.html");
	    }
	    else 
	    {
	      is_static = 0;
	      p = index(uri, '?');
	      if (p) 
	      {
		     strcpy(cgiargs, p+1);
		     *p = '\0';
	      }
	      else 
	      {
		     strcpy(cgiargs, "");
	      }
	      strcpy(filename, ".");
	      strcat(filename, uri);
	    }
    /* FINE CODICE ANDREA */

	    /* ERROR 404 */
	    if (stat(filename, &sbuf) < 0) {
	      fprintf(stream, "%s\n",NOTOK_404 );
	      //cerror(stream, filename, "404", "Not found", "Tiny couldn't find this file");
	      fclose(stream);
	      close(acceptfd);
	      continue;
	    }

	    /* serve static content */
	    if (is_static) 
	    {
	      if (strstr(filename, ".html"))
		     strcpy(filetype, "text/html");
	      else if (strstr(filename, ".gif"))
		     strcpy(filetype, "image/gif");
	      else if (strstr(filename, ".jpg"))
		     strcpy(filetype, "image/jpg");
	      else 
		     strcpy(filetype, "text/plain");
		 }

		 /* INVIA TESTO */
        sprintf(out_buffer, OK_TEXT);
      	write(acceptfd, out_buffer, BUFSIZE);
        sprintf(out_buffer, "<html> <body> hello world </body> </html>");
      	write(acceptfd, out_buffer, BUFSIZE);
		close(acceptfd);
		
		/* CODICE UFO */
		close(0);
		dup2(acceptfd, 1); /* map socket to stdout */
		dup2(acceptfd, 2); /* map socket to stderr */
		printf("\nfilename%s\n",filename );
		if (execve(filename, NULL, environ) < 0) {
	  		perror("ERROR in execve");
		}
		/*FINE CODICE ANDREA */


		//pthread_cond_signal(&cond_var);
		printf("thread chiuso\n");
		//coda dei processi (SJF o FCFS)
		
	}
}