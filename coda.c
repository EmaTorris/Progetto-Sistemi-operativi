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
/**
 * @file coda.c
 * @brief Implementazioni delle funzioni per gestire la coda circolare (per i fd)
 */

#ifndef CODA_C_
#define CODA_C_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "coda.h"
#include "funzioniaux.h"

/*
Funzione utilizzata inserire un file descriptor in coda all'array circolare
parametri:
	head è il puntatore della coda circolare
	fd è file descriptor
restituisce:
	-1 se l'operazione non è andata a buon fine
	 1 se l'operazione è andata a buon fine
*/
int inseriscicoda(Scoda* coda, int fd){
	int esito = -1;
	if (coda == NULL){
		fprintf(stderr,"errore la coda è vuota\n");
		exit(EXIT_FAILURE);
	}
	else{
		//se la coda non ha raggiunto il massimo numero di elementi
		if( coda->testa != (coda->lunghezza + 1) % (coda->dimensione)){
			coda->array[coda->lunghezza] = fd;
			coda->lunghezza = (coda->lunghezza + 1) % coda->dimensione;
			esito = 1;
		}
	}
	return esito;
}

/*
Funzione utilizzata per estrarre un file descriptor dalla testa della coda circolare
parametri:
	head è il puntatore della coda circolare
restituisce:
	-1 se l'operazione non è andata a buon fine
	il file descriptor se l'operazione è andata a buon fine
*/
int estraitesta(Scoda* coda){
	int estrai = -1;
	if (coda == NULL){
		fprintf(stderr,"errore coda piena\n");
		exit(EXIT_FAILURE);
	}
	else{
		//estraggo il primo elemento ed aggiorno il puntatore del prossimo elemento da estrarre
		estrai = coda->array[coda->testa];
		coda->testa = (((coda->testa)+1) % coda->dimensione);
	}
	return estrai;
}

/*
Funzione utilizzata per cancellare la coda circolare
parametri:
	head è il puntatore alla coda circolare
restituisce:
*/
void cancellacoda(Scoda* head){
	if(head == NULL) return ;
	free(head->array);
		return;
}


#endif
