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
 * @file hash.h
 * @brief Definizione delle funzioni e delle strutture dati dedicate alla memorizzazione degli utenti
 */

#ifndef	HASH_H_
#define HASH_H_

#include <string.h>
#include "coda.h"
#include "config.h"
#include "message.h"

typedef enum { FALSE, TRUE } bool;

//struttura utilizzata per la lista dei messaggi degli utenti
typedef struct message{
	message_t *messaggio;
	struct message* next;
}ListaDiMessaggi;

//struttura utilizzata per la tabella hash degli utenti
typedef struct hash{
	char nome[MAX_NAME_LENGTH+1];//nome di un utente
	bool online;//variabile booleana utilizzata per verificare se un utente è online o no
	int fd;//file descriptor con cui si è registrato l'utente
	ListaDiMessaggi* MessLista; //lista di messaggi nel caso in cui l'utente risulti offline
	struct hash * next;
}T_user_hash;


//	QUESTA STRUTTURA NON È STATA UTILIZZATA PER IL PROGETTO

typedef struct memgroup{
	char membro[MAX_NAME_LENGTH+1];
	struct memgroup* next;
}membrogruppo;

// QUESTA STRUTTURA NON È STATA UTILIZZATA PER IL PROGETTO

typedef struct grouphash{
	char nomeg[MAX_NAME_LENGTH+1];
	membrogruppo** membri;
	int dimensione;
	struct grouphash* next;
}T_hash_group;


/*---------------------FUNZIONI UTENTI HASH---------------------------*/
/*
Funzione utilizzata per calcolare l'hash
parametri:
	stringa è il nome dell'utente
	n è il numero massimo di elementi della tabella
restituisce:
	l'indice della posizione della tabella hash nella quale accedere
*/
int FunzioneHash(char* stringa, int n);

/*
Funzione utilizzata per inserire un utente nella tabella hash
parametri:
	head è la lista di trabocco della tabella hash
	nome è il nome dell'utente che si vuole registrare
	fd è il file descriptor con cui si è connesso
restituisce:
	NULL se fallisce
	il puntatore al nuovo utente creato della tabella hash
*/
T_user_hash* InserisciTestaHash(T_user_hash* head, char* nome, int fd);

/*
Funzione utilizzata per ricercare un utente nella tabella hash
parametri:
	nome1 è il nome dell'utente da cercare nella tabella hash
	hash è la chiave (funzione hash) dell'utente
	T è la tabella hash
restituisce:
	NULL se fallisce
	il puntatore all'utente cercato
*/
T_user_hash* RicercaHash(char* nome1, int hash, T_user_hash** T);

/*
Funzione utilizzata per cancellare la tabella hash
parametri:
	T è la tabella hash
	dim è la dimensione della tabella hash
restituisce:
*/
void CancellaTabellaUtenti(T_user_hash** T, int dim);

/*
Funzione utilizzata per ricercare un file descriptor associato ad un utente nella tabella hash
parametri:
	nome è il nome dell'utente da cercare nella tabella hash
	funzionehash è la chiave (funzione hash) dell'utente
	T è la tabella hash
restituisce:
	il file descriptor associato all'utente avente come nome quello specificato come parametro
*/
int RicercaFd(T_user_hash** T,int funzionehash, char* nome);

/*
Funzione utilizzata per creare la lista con gli utenti connessi da inviare al client
parametri:
	T è la tabella hash
	lista è un puntatore alla lista dove verranno effettivamente salvati gli utenti online
	membrionline è il numero di membrionline presenti al momento della richiesta quindi è anche la dimensione della lista
	dimensione è la dimensione della tabella hash
restituisce:
*/
void SettaListaUtentiOnline(T_user_hash** T, char** lista, int membrionline, int dimensione);

/*
Funzione utilizzata per inserire un messaggio destinato ad un utente offline
parametri:
	head è il puntatore alla lista dei messaggi
	nuovomessaggio è il nuovo messaggio da inserire
	limite è il numero massimo di messaggi che possono andare nella lista
restituisce:
	il puntatore alla lista con il nuovo messaggio inserito
*/
ListaDiMessaggi* InserisciMessaggio(ListaDiMessaggi* head, message_t* nuovomessaggio, int limite);

/*
Funzione utilizzata per estrarre un messaggio dalla lista di un utente
parametri:
	head è il puntatore alla lista dei messaggi
	messaggio è dove verrà salvato il messaggio estratto
restituisce:
	NULL se fallisce
	il puntatore alla lista dei messaggi con un elemento in meno
*/
ListaDiMessaggi* EstraiMessaggio(ListaDiMessaggi* head, message_t ** messaggio);

/*
Funzione utilizzata per cancellare un utente dalla tabella hash
parametri:
	T è la tabella hash degli utenti
	hash è la funzione hash che mi serve per accedere alla specifica posizione della tabella
	nomeutente è il nome dell'utente da cancellare
restituisce:
	-1 se fallisce
	 1 se è andato a buon fine
*/
int CancellaUtenteHash(T_user_hash** T, int hash, char* nomeutente );

/*
Funzione utilizzata per restituire un nome associato ad un fd
parametri:
	T è la tabella hash degli utenti
	auxfd è il file descriptor da cercare
	dimensione è la dimensione della tabella hash
restituisce:
	NULL se fallisce
	il nome dell'utente con il fd ricercato
*/
char* RicercaFdNome(T_user_hash** T, int auxfd, int dimensione);

/*
Funzione utilizzata per contare il numero di messaggi per un determinato utente
parametri:
	tabella è la tabella hash degli utenti
	hash è la funzione hash che mi permette di accedere ad un precisa posizione della Tabella
	utente è il nome dell'utente di cui vogliamo il numero dei messaggi
restituisce:
	il numero di messaggi della lista associata ad un utente
*/
int NumeroMessaggi(T_user_hash** tabella, int hash, char* utente);


//----------------------FUNZIONI GRUPPO-----------------------------

/*
FUNZIONI DICHIARATE MA NON UTILIZZATE NEL Progetto
*/

membrogruppo* InserisciUtenteInGruppo(membrogruppo* head, char* nome);
T_hash_group* RicercaHashGruppo(char* stringa, int hash, T_hash_group** T);
T_hash_group* InserisciGruppo(T_hash_group* tabella, char* nomegruppo,int elementi);
int CancellaUtenteInGruppo(membrogruppo** T, int hash, char* nomeutente );
T_hash_group** CancellaTabellaGruppi(T_hash_group** T, int dim);
int RicercaMembroInGruppo(T_hash_group** gruppo, int funzionehashgruppo, char* membro, int funzionehashmembro);
membrogruppo* HashToList(T_hash_group** gruppo, int fhashgruppo, int dimensione);


#endif
