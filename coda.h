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
 * @brief Definizione delle funzioni per gestire la coda circolare (per i fd)
 */



#ifndef CODA_H_
#define CODA_H_

#include <pthread.h>
#include "config.h"

typedef struct elem{
	int *array;
	int dimensione;
	int testa;
	int coda;
	int lunghezza;
}Scoda;

//----------------------FUNZIONI CODA-----------------------------

/*
Funzione utilizzata inserire un file descriptor in coda all'array circolare
parametri:
	head è il puntatore della coda circolare
	fd è file descriptor
restituisce:
	-1 se l'operazione non è andata a buon fine
	 1 se l'operazione è andata a buon fine
*/
int inseriscicoda(Scoda* head, int fd);

/*
Funzione utilizzata per estrarre un file descriptor dalla coda circolare
parametri:
	head è il puntatore della coda circolare
restituisce:
	-1 se l'operazione non è andata a buon fine
	il file descriptor se l'operazione è andata a buon fine
*/
int estraitesta(Scoda* head);

/*
Funzione utilizzata per cancellare la coda circolare
parametri:
	head è il puntatore alla coda circolare
restituisce:
*/
void cancellacoda(Scoda* head);



#endif
