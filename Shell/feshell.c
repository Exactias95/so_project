/*SHELL ls*/
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#define TRUE 1
#define FALSE 0
#define MAX 10

/*typedef struct _myfile
{
  int permessi[3];
  int minuti;
  int ora;
  int giorno;
  int mese;
  //int anno;
  char *proprietario;
  char* gruppo;
  char* nome_file;
  int nascosto;
  struct _myfile *prev;
  struct _myfile *next;
}myfile;
*/

int visibili;
int tutti_file;

void ls(char* c[MAX]) 
{
  DIR *p;
  struct dirent *d, *t;
  int count=0,i;
  p=opendir(".");
  if (p==NULL)
  {
    printf("directory non trovata\n");
  }
  else
  {
    while(d=readdir(p)) //leggo directory successiva
    {
      //se inizia col punto non stampare
      if (d->d_name[0]!='.')
      {
        c[count]=d->d_name;
        //printf(" %s[%d]\n", d->d_name, count);
        count++;
      }
    }
  }

}



void ls_a(char* c[MAX]) 
{
  DIR *p;
  struct dirent *d, *t;
  int count=0,i;
  p=opendir(".");
  if (p==NULL)
  {
    printf("directory non trovata\n");
  }
  else 
  {
    while(d=readdir(p)) //leggo directory successiva
    {
      c[count]=d->d_name;
      //printf(" %s[%d]\n", d->d_name, count);
      count++;
    }   
  }
}


void ls_l(char *c[MAX], char *tmp[MAX], int *permessi[3][MAX], int *dim[MAX])
{
  int ret,i;

  for (i=0;i<MAX;i++)
  {
    permessi[0][i]=0;
    permessi[1][i]=0;
    permessi[2][i]=0;
  }

  struct stat buf;
  for (i=0;i<MAX;i++)
  {
    if (ret= stat(c[i], &buf)<0)
      printf("");
    //printf(" %s | %d\n", c[i],ret);
    char date[MAX];
    //printf( "File size : %ld\n", buf.st_size );                       /* DIMENSIONE */
    strftime(date, MAX, "%d-%m-%y", gmtime(&(buf.st_ctime)));          /* DATA */
    tmp=date;
    dim[i]=buf.st_size;
    //printf("Il file %s è stato modificato il %s\n\n", c[i], date);
    date[0] = 0;


    if (ret=access(c[i], R_OK)==0)
      permessi[0][i]=TRUE;
    else
      permessi[0][i]=FALSE;

    if (ret=access(c[i], W_OK)==0)
      permessi[1][i]=TRUE;
    else
      permessi[1][i]=FALSE;

    if (ret=access(c[i], X_OK)==0)
      permessi[2][i]=TRUE;
    else
      permessi[2][i]=FALSE;
  }
}

void conta_file()
{
  DIR *p;
  struct dirent *d;
  p=opendir(".");
  if (p==NULL)
  {
    printf("directory non trovata\n");
  }
  else
  {
    while(d=readdir(p)) //leggo directory successiva
    {
      tutti_file++;
      if (d->d_name[0]!='.')
      {
        visibili++;
      }
    }
  }
  printf("file visibili %d, tutti i file %d\n",visibili,tutti_file);
}


void ls_t(char *nome[MAX], int presenza[3])
{
  int sauro[MAX];
  char data1[MAX],data2[MAX], *tmp;
  struct stat buf;
  int i=0,n,j;
  if(presenza[0]==1)
    n=tutti_file;
  else
    n=visibili;


  //Bubblesort
  for (j=0;j<n;j++)
  {

    for (i=0;i<n-1;i++)
    {
      stat(nome[i],&buf);
      strftime(data1, MAX, "%y%m%d", gmtime(&(buf.st_ctime)));
      stat(nome[i+1],&buf);
      strftime(data2, MAX, "%y%m%d", gmtime(&(buf.st_ctime)));
      if (strcmp(data1,data2)>0)
      {
        tmp=nome[i+1];
        nome[i+1]=nome[i];
        nome[i]=tmp;
      }  
    }
  }
}

void stampa(int presenza[3], char *nome[MAX], int *dim[MAX], char *data[MAX], int *permessi[3][MAX])
{
  int n=0;
  if(presenza[0]==1)
    n=tutti_file;
  else
    n=visibili;
  
  struct stat buf;
  char prova[MAX];
  if (presenza[2]==FALSE) //stampa solo i nomi
  {
    int i;
    for (i=0;i<n;i++)
    {
      printf("%s\n",nome[i]);
    }
  }
  else   // stampa tutti i formati
  {
    int i;
    for (i=0;i<n;i++)
    {
      printf("Nome\t %s\n",nome[i]);
      printf("Dim.\t %d\n",dim[i]);
      stat(nome[i],&buf);
      strftime(prova, MAX, "%d-%m-%y", gmtime(&(buf.st_ctime)));
      printf("Data\t %s\n",prova);
      printf("R %d\t W %d\t X %d\n\n",permessi[0][i],permessi[1][i],permessi[2][i]);
    }
  }
}

int main(void)
{

  conta_file();
  char *arg_list[10];
  int status;
  int n_parametri = 0;
  int counter2 = 0;
  pid_t pid;
  char buf[100];
  char inFile[10];
  char outFile[10];
  int presenza[3]={0,0,0};
  int visibili, tutti_file;
  char *carattere[MAX];
  char *date[MAX];
  int *permessi[3][MAX];
  int *Dimensione[MAX];
   while(TRUE)
   {
      printf(">");  

      if (!fgets(buf, 100, stdin))
      return 0;

      pid = fork();
      switch(pid)
      {
        case -1:
          fprintf(stderr, "dead father\n");
          return 1;

        case 0:
          arg_list[n_parametri] = strtok(buf, " \n");
          n_parametri = 0;

          /* TOKENIZZO LA LISTA */
          while(arg_list[n_parametri] != NULL)
          {
           n_parametri++;
           arg_list[n_parametri] = strtok(NULL, " \n");
          }

          /* CASI DI LS */

          int i;
   
          if (strcmp(arg_list[0], "ls")==0)
          {
            //individuo i parametri emessi in input
            for (i=1;i<n_parametri;i++)
            {
              if (strcmp(arg_list[i], "-a")==0)
                presenza[0]=TRUE;
              else if (strcmp(arg_list[i], "-t")==0)
                presenza[1]=TRUE;
              else if (strcmp(arg_list[i], "-l")==0)
                presenza[2]=TRUE;
              else if (strcmp(arg_list[i], "|")==0)
                execvp(arg_list[0],arg_list);
              else if (strcmp(arg_list[i],">")==0 || strcmp(arg_list[i],"<")==0)
                execvp(arg_list[0],arg_list);
              else
                printf("Comando non riconosciuto\n");
            }


            /* ESECUZIONE DI LS */
            ls(carattere);

            /* Parametro -a */
            if (presenza[0]==TRUE)
              ls_a(carattere);
            /* Parametro -t */
            if (presenza[1]==TRUE)
              ls_t(carattere,presenza);
            /* Parametro -l */
            if (presenza[2]==TRUE)
              ls_l(carattere,date,permessi,Dimensione);
          }
          //se non è ls
          else
          {
              execvp(arg_list[0], arg_list);
              break;
          }
          stampa(presenza, carattere, Dimensione, date, permessi);

      }
    }
    return 0;
}
 
