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
 * @file hash.c
 * @brief Implementazioni delle funzioni e delle strutture dati dedicate alla memorizzazione degli utenti
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "message.h"
#include "hash.h"

#define A 4321
#define B 5678
#define P 999149

/*
Funzione utilizzata per calcolare l'hash
parametri:
	stringa è il nome dell'utente
	n è il numero massimo di elementi della tabella
restituisce:
	l'indice della posizione della tabella hash nella quale accedere
*/
int FunzioneHash(char* stringa, int n){
	int v = 0;
	int i;
	for(i = 0; stringa[i] ; i++)
		v += (int)stringa[i];
	return (( A*v + B) % P) % n;
}

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
T_user_hash* InserisciTestaHash(T_user_hash* head, char* nome,int fd){
	if(nome!=NULL){
		//creo un nuovo utente
		T_user_hash* nuovo = malloc(sizeof(T_user_hash));
		if(nuovo == NULL) return NULL;
		strncpy(nuovo->nome,nome,MAX_NAME_LENGTH+1);
		nuovo->online = TRUE;
		nuovo->fd = fd;
		nuovo->MessLista = NULL;
		nuovo->next = head;
		return nuovo;
	}
	else{
		printf("c'è stato un errore durante l'inserimento del nome\n");
		return NULL;
	}
}

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
T_user_hash* RicercaHash(char* nomeutente, int hash, T_user_hash** T){
	int hashIndex;
	T_user_hash* corr;

	if(T== NULL){
		return NULL;
	}
	hashIndex = hash;
	corr = T[hashIndex];
	while(corr != NULL){
		if(corr->nome != NULL){
		int n = strncmp(corr->nome, nomeutente,strlen(nomeutente)+1);
		if (n == 0) return corr;

	}
		corr = corr->next;
	}

	return NULL;
}

/*
Funzione utilizzata per ricercare un file descriptor associato ad un utente nella tabella hash
parametri:
	nome è il nome dell'utente da cercare nella tabella hash
	funzionehash è la chiave (funzione hash) dell'utente
	T è la tabella hash
restituisce:
	il file descriptor associato all'utente avente come nome quello specificato come parametro
*/
int RicercaFd(T_user_hash** T,int funzionehash,char* nome){
	int fd;
	int trovato = 0; //variabile che mi serve per terminare la ricerca una volta trovato il nome senza scorrere tutta la lista
	T_user_hash* corr = T[funzionehash];
	while(!trovato && corr != NULL){
		if(strncmp(corr->nome,nome,sizeof(nome)+1) == 0){
			fd = corr->fd;
			trovato = 1;
		}
		corr = corr->next;
	}
	return fd;
}

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
char* RicercaFdNome(T_user_hash** T, int auxfd, int dimensione){
	T_user_hash* corr;
	int dimensionenome;
	char* nomeaux;
	int i;
	for(i = 0; i < dimensione; i++){
		corr = T[i];
		while(corr != NULL){
			if(corr->fd == auxfd ){
				dimensionenome = strlen(corr->nome) + 1;
				nomeaux = malloc(dimensionenome*sizeof(char));
				strncpy(nomeaux,corr->nome,dimensionenome);
				return nomeaux;
			}
			corr = corr->next;
		}
	}
	return NULL;
}

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
int CancellaUtenteHash(T_user_hash** T, int hash, char* nomeutente ){
	if(T == NULL) return -1;
	int hashIndex = hash;
	T_user_hash* corr = T[hashIndex];
	T_user_hash* prec = NULL;
	while(corr!=NULL){
		//durante la cancellazione distinguo la cancellazione del primo elemento della lista
		//con quello riguardante un generico elemento della lista
		int n = strncmp(corr->nome, nomeutente,strlen(nomeutente)+1);
		if (n == 0){
			if(prec == NULL){
				T[hashIndex] = corr->next;
			}
			else{
				prec->next = corr->next;
			}
			free(corr);
			return 1;
		}
		prec = corr;
		corr = corr->next;
	}
	return -1;
}

/*
Funzione utilizzata per estrarre un messaggio dalla lista di un utente
parametri:
	head è il puntatore alla lista dei messaggi
	messaggio è dove verrà salvato il messaggio estratto
restituisce:
	NULL se fallisce
	il puntatore alla lista dei messaggi con un elemento in meno
*/
ListaDiMessaggi* EstraiMessaggio(ListaDiMessaggi* head, message_t ** messaggio){
	//estraggo il messaggio in testa alla lista dei messaggi
	if(head != NULL){
			ListaDiMessaggi *corr = head;
			ListaDiMessaggi *prec = NULL;
			prec = corr;
			*messaggio = prec->messaggio;
			printf("operazione %d di prec\n",prec->messaggio->hdr.op);
			corr = corr->next;
			head = corr;
			free(prec);
			return corr;
	}
	return NULL;
}

