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

#ifndef PERCORSO_H_
#define PERCORSO_H_

// struttura contenente che dovrà contenere tutte le informazioni del file di configurazione
typedef struct c{
	char* UnixPath;
	int MaxConnections;
	int ThreadsInPool;
	int MaxMsgSize;
	int MaxFileSize;
	int MaxHistMsgs;
	char* DirName;
	char* StatFileName;
}ElemFileConfig;


int leggiconfig(char * percorso, ElemFileConfig* configurazione);

#endif
