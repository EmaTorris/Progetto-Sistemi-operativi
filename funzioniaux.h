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

#ifndef FUNZIONIAUX_H_
#define FUNZIONIAUX_H_
#include <stdio.h>
#include <errno.h>


/* Mylock è una macro che permette di fare una lock su una pthread_mutex_lock e in caso di errore esco dal programma stampando
sullo stderr il messaggio di errore */


#define MyLock(varmutex) \
	if(pthread_mutex_lock(&varmutex) != 0){ \
		fprintf(stderr, "Nel seguente file %s alla riga %d c'è un problema con la lock\n",__FILE__, __LINE__);	\
		exit(EXIT_FAILURE);	\
	}

/* FError è una macro che stampa un messaggio di errore uscendo dal programma */

#define FError(condizione, messaggiodierrore) \
	if(condizione) { \
		fprintf(stderr, "Errore: "#messaggiodierrore"; Nel seguente file %s alla seguente linea %d\n", __FILE__, __LINE__ ); \
		fflush(stderr); \
		exit(EXIT_FAILURE); \
	}

/* Mylock è una macro che permette di fare una unlock su una pthread_mutex_lock e in caso di errore esco dal programma stampando
sullo stderr il messaggio di errore */

#define MyUnlock(varmutex) \
	if(pthread_mutex_unlock(&varmutex) != 0){ \
		fprintf(stderr, "Nel seguente file %s alla riga %d c'è un problema con la lock\n",__FILE__, __LINE__);	\
		exit(EXIT_FAILURE);	\
	}

#endif