/*
Funzione utilizzata per contare il numero di messaggi per un determinato utente
parametri:
	tabella è la tabella hash degli utenti
	hash è la funzione hash che mi permette di accedere ad un precisa posizione della Tabella
	utente è il nome dell'utente di cui vogliamo il numero dei messaggi
restituisce:
	il numero di messaggi della lista associata ad un utente
*/
int NumeroMessaggi(T_user_hash** tabella, int hash, char* mittente){
	int quanti = 0;
	T_user_hash *utente = tabella[hash];
	while(strncmp(utente->nome,mittente,strlen(mittente)+1) != 0 ) utente = utente->next;
	ListaDiMessaggi *aux = utente->MessLista;
	while(utente->MessLista != NULL){
		quanti++;
		utente->MessLista = (utente->MessLista)->next;
	}
	utente->MessLista = aux;
	return quanti;
}

/*
Funzione utilizzata per inserire un messaggio destinato ad un utente offline
parametri:
	head è il puntatore alla lista dei messaggi
	nuovomessaggio è il nuovo messaggio da inserire
	limite è il numero massimo di messaggi che possono andare nella lista
restituisce:
	il puntatore alla lista con il nuovo messaggio inserito
*/
ListaDiMessaggi* InserisciMessaggio(ListaDiMessaggi* head, message_t* nuovomessaggio, int limite){
	ListaDiMessaggi* corr;
	ListaDiMessaggi* new = malloc(sizeof(ListaDiMessaggi));
	memset(new,0,sizeof(ListaDiMessaggi));
	new->messaggio = nuovomessaggio;
	new->next = NULL;
	if(head == NULL){
		head = new;
		return head;
	}
	corr = head;
	int lunghezza = 0;
	while(corr != NULL){
		lunghezza = lunghezza + 1;
		corr = corr->next;
	}
	//se abbiamo raggiunto il numero massimo di messaggi cancello il messaggio più vecchio ed inserisco in coda il più recente
	if(lunghezza == limite ){
		ListaDiMessaggi* aux = head;
		head = head->next;
		free(aux);
		aux = head;
		while(aux->next != NULL){
			aux = aux->next;
		}
		aux->next = new;
		return head;
	}
	//se invece non abbiamo raggiunto ancora il limite dei messaggi aggiungiamo in coda
	else if (lunghezza < limite){
		ListaDiMessaggi* aux = head;
		while(aux->next != NULL){
			aux = aux->next;
		}
		aux->next = new;
		return head;
	}
	corr->next = new;
	return head;
}

/*
Funzione utilizzata per cancellare la tabella hash
parametri:
	T è la tabella hash
	dim è la dimensione della tabella hash
restituisce:
*/
void CancellaTabellaUtenti(T_user_hash** T, int dim ){
	if (T == NULL){
		fprintf(stderr, "non sono riuscito a cancellare la tabella hash\n" );
	}
	int i;
	T_user_hash* corr;
	T_user_hash* prec;
	int dimhash = dim;
	for(i = 0 ; i < dimhash; i++){
		corr = T[i];
		while(corr != NULL){
			if(corr->MessLista != NULL){
				while(corr->MessLista != NULL){
					ListaDiMessaggi* precmess = corr->MessLista;
					message_t *aux;
					aux = corr->MessLista->messaggio;
					corr->MessLista = corr->MessLista->next;
					free(aux->data.buf);
					aux->data.buf = NULL;
					free(aux);
					aux = NULL;
					free(precmess);
					precmess = NULL;
				}
				free(corr->MessLista);
			}
			prec = corr;
			corr = corr->next;
			free(prec);
			}
	}
	free(T);
	T = NULL;
}

/*
Funzione utilizzata per creare la lista con gli utenti connessi da inviare al client
parametri:
	T è la tabella hash
	lista è un puntatore alla lista dove verranno effettivamente salvati gli utenti online
	membrionline è il numero di membrionline presenti al momento della richiesta quindi è anche la dimensione della lista
	dimensione è la dimensione della tabella hash
restituisce:
*/
void SettaListaUtentiOnline(T_user_hash** T, char** lista, int membrionline, int dimensione){
	if(T == NULL){
		return;
	}
	int i;
	// alloco la lista per gli utenti online che conterrà tutti gli utenti online
	*lista = malloc(membrionline * (MAX_NAME_LENGTH +1) * sizeof(char));
	memset(*lista, 0 , membrionline*(MAX_NAME_LENGTH +1) * sizeof(char));
	char* copia = * lista;
	  //cerco tutti gli utenti online e li copio nella lista
    for (i = 0; i < dimensione; i++){
		T_user_hash* corr = T[i];
		while(corr!=NULL){

			if(corr->nome != NULL && corr->online == TRUE){

				strncat(copia,corr->nome,sizeof(corr->nome)+1);
				strcat(copia," ");
			}
		corr = corr->next;
		}
	}
}

