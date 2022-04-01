/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 *
 *  Autore: Emanuele Torrisi
 *  Si dichiara che il contenuto di questo file è in ogni sua parte opera
 *  originale dell'autore
 */

#ifndef PERCORSO_C_
#define PERCORSO_C_

#include "percorso.h"
#include "funzioniaux.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define Nmassimoelementi 512

//tale funzione ha il compito di leggere il file di configurazione e salvarlo in una apposita struttura
int leggiconfig(char * percorso, ElemFileConfig* configurazione) {

	//provo ad aprire il file controllando se l'argomento sia valido e se esista
	FILE* fileconfig = fopen(percorso, "r");
	FError(fileconfig == NULL || errno == EINVAL, "apertura del file di configurazione");
	char linealetta[Nmassimoelementi];

	char aux[Nmassimoelementi] ;
	char aux1[Nmassimoelementi];
	//array che mi serve per capire se ho copiato tutti ed 8 i dati significativi del file di configurazione
	int ArrSupp[8] = {-1 , -1 , -1, -1 , -1 , -1 , -1 , -1};

	//finchè c'è qualcosa da leggere
	while(fgets(linealetta,Nmassimoelementi,fileconfig) != NULL){

		char lett = linealetta[0];
    memset(&aux[0], 0, sizeof(aux));
    memset(&aux1[0],0, sizeof(aux1));
		//escludo tutti i commenti
		if (lett != '#'){
      int x = 0;
		  int y = 0;
      int z = 0;

			// aux conterrà tutto quello che precede il simbolo "="
			// aux1 conterrà tutto quello che segue il simbolo "="
		 	while(lett != '\0' && lett != '\n' && lett != EOF && lett != '='){
					lett = linealetta[x];
					aux[y] = lett;
					x++;
					y++;
				}
      	x = x+1;
      	while(lett != '\0' && lett != '\n' && lett != EOF){
          	lett = linealetta[x];
          	aux1[z] = lett;
          	z++;
          	x++;
      	}
      	aux1[z-1] = '\0'; // per non salvare il carattere speciale '\n'
      	if(strncmp(aux,"UnixPath",8) == 0){
           	configurazione->UnixPath = malloc((strlen(aux1)+1) * sizeof(char));
           	strncpy(configurazione->UnixPath,aux1,strlen(aux1)+1);
           	ArrSupp[0] = 1;
           	continue;
				}

				else if (strncmp(aux,"DirName",7) == 0){
            	configurazione->DirName = malloc((strlen(aux1)+1)* sizeof(char));
            	strncpy(configurazione->DirName,aux1,strlen(aux1)+1);
            	ArrSupp[1] = 1;
            	continue;
				}
				else if (strncmp(aux,"StatFileName",12) == 0){
            	configurazione->StatFileName = malloc((strlen(aux1)+1)* sizeof(char));
            	strncpy(configurazione->StatFileName,aux1,strlen(aux1)+1);
            	ArrSupp[2] = 1;
            	continue;
				}
      	else if (strncmp(aux,"MaxConnection",13) == 0){
            	configurazione->MaxConnections = atoi(aux1);
            	ArrSupp[3] = 1;
            	continue;
      	}
      	else if (strncmp(aux,"ThreadsInPool",13) == 0){
            	configurazione->ThreadsInPool = atoi(aux1);
            	ArrSupp[4] = 1;
            	continue;
      	}
     		else if (strncmp(aux,"MaxMsgSize",10) == 0){
            	configurazione->MaxMsgSize = atoi(aux1);
            	ArrSupp[5] = 1;
            	continue;
      	}
      	else if (strncmp(aux,"MaxFileSize",11) == 0){
            	configurazione->MaxFileSize = atoi(aux1);
            	ArrSupp[6] = 1;
            	continue;
      	}
      	else if (strncmp(aux,"MaxHistMsgs",11) == 0){
            	configurazione->MaxHistMsgs = atoi(aux1);
            	ArrSupp[7] = 1;
            	continue;
      	}
			}
	}
  int i = 0;
  int trovato = 0;
  while( !trovato && i < 8 ) {
  if(ArrSupp[i] == -1) trovato = 1;
  	i++;
  }
	fclose(fileconfig);
  if(trovato) return -1; // errore almeno un dato non è stato letto
  else return 1; // è andato tutto ok
}
#endif