/*
********LE FUNZIONI QUI IMPLEMENTATE NON SONO STATE UTILIZZATE PER IL PROGETTO*********
*/

T_hash_group* InserisciGruppo(T_hash_group* tabella, char* nomegruppo, int dimensione){
			T_hash_group* nuovo = malloc(sizeof(T_hash_group));
			strncpy(nuovo->nomeg,nomegruppo,strlen(nomegruppo)+1);
			nuovo->next = tabella;
			nuovo->membri = (membrogruppo**)malloc(dimensione*sizeof(membrogruppo*));
			return nuovo;
}

membrogruppo* InserisciUtenteInGruppo(membrogruppo* head, char* nome){
	membrogruppo* nuovo = malloc(sizeof(membrogruppo));
	strncpy(nuovo->membro,nome,strlen(nome)+1);
	nuovo->next = head;
	return nuovo;
}

int CancellaUtenteInGruppo(membrogruppo** T, int hash, char* nomeutente ){
	if(T == NULL || hash == 0) return -1;
	int hashIndex = hash;
	membrogruppo* corr = T[hashIndex];
	membrogruppo* prec = NULL;
	while(corr!=NULL){
		int n = strncmp(corr->membro, nomeutente,strlen(nomeutente)+1);
		if (n == 0){
			if(prec == NULL){
				T[hashIndex] = corr->next;
			}
			else{
				prec->next = corr->next;
			}
			free(corr);
			return 1;
		}
		prec = corr;
		corr = corr->next;
	}
	return -1;
}

T_hash_group* CancellaGruppo(T_hash_group* head,char* nome){
	if(head != NULL){
		T_hash_group* prec = NULL;
		T_hash_group* corr = head;
		int trovato = 0;
		while(corr != NULL && !trovato){
			if(strncmp(corr->nomeg,nome,strlen(nome)+1) == 0) trovato = 1;
			else{
				prec = corr;
				corr = corr->next;
			}
		}
		if(trovato){
			if(prec == NULL){
				head = head->next;
				free(corr);
			}
			else{
				prec->next = corr->next;
				free(corr);
			}
		}
	}
return head;
}

T_hash_group* RicercaHashGruppo(char *nome ,int hash ,T_hash_group** T){
int hashIndex = hash;
	if(T[hashIndex] != NULL){

		int n = strncmp(T[hashIndex]->nomeg, nome,strlen(nome)+1);
		if (n == 0) return T[hashIndex];

	}

	return NULL;
}


int RicercaMembroInGruppo(T_hash_group** gruppo, int funzionehashgruppo, char* membro, int funzionehashmembro){
	int temp = -1;
	if(gruppo[funzionehashgruppo] != NULL){
		int trovato = 0;
		membrogruppo* utente = gruppo[funzionehashgruppo]->membri[funzionehashmembro];
		while(!trovato && utente != NULL){
			temp = strncmp(membro,utente->membro,(strlen(membro)+1));
			if (temp == 0) trovato = 1;
			utente = utente->next;
		}
	}
	return temp;
}


membrogruppo* HashToList(T_hash_group** gruppo, int fhashgruppo, int dimensione){

	printf("%d\n",fhashgruppo);
	if(gruppo[fhashgruppo] == NULL){
		fprintf(stderr,"posizione nulla\n");
		return NULL;
	}
	membrogruppo** Tabellaaux = gruppo[fhashgruppo]->membri;
	membrogruppo* listaaux = NULL;
	int i;
    for (i = 0; i < dimensione; i++){
		membrogruppo* corrTab = Tabellaaux[i];
		while(corrTab!=NULL){
			if(corrTab->membro != NULL){
				membrogruppo* corr;
				membrogruppo* new = (membrogruppo*)malloc(sizeof(membrogruppo));
				strncpy(new->membro, corrTab->membro, strlen(corrTab->membro)+1);
				new->next = NULL;
				if(listaaux == NULL){
					listaaux = new;
				}
				else{
				corr = listaaux;
				while(corr->next!=NULL){
					corr = corr->next;
				}
				corr->next = new;
				}
			}
			corrTab = corrTab->next;
		}
	}
	return listaaux;
}
