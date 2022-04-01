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
 * @file chatty.c
 * @brief File principale del server chatterbox
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>


/* inserire gli altri include che servono */
#include "message.h"
#include "hash.h"
#include "funzioniaux.h"
#include "coda.h"
#include "ops.h"
#include "config.h"
#include "stats.h"
#include "percorso.h"
#include "connections.h"
#include <sys/time.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sys/select.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <malloc.h>
/* struttura che memorizza le statistiche del server, struct statistics
 * e' definita in stats.h.
 *
 */
struct statistics  chattyStats = { 0,0,0,0,0,0,0 };

ElemFileConfig config = {"", -1, -1, -1, -1, -1, "", ""};

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

fd_set fdset;// fd per la select

int nMutex = 35; // n° mutex per la tabella hash degli utenti

int elementi = 0; // n° elementi della tabella hash per gli utenti (serve a calcolare la funzione hash)

volatile sig_atomic_t termina; // per quanto riguarda SIGINT, SIGTERM, SIGQUIT

volatile sig_atomic_t stampastatistiche = 0; // variabile per quanto riguarda SIGUSR1

int Nmassimocoda; // numero di elementi totali nella coda

char* DirName = NULL; //directory dove memorizzare i files da inviare agli utenti

Scoda* coda = NULL; // coda dove inserisco gli fd degli utenti

T_user_hash **Tabellahash; // tabella hash dove verranno memorizzati gli utenti

int LungHstyMess = 0; // lunghezza history messaggi

pthread_mutex_t Mdirectory = PTHREAD_MUTEX_INITIALIZER; // mutex per la cartella dei file

pthread_mutex_t* ArrayMutex = NULL; // array dove sono memorizzati i mutex della hash table per gli utenti

pthread_mutex_t MFile = PTHREAD_MUTEX_INITIALIZER; // mutex per i file nell'operazione di POSTFILE_OP

pthread_mutex_t Mcoda = PTHREAD_MUTEX_INITIALIZER; // mutex per la coda dei thread

pthread_mutex_t Mstatistiche = PTHREAD_MUTEX_INITIALIZER; // mutex per le statistiche

pthread_mutex_t MSet = PTHREAD_MUTEX_INITIALIZER; // mutex per la set

pthread_cond_t Cthreadpool = PTHREAD_COND_INITIALIZER; // variabile di condizione per svegliare un thread del pool dopo aver stabilito la connessione


/*
RichiesteOp è una funzione invocata da worker() e cioè viene eseguita dai thead del pool che servono il client.
*/

message_t* RichiesteOp(message_t* Richiesta, int fd );


void terminaserver() {

  write(1,"Ricevuto segnale terminazione\n",31);
  termina = 1;

}

void stampastatisticheserver(){

  write(1,"stampo statistiche\n",20);
  stampastatistiche = 1;

}

void StampaStatistiche(){

  FILE *Fstatistiche = fopen(config.StatFileName, "a+");
  if (Fstatistiche != NULL) {
    MyLock(Mstatistiche);
    printStats(Fstatistiche);
    MyUnlock(Mstatistiche);
  }
  fclose(Fstatistiche);
}

int signals_handler() {


  struct sigaction Huscita;
  struct sigaction Hstatistiche;
  struct sigaction Hpipe;

  //tratto prima i segnali di terminazione
  memset(&Huscita, 0, sizeof(Huscita));

  termina = 0;
  //assegno la funzione di terminazione all'header per gestire i segnali
  Huscita.sa_handler = terminaserver;

  //registro i segnali SIGINT, SIGQUIT, SIGTERM
  FError((sigaction(SIGINT, &Huscita, NULL) == -1),"sigaction");
  FError((sigaction(SIGQUIT, &Huscita, NULL) == -1),"sigaction");
  FError((sigaction(SIGTERM, &Huscita, NULL) == -1),"sigaction");

  //tratto il segnale per stampare le statistiche
  memset(&Hstatistiche, 0, sizeof(Hstatistiche));
  Hstatistiche.sa_handler = stampastatisticheserver;

  if (config.StatFileName != NULL) {
    FError((sigaction(SIGUSR1, &Hstatistiche, NULL) == -1),"sigaction");
  }

  //tratto il segnale SIGPIPE ignorandolo
  memset(&Hpipe, 0, sizeof(Hpipe));
  Hpipe.sa_handler = SIG_IGN;
  FError((sigaction(SIGPIPE, &Hpipe, NULL) == -1),"sigaction");

  return 0;

}



void *worker() {

	int conness = -1;
	int valorediritorno = 0;
	message_t* risp = NULL;
	message_t* Richiesta = NULL;

  	//continua fino a quando non è arrivato un segnale di terminazione
	while(!termina){
		valorediritorno = 0;

		printf("inizio il worker\n");

    	// mi alloco un messaggio che indicherà la richiesta del client
		Richiesta = malloc(sizeof(message_t));
		memset(Richiesta,0,sizeof(message_t));

		FError((Richiesta == NULL), "la calloc di richiesta non è andata a buon fine");

		MyLock(Mcoda);

   		 // se la coda è vuota mi sospendo in attesa che vi venga inserito almeno un client
		while(coda->lunghezza == coda->testa){

			FError((pthread_cond_wait(&Cthreadpool,&Mcoda) != 0),"wait nella coda");
     		 // una volta risvegliato controllo se ho ricevuto il segnale di terminazione in tal caso termino il thread
			if(termina) {
        		free(Richiesta);
        		Richiesta=NULL;
				MyUnlock(Mcoda);
				return (void*) NULL;
			}

		}

		//ora che ho almeno un elemento dalla coda lo estraggo
		conness = estraitesta(coda);
		MyUnlock(Mcoda);

		if (conness == -1) continue;
		printf(" estratto il descrittore %d\n", conness);

		//eseguo le richieste del thread che è stato estratto dalla coda (che quindi è connesso)
		Richiesta->data.buf = NULL;
		Richiesta->data.hdr.len = 0;


		valorediritorno = readHeader( conness, &(Richiesta->hdr));

		printf("DESTINATARIO %s\n", Richiesta->data.hdr.receiver);

		if(valorediritorno > 0){

			// la lettura dell'header è andata a buon fine

			//eseguo l'operazione
			risp = RichiesteOp(Richiesta, conness);

			printf("risposta %d\n", risp->hdr.op);
			//L'operazione è andata a buon fine
			if( risp->hdr.op == OP_OK && ( Richiesta->hdr.op != POSTTXT_OP && Richiesta->hdr.op != POSTTXTALL_OP && Richiesta->hdr.op != POSTFILE_OP && Richiesta->hdr.op != GETPREVMSGS_OP && Richiesta->hdr.op != REGISTER_OP && Richiesta->hdr.op != CONNECT_OP && Richiesta->hdr.op != USRLIST_OP)){

				//invio messaggio di risposta al client: controllo se devo mandare header + corpo oppure solo header
				if( Richiesta->hdr.op == UNREGISTER_OP || Richiesta->hdr.op == DISCONNECT_OP){

					int verificaintestazione = sendHeader(conness, &(risp->hdr));
					if(verificaintestazione != 1){
						printf("\t\tla sendHeader non è andata a buon fine\n");
						break;
					}
					else printf("richiesta di %d inviata con successo\n",conness);
				}
				else{

					int verificarichiesta= sendRequest(conness, risp);
					if(verificarichiesta != 1) {
						printf("\t\tla sendRequest non è andata a buon fine\n");
						break;
					}
					else printf("richiesta di %d inviata con successo\n",conness);
				}
			}

      		// se ricevo OP_FAIL_MITT oppure OP_STANDARD_ERR devo disconnettere l'utente
			if(risp->hdr.op == OP_FAIL_MITT || risp->hdr.op == OP_STANDARD_ERR){
				char* utente = RicercaFdNome(Tabellahash,conness,elementi);
        		//se l'utente esiste accedo alla sua posizione e scorro tutta la lista di utenti avente lo stesso hash
        		// subito dopo lo disconnetto
				if(utente != NULL){
					MyLock(ArrayMutex[((FunzioneHash(utente,elementi) % elementi) % nMutex)]);
					T_user_hash* nomeaux;
					int fhash = FunzioneHash(utente,elementi);
					nomeaux = Tabellahash[fhash];
					int trovatonome = 0;
					while(!trovatonome && nomeaux != NULL){
						if(strncmp(nomeaux->nome,utente,strlen(utente)+1) == 0) trovatonome = 1;
						else nomeaux = nomeaux->next;
					}
					if(trovatonome){
					  nomeaux->online = FALSE;
					  nomeaux->fd = -1;
					  MyUnlock(ArrayMutex[((FunzioneHash(utente,elementi) % elementi) % nMutex)]);
					  MyLock(Mstatistiche);
					  chattyStats.nonline--;
					  MyUnlock(Mstatistiche);
					}
				}
        if(risp->hdr.op == OP_FAIL_MITT){
          printf("RISPOSTA NON CORRETTA PER MANCANZA DI MITTENTE\n");
          setHeader(&(risp->hdr), OP_FAIL, "");
          sendHeader(conness, &(risp->hdr));
        }
				free(utente);
			}
			else{
				printf("rimetto in coda l'fd %d\n", conness);
				MyLock(MSet);
				FD_SET(conness, &fdset);
				MyUnlock(MSet);
			}
			free(risp);
			printf("leggo l'header successivo di %d\n",conness);
		}
    	//se il valore di ritorno della readHeader è <= 0 (caso di errore) disconnetto l'utente avente "conness" come filedescriptor
    	// ed assegnandogli -1 al suo posto
		else{
		char* utente = RicercaFdNome(Tabellahash,conness,elementi);
			if(utente != NULL){
				MyLock(ArrayMutex[((FunzioneHash(utente,elementi) % elementi) % nMutex)]);
				T_user_hash* nomeaux;
				int fhash = FunzioneHash(utente,elementi);
				nomeaux = Tabellahash[fhash];
				int trovatonome = 0;
				while(!trovatonome && nomeaux != NULL){
					if(strncmp(nomeaux->nome,utente,strlen(utente)+1) == 0) trovatonome = 1;
					else nomeaux = nomeaux->next;
				}
				if(trovatonome){
					nomeaux->online = FALSE;
					nomeaux->fd = -1;
					MyLock(Mstatistiche);
					chattyStats.nonline--;
					MyUnlock(Mstatistiche);
				}
				MyUnlock(ArrayMutex[((FunzioneHash(utente,elementi) % elementi) % nMutex)]);

			}

		free(utente);
		fprintf(stdout,"connessione chiusa\n");
		}




	if(Richiesta->data.buf!=NULL){
            free(Richiesta->data.buf);
            Richiesta->data.buf=NULL;
        }
        free(Richiesta);
        Richiesta=NULL;

	}
	if(Richiesta->data.buf!=NULL){
        free(Richiesta->data.buf);
        Richiesta->data.buf=NULL;
    }

    free(Richiesta);
    Richiesta=NULL;
		if(risp->data.buf!=NULL){
			free(risp->data.buf);
			risp->data.buf = NULL;
		}
		free(risp);
		risp=NULL;
    printf("\t\tsono prima del termine del worker\n");
	return (void*) NULL;

}


/*
RichiesteOp è una funzione invocata da worker() e cioè viene eseguita solo dai thread del pool che servono il client.
La funzione ha il compito di comprendere l'operazione richiesta dal client, eseguire tale operazione e aggiornare le statistiche
restituendo un messaggio contenente la risposta da inviare al client.
*/

message_t* RichiesteOp(message_t* Richiesta, int fd ){

	int verificainvio = -1;
	message_t* risposta = (message_t*)calloc(1,sizeof(message_t));

	FError(risposta == NULL, "errore della malloc nelle richiesteop");
	
  	//setto inizialmente una generica risposta definita da questa "OP_STANDARD" che
	setHeader(&(risposta->hdr),OP_STANDARD, "");

  	//leggo il body del messaggio, se c'è stato un errore mando una risposta di errore
	if(readData(fd, &(Richiesta->data)) < 0 ){
		free(Richiesta->data.buf);
		Richiesta->data.buf = NULL;
		printf("errore nella lettura dei dati\n");

		setHeader(&(risposta->hdr),OP_STANDARD_ERR, "");
		return risposta;
	}
	risposta->data.hdr.len = 0;
	risposta->data.buf = "";
	char *mittente = Richiesta->hdr.sender;
  	//se non il mittente inserito non è valido rispondo con un errore
	if(mittente == NULL || strlen(mittente) == 0) {
		setHeader(&(risposta->hdr), OP_FAIL_MITT, "");
		printf("il mittente non esiste\n");
		return risposta;
	}
	printf("mittente %s\n", mittente);

	op_t operazione = Richiesta->hdr.op;

  	//guardo che operazione è stata richiesta
	switch(operazione) {

    	//caso in cui voglio registrare un utente
		case REGISTER_OP:{

      printf("inizio la REGISTER_OP\n");
			T_user_hash* newutente = NULL;
			int fhashutenti;

      		//risposta->hdr.op conterrà sicuramente OP_STANDARD perchè è stato settato prima dello switch
			if(risposta->hdr.op == OP_STANDARD){

					//controllo che l'utente non sia già registrato
					MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);

					fhashutenti = FunzioneHash(mittente,elementi);
					newutente = RicercaHash(mittente,fhashutenti,Tabellahash);

					MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);

          			//se l'utente è già presente setto OP_NICK_ALREADY come risposta e la invio al client
					if (newutente != NULL){

						setHeader(&(risposta->hdr) , OP_NICK_ALREADY , "");
						verificainvio = sendHeader(fd, &(risposta->hdr));
						if(verificainvio < 0){
							free(Richiesta->data.buf);
							Richiesta->data.buf = NULL;
							setHeader(&(risposta->hdr), OP_STANDARD_ERR, "");
							return risposta;
						}

					}
			}
			//in questo caso l'utente non è registrato ed avrà ancora OP_STANDARD come risposta
			if(risposta->hdr.op == OP_STANDARD){

				MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
				fhashutenti = FunzioneHash(mittente,elementi);
				//inserisco il mittente all'interno della tabella hash salvando anche il suo file descriptor
				if((Tabellahash[fhashutenti] = InserisciTestaHash(Tabellahash[fhashutenti],mittente,fd)) != NULL){

          			//se è andato tutto bene l'inserimento setto OP_OK come risposta
					setHeader(&(risposta->hdr) , OP_OK, "");
					sendHeader(fd,&(risposta->hdr));

				}

			MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);

			}

			//aggiorno le statistiche
			MyLock(Mstatistiche);

			if(risposta->hdr.op != OP_OK) chattyStats.nerrors++;
			else{
				chattyStats.nusers++;
				chattyStats.nonline++;
			}
			int lunghezza = chattyStats.nonline;
			MyUnlock(Mstatistiche);
			char *buffer;
			SettaListaUtentiOnline(Tabellahash,&buffer,lunghezza,elementi);
			setData(&(risposta->data), "",buffer, lunghezza * (MAX_NAME_LENGTH +1 ));
			sendData(fd,&(risposta->data));
			free(buffer);

      printf("termino la REGISTER_OP\n");

		} break;

    	//caso in cui voglio connettere un utente
		case CONNECT_OP:{

      printf("inizio la CONNECT_OP\n");

			T_user_hash* new = NULL;
      		// trovatomitt è una variabile che è uguale ad 1 solo se l'utente da impostare "online" è stato trovato
			int trovatomitt = 0;
			// verificaonline è una variabile che mi permette di capire se l'utente è già connesso oppure no, nel caso in cui fosse già
			// connesso non incremento il numero di utenti online nelle statistiche
			int verificaonline = 0;

			if(risposta->hdr.op == OP_STANDARD){

					MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
					int fhash = FunzioneHash(mittente,elementi);
					new = RicercaHash(mittente,fhash,Tabellahash);
					MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);

          			// in questo caso l'utente non è presente quindi non posso neanche impostarlo come "online" e quindi rispondo con
          			// OP_NICK_UNKNOWN al client
					if (new == NULL) {

						setHeader(&(risposta->hdr) , OP_NICK_UNKNOWN, "");
						sendHeader(fd, &(risposta->hdr));

						}

					/*Passo l'utente che ho trovato da offline ad online*/
					MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
					while(!trovatomitt && new!=NULL){

            			// se è stato trovato l'utente ma è già online
						if (new != NULL && new->online == TRUE && (strcmp(mittente,new->nome) == 0 )){
							trovatomitt = 1;
							verificaonline = 1;

						}

            			// se l'utente è stato trovato ma era offline
						if(new!=NULL && new->online == FALSE && (strcmp(mittente,new->nome) == 0)){
							trovatomitt = 1;
							new->online = TRUE;
							new->fd = fd;
						}
						new = new->next;

					}
					MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
			}

				//caso in cui l'utente è registrato indipendentemente dal fatto di essere online/offline
				if(risposta->hdr.op == OP_STANDARD){

					// inserisco l'utente nella coda degli utenti online
					MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
					setHeader(&(risposta->hdr) , OP_OK, "");
					sendHeader(fd,&(risposta->hdr));
					MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
				}

			// modifico le statistiche
			MyLock(Mstatistiche);

			if(risposta->hdr.op != OP_OK) chattyStats.nerrors++;
			if(verificaonline == 0 && risposta->hdr.op == OP_OK) chattyStats.nonline++;
			int lunghezza = chattyStats.nonline;
			MyUnlock(Mstatistiche);
			//stampo la lista dei membri online
			char *buffer;
			SettaListaUtentiOnline(Tabellahash,&buffer,lunghezza,elementi);

			setData(&(risposta->data), "",buffer, lunghezza * (MAX_NAME_LENGTH +1 ));
			sendData(fd,&(risposta->data));
			free(buffer);

      printf("termino la CONNECT_OP\n");

		} break;

    	//caso in cui voglio mandare un messaggio ad un altro utente
		case POSTTXT_OP: {

      printf("inizio la POSTTXT_OP\n");

			T_user_hash *new;
			T_user_hash *destinatario;
			message_t *nuovomessaggio;
			int fhash;
			int fhashdest;
			int auxfd;

			//controllo sul body del messaggio
			if(risposta->hdr.op == OP_STANDARD){

        		// se il messaggio ha una dimensione più grande del dovuto incremento il numero di errori e rispondo al client con
        		// OP_MSG_TOOLONG
				if(Richiesta->data.hdr.len > config.MaxMsgSize){
					MyLock(Mstatistiche);
					chattyStats.nerrors++;
					MyUnlock(Mstatistiche);
					setHeader(&(risposta->hdr), OP_MSG_TOOLONG, "");
					sendHeader(fd, &(risposta->hdr));
				}
			}

      		// se il controllo sul body del messaggio è andato tutto bene
			if(risposta->hdr.op == OP_STANDARD){

					MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
					fhash = FunzioneHash(mittente,elementi);
					new = RicercaHash(mittente,fhash,Tabellahash);
					MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);

          			//se l'utente non è presente aggiorno le statistiche e rispondo al client con un messaggio di errore
					if (new == NULL) {
						MyLock(Mstatistiche);
						chattyStats.nerrors++;
						MyUnlock(Mstatistiche);
						setHeader(&(risposta->hdr), OP_NICK_UNKNOWN,"");
						sendHeader(fd, &(risposta->hdr));

						}

			}

				//caso in cui il mittente esiste e voglio inviare il messaggio
				//controllo prima se esiste anche il destinatario
				if(risposta->hdr.op == OP_STANDARD){

					fprintf(stdout,"il mittente è connesso e pronto ad inviare il messaggio\n");

						MyLock(ArrayMutex[((FunzioneHash(Richiesta->data.hdr.receiver,elementi) % elementi) % nMutex)]);
						fhashdest = FunzioneHash(Richiesta->data.hdr.receiver,elementi);
						destinatario = RicercaHash(Richiesta->data.hdr.receiver,fhashdest,Tabellahash);
						MyUnlock(ArrayMutex[((FunzioneHash(Richiesta->data.hdr.receiver,elementi) % elementi) % nMutex)]);

            		// essendo una tabella hash con liste di trabocco può essere che ad una determinata chiave ci siano più
           			// utenti ergo scansiono tutta la lista fino a trovare l'utente che mi serve
					if (destinatario != NULL){

              			int trovato = 0;

              			while(!trovato && destinatario != NULL){

                			if(strcmp(destinatario->nome,Richiesta->data.hdr.receiver) == 0 ) trovato = 1;
							else destinatario = destinatario->next;

              			}

					}

            			// se il destinatario non esiste rispondo con un messaggio di errore ed aggiorno le statistiche
						if (destinatario == NULL) {

							setHeader(&(risposta->hdr), OP_NICK_UNKNOWN, "");
							MyLock(Mstatistiche);
							chattyStats.nerrors++;
							MyUnlock(Mstatistiche);
							sendHeader(fd, &(risposta->hdr));

						}

						//se il destinatario è presente ed online e non ha lo stesso nome del mittente (altrimenti invierebbe un messaggio a se stesso)
						else if((strncmp(destinatario->nome,mittente,strlen(destinatario->nome)+1) != 0) && destinatario!=NULL && destinatario->online == TRUE){

              				//creo un nuovo messaggio con le relative informazioni settando come TXT_MESSAGE l'operazione ad esso associata
              				//e lo invio al destinatario aggiornando le statistiche
							nuovomessaggio = (message_t*)malloc(sizeof(message_t));
							FError(nuovomessaggio == NULL, "malloc per l'allocazione del messaggio");
							memset((char*) nuovomessaggio,0,sizeof(message_t));
							strncpy(nuovomessaggio->hdr.sender,mittente,MAX_NAME_LENGTH + 1);
							nuovomessaggio->data.hdr.len = Richiesta->data.hdr.len;
							strncpy(nuovomessaggio->data.hdr.receiver,Richiesta->data.hdr.receiver,MAX_NAME_LENGTH + 1);
							nuovomessaggio->data.buf = (char*)calloc(nuovomessaggio->data.hdr.len,sizeof(char));
							for(int indice = 0; indice < nuovomessaggio->data.hdr.len; indice++){
								nuovomessaggio->data.buf[indice] = Richiesta->data.buf[indice];
							}

							nuovomessaggio->hdr.op = TXT_MESSAGE;
							setHeader(&(risposta->hdr), OP_OK, "");
							MyLock(Mstatistiche);
							chattyStats.ndelivered++;
							MyUnlock(Mstatistiche);
							//ricerco il file descriptor dell'utente al quale inviare il messaggio e lo invio
							auxfd = RicercaFd(Tabellahash,fhashdest,destinatario->nome);
							sendRequest(auxfd,nuovomessaggio);
							free(nuovomessaggio->data.buf);
							free(nuovomessaggio);

						}

						//se il destinatario c'è ma è offline e non ha lo stesso nome del mittente
						else if((strncmp(destinatario->nome,mittente,strlen(destinatario->nome)+1) != 0) && destinatario!=NULL && destinatario->online == FALSE){

                				//creo un nuovo messaggio con le relative informazioni settando come TXT_MESSAGE l'operazione ad esso associata
               					// e lo inserisco nella lista dei messaggi pendenti del destinatario
								nuovomessaggio = malloc(sizeof(message_t));
								FError(nuovomessaggio == NULL, "malloc per l'allocazione del messaggio");
								memset((char*) nuovomessaggio,0,sizeof(message_t));
								strncpy(nuovomessaggio->hdr.sender,mittente,MAX_NAME_LENGTH + 1);
								nuovomessaggio->data.hdr.len = Richiesta->data.hdr.len;
								strncpy(nuovomessaggio->data.hdr.receiver,Richiesta->data.hdr.receiver,MAX_NAME_LENGTH + 1);
								nuovomessaggio->data.buf = (char*)malloc(nuovomessaggio->data.hdr.len * sizeof(char));

								for(int indice = 0; indice < nuovomessaggio->data.hdr.len; indice++){
									nuovomessaggio->data.buf[indice] = Richiesta->data.buf[indice];
								}
								nuovomessaggio->hdr.op = TXT_MESSAGE;

								MyLock(ArrayMutex[((FunzioneHash(destinatario->nome,elementi) % elementi) % nMutex)]);
								destinatario->MessLista = InserisciMessaggio(destinatario->MessLista,nuovomessaggio,LungHstyMess);
								MyUnlock(ArrayMutex[((FunzioneHash(destinatario->nome,elementi) % elementi) % nMutex)]);
								MyLock(Mstatistiche);
								chattyStats.nnotdelivered++;
								MyUnlock(Mstatistiche);
								setHeader(&(risposta->hdr), OP_OK, "");

						}

			sendHeader(fd,&(risposta->hdr));

      printf("termino la POSTTXT_OP\n");
      }
	}break;

    // caso in cui voglio inviare un messaggio a tutti gli utenti
	case POSTTXTALL_OP:{

      printf("inizio la POSTTXTALL_OP\n");
			T_user_hash *new = NULL;
			int nomeuguale;

			//controllo sul body del messaggio (Richiesta->data)
			FError(Richiesta->data.hdr.receiver == NULL || Richiesta->data.hdr.len <= 0, "operazione POSTTXT_OP non riuscita");

      		//se il messaggio ha una dimensione più grande del dovuto aggiorno le statistiche e rispondo con OP_MSG_TOOLONG
			if(risposta->hdr.op == OP_STANDARD){

				if(Richiesta->data.hdr.len > config.MaxMsgSize){
					setHeader(&(risposta->hdr), OP_MSG_TOOLONG, "");
					MyLock(Mstatistiche);
					chattyStats.nerrors++;
					MyUnlock(Mstatistiche);
					sendHeader(fd, &(risposta->hdr));

				}
			}

      		//se il controllo sulla lunghezza è andato a buon fine controllo se esiste il mittente che ha richiesto l'operazione
			if(risposta->hdr.op == OP_STANDARD){

					MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
					int fhash = FunzioneHash(mittente,elementi);
					new = RicercaHash(mittente,fhash,Tabellahash);
					MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);

          			//se non esiste rispondo con un messaggio di errore OP_NICK_UNKNOWN e aggiorno le statistiche
					if (new == NULL) {

						setHeader(&(risposta->hdr), OP_NICK_UNKNOWN, "");
						MyLock(Mstatistiche);
						chattyStats.nerrors++;
						MyUnlock(Mstatistiche);
						sendHeader(fd, &(risposta->hdr));

					}

          			// se l'utente è presente non faccio nulla e proseguo nell'operazione
					if (new != NULL);
			}

		//caso in cui il mittente esiste e voglio inviare il messaggio a tutta la Tabellahash
    	//non controllo l'esistenza del destinatario in questo caso
		if(risposta->hdr.op == OP_STANDARD){

			for (int i = 0; i < elementi ; i++){

				T_user_hash* corr = Tabellahash[i];

				//non sto considerando una posizione vuota
				if(corr!=NULL){

					while(corr != NULL){

						nomeuguale = strncmp(corr->nome,Richiesta->hdr.sender,strlen(Richiesta->hdr.sender) + 1);
						//se il mittente ha il nome diverso dal destinatario(altrimenti manderei un messaggio a me stesso)
						if(nomeuguale != 0){

							//utente esistente ma offline
							if(corr->nome !=NULL && corr->online == FALSE){

								  // creo il messaggio da appendere alla coda dei messaggi
									message_t *nuovomessaggio = (message_t*)malloc(sizeof(message_t));
									FError(nuovomessaggio == NULL, "malloc per l'allocazione del messaggio");
									memset((char*) nuovomessaggio,0,sizeof(message_t));
									strncpy(nuovomessaggio->hdr.sender,mittente,MAX_NAME_LENGTH + 1);
									nuovomessaggio->data.hdr.len = Richiesta->data.hdr.len;
									strncpy(nuovomessaggio->data.hdr.receiver,Richiesta->data.hdr.receiver,MAX_NAME_LENGTH + 1);
									nuovomessaggio->data.buf = (char*)calloc(nuovomessaggio->data.hdr.len,sizeof(char));
									for(int indice = 0; indice < nuovomessaggio->data.hdr.len; indice++){
										nuovomessaggio->data.buf[indice] = Richiesta->data.buf[indice];
									}
									nuovomessaggio->hdr.op = TXT_MESSAGE;

                  					//appendo il messaggio ed aggiorno le statistiche
									MyLock(ArrayMutex[((FunzioneHash(corr->nome,elementi) % elementi) % nMutex)]);
									corr->MessLista = InserisciMessaggio(corr->MessLista,nuovomessaggio,LungHstyMess);
									MyUnlock(ArrayMutex[((FunzioneHash(corr->nome,elementi) % elementi) % nMutex)]);

									MyLock(Mstatistiche);
									chattyStats.nnotdelivered++;
									MyUnlock(Mstatistiche);

							}

              				// se il destinatario esiste ed è online
							else if(corr->nome != NULL && corr->online == TRUE){

                				//creo il nuovo messaggio e lo invio direttamente al destinatario aggiornando le statistiche
								message_t *nuovomessaggio = (message_t*)malloc(sizeof(message_t));
								FError(nuovomessaggio == NULL, "malloc per l'allocazione del messaggio");
								memset((char*) nuovomessaggio,0,sizeof(message_t));
								strncpy(nuovomessaggio->hdr.sender,mittente,MAX_NAME_LENGTH + 1);
								nuovomessaggio->data.hdr.len = Richiesta->data.hdr.len;
								strncpy(nuovomessaggio->data.hdr.receiver,Richiesta->data.hdr.receiver,MAX_NAME_LENGTH + 1);
								nuovomessaggio->data.buf = (char*)calloc(nuovomessaggio->data.hdr.len,sizeof(char));
								for(int indice = 0; indice < nuovomessaggio->data.hdr.len; indice++){
									nuovomessaggio->data.buf[indice] = Richiesta->data.buf[indice];
								}
								nuovomessaggio->hdr.op = TXT_MESSAGE;
								MyLock(Mstatistiche);
								chattyStats.ndelivered++;
								MyUnlock(Mstatistiche);
								setHeader(&(risposta->hdr), OP_OK, "");
								int verificasendrequest = sendRequest(corr->fd,nuovomessaggio);
								if(verificasendrequest != 1){
									printf("\tla sendrequest di: %d non è andata a buon fine\n",corr->fd);
								}
								free(nuovomessaggio->data.buf);
								free(nuovomessaggio);

							}

						}
					corr = corr->next;
					}
				}
			}
		}

			setHeader(&(risposta->hdr), OP_OK, "");
			sendHeader(fd,&(risposta->hdr));

      printf("termino la POSTTXTALL_OP\n");

		} break;

    // caso in cui voglio inviare un file ad un utente
	case POSTFILE_OP:{

      printf("inizio la POSTFILE_OP\n");

			T_user_hash *new = NULL;
			T_user_hash *destinatario = NULL;
			message_data_t file;
			readData(fd, &file);
			char* nomecartella;
			char* nomefile;
			int fhashdest;

			//controllo sul body del messaggio (Richiesta->data) e sulla dimensione del file
			FError(Richiesta->data.hdr.receiver == NULL || Richiesta->data.hdr.len <= 0, "operazione POSTFILE_OP non riuscita");
			printf("inizio la POSTFILE_OP\n");
			if(risposta->hdr.op == OP_STANDARD){
				printf("NOME FILE :%s\n", Richiesta->data.buf);
				printf("NOME DEL MITTENTE :%s\n", mittente);
				printf("NOME DEL DESTINATARIO :%s\n", Richiesta->data.hdr.receiver);
				printf("DIMENSIONE FILE.HDR: %d, DIMENSIONE CONFIG.MAXFILESIZE: %d\n", file.hdr.len,(config.MaxFileSize*1024));

        		//se la dimensione non va bene rispondo con OP_MSG_TOOLONG e aggiorno le statistiche
				if( (file.hdr.len / 1024) > config.MaxFileSize){
					MyLock(Mstatistiche);
					chattyStats.nerrors++;
					MyUnlock(Mstatistiche);
					setHeader(&(risposta->hdr), OP_MSG_TOOLONG, "");
					if ((sendHeader(fd, &(risposta->hdr))) < 0 ){
					//il file ha una dimensione più grande del dovuto
						free(Richiesta->data.buf);
						Richiesta->data.buf = NULL;
						free(file.buf);
						setHeader(&(risposta->hdr), OP_STANDARD_ERR, "");
						sendHeader(fd, &risposta->hdr);
					}
				}

			}

      		// caso in cui i controlli sono andati a buon fine controllo se esiste il mittente
			if(risposta->hdr.op == OP_STANDARD){

					MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
					int fhash = FunzioneHash(mittente,elementi);
					new = RicercaHash(mittente,fhash,Tabellahash);
					MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);

        		//se il mittente non esiste rispondo con un messaggio di errore ed aggiorno le statistiche
				if (new == NULL) {

						setHeader(&(risposta->hdr), OP_NICK_UNKNOWN, "");
						MyLock(Mstatistiche);

						chattyStats.nerrors++;
						MyUnlock(Mstatistiche);
						sendHeader(fd, &(risposta->hdr));

						}

				}

				//caso in cui il mittente esiste e voglio inviare il file
				//controllo prima se esiste anche il destinatario
				if(risposta->hdr.op == OP_STANDARD){

					MyLock(ArrayMutex[((FunzioneHash(Richiesta->data.hdr.receiver,elementi) % elementi) % nMutex)]);
					fhashdest = FunzioneHash(Richiesta->data.hdr.receiver,elementi);
					destinatario = RicercaHash(Richiesta->data.hdr.receiver,fhashdest,Tabellahash);
					MyUnlock(ArrayMutex[((FunzioneHash(Richiesta->data.hdr.receiver,elementi) % elementi) % nMutex)]);

          			// se il destinatario non è presente rispondo con un messaggio di errore ed aggiorno le statistiche
					if (destinatario == NULL) {

						setHeader(&(risposta->hdr), OP_NICK_UNKNOWN, "");
						MyLock(Mstatistiche);
						chattyStats.nerrors++;
						MyUnlock(Mstatistiche);
						sendHeader(fd, &(risposta->hdr));

						}

				}

				//caso in cui sia il mittente che il destinatario sono presenti e il mittente può inviare un file al destinatario
				if(risposta->hdr.op == OP_STANDARD){

          			//prendo il nome della cartella dal file delle configurazioni
					nomecartella = malloc((strlen(config.DirName)+1)*sizeof(char));
					memset(nomecartella,0,(strlen(config.DirName)+1));
					strncpy(nomecartella,config.DirName,strlen(config.DirName)+1);
          			//prendo il nome del file preceduto dal percorso
					nomefile = malloc((strlen(config.DirName) + strlen(Richiesta->data.buf) + 2)*sizeof(char));
					memset(nomefile,0,(strlen(config.DirName) + strlen(Richiesta->data.buf) + 2));
					strncpy(nomefile,config.DirName,strlen(config.DirName)-1);
          			//prendo il puntatore della prima occorrenza di "/" all'interno del messaggio. Il file può
          			//essere sia nella forma "./nome" oppure "nome" quindi il nome del file va modificato di conseguenza
					char *auxbuffer = strstr(Richiesta->data.buf,"/");
					printf("AUXBUFFER: %s RICHIESTA->DATA.BUF: %s\n",auxbuffer,Richiesta->data.buf);

          			//caso in cui il nome del file non contenga il carattere "/". Concateno quest'ultimo simbolo
          			//ed aggiungo il nome effettivo del file
					if(auxbuffer == NULL){
        				strncat(nomefile,"/",sizeof(char)+1);
        				strncat(nomefile,Richiesta->data.buf,strlen(Richiesta->data.buf)+1);
      		}

          	//il simbolo "/" è già presente quindi mi basta aggiungere il nome del file che sarà
          	//dato da auxbuffer
      		else{
        				strncat(nomefile,auxbuffer,strlen(auxbuffer)+1);
      		}

          			//cerco di aprire la cartella
					DIR * directoryFile;
          			// se non esiste la creo
					if ((directoryFile = opendir(nomecartella)) == NULL){
						if(errno == ENOENT){

						mkdir(nomecartella, 0700);

						}
						else{

						perror("apertura cartella");
						exit(EXIT_FAILURE);

						}

					}
          free(nomecartella);

          			//apro il file
					FILE* nuovofile = NULL;
					if ( (nuovofile = fopen(nomefile,"w")) == NULL) {
					   perror("apertura del file");
					   fprintf(stderr,"errore nell'apertura del file nella POSTFILE_OP\n");
					   Richiesta->data.buf = NULL;
					   fclose(nuovofile);
					   exit(EXIT_FAILURE);
					}
					// file aperto
					else{
            			free(nomefile);
						MyLock(MFile);
				    	//scrivo il contenuto del file
						if (fwrite(file.buf,sizeof(char),file.hdr.len,nuovofile) != file.hdr.len){
              			//se l'operazione non è andata bene
							perror("open");
      						fprintf(stderr, "LINEA DELL'ERRORE:%d\n", __LINE__);
          					fclose(nuovofile);
          					free(Richiesta->data.buf);
          					Richiesta->data.buf = NULL;
          					free(file.buf);
          				}
						MyUnlock(MFile);
					}

					fclose(nuovofile);
					closedir(directoryFile);
					printf("ho finito di scrivere nel file\n");

					//scorro fino a quando non trovo il nome del destinatario
					while(strncmp(destinatario->nome,Richiesta->data.hdr.receiver,strlen(destinatario->nome)+1) != 0 ) destinatario = destinatario->next;

          			//se ho trovato il destinatario ed è online
					if(destinatario!=NULL && destinatario->online == TRUE && strcmp(Richiesta->data.hdr.receiver,destinatario->nome) == 0){

            			//scrivo il nuovo messaggio da inviare direttamente avente FILE_MESSAGE come operazione e aggiornando le statistiche
						message_t *nuovomessaggio = (message_t*)malloc(sizeof(message_t));
						FError(nuovomessaggio == NULL, "malloc per l'allocazione del messaggio");
						memset((char*) nuovomessaggio,0,sizeof(message_t));
						strncpy(nuovomessaggio->hdr.sender,mittente,MAX_NAME_LENGTH + 1);
						nuovomessaggio->data.hdr.len = Richiesta->data.hdr.len;
						strncpy(nuovomessaggio->data.hdr.receiver,Richiesta->data.hdr.receiver,MAX_NAME_LENGTH + 1);
						nuovomessaggio->data.buf = (char*)calloc(nuovomessaggio->data.hdr.len,sizeof(char));
						for(int indice = 0; indice < nuovomessaggio->data.hdr.len; indice++){
							nuovomessaggio->data.buf[indice] = Richiesta->data.buf[indice];
						}
						nuovomessaggio->hdr.op = FILE_MESSAGE;
						setHeader(&(risposta->hdr), OP_OK, "");
						int verificasendrequest = sendRequest(destinatario->fd,nuovomessaggio);
						if(verificasendrequest != 1){
							printf("\t\t la verificasendrequest dentro la posttxt non è andata a buon fine\n");
						}
						free(nuovomessaggio->data.buf);
						free(nuovomessaggio);
						MyLock(Mstatistiche);
						chattyStats.nfiledelivered++;
						MyUnlock(Mstatistiche);
					}

          			//altrimenti se il destinatario esiste ma è offline
					else if(destinatario!=NULL && destinatario->online == FALSE  && strcmp(Richiesta->data.hdr.receiver,destinatario->nome) == 0){

						//creo il nuovo messaggio da mettere in coda avente FILE_MESSAGE come operazione ed aggiornando le statistiche
						message_t *nuovomessaggio = (message_t*)malloc(sizeof(message_t));
						FError(nuovomessaggio == NULL, "malloc per l'allocazione del messaggio");
						strncpy(nuovomessaggio->hdr.sender,mittente,MAX_NAME_LENGTH + 1);
						nuovomessaggio->data.hdr.len = Richiesta->data.hdr.len;
						strncpy(nuovomessaggio->data.hdr.receiver,Richiesta->data.hdr.receiver,MAX_NAME_LENGTH + 1);
						nuovomessaggio->data.buf = (char*)calloc(nuovomessaggio->data.hdr.len,sizeof(char));
						for(int indice = 0; indice < nuovomessaggio->data.hdr.len; indice++){
							nuovomessaggio->data.buf[indice] = Richiesta->data.buf[indice];
						}
						nuovomessaggio->hdr.op = FILE_MESSAGE;

						MyLock(ArrayMutex[((FunzioneHash(destinatario->nome,elementi) % elementi) % nMutex)]);
						destinatario->MessLista = InserisciMessaggio(destinatario->MessLista,nuovomessaggio,LungHstyMess);
						MyUnlock(ArrayMutex[((FunzioneHash(destinatario->nome,elementi) % elementi) % nMutex)]);
						setHeader(&(risposta->hdr), OP_OK, "");
						MyLock(Mstatistiche);
						chattyStats.nfilenotdelivered++;
						MyUnlock(Mstatistiche);
					}

				}

		sendHeader(fd,&(risposta->hdr));
		if(file.buf!=NULL) free(file.buf);

    printf("termina la POSTFILE_OP\n");

	}break;

    // caso in cui un utente vuole scaricare un file
	case GETFILE_OP:{

      printf("inizio la GETFILE_OP\n");

			FILE* nuovo;
			FILE* old;
			T_user_hash *new = NULL;

      		//prendo il nome del file preceduto dal percorso
			char* nomefile = malloc((strlen(Richiesta->data.buf) + strlen(config.DirName) +2)*sizeof(char));
			memset(nomefile,0,strlen(Richiesta->data.buf) + strlen(config.DirName) +2);
			strncpy(nomefile,config.DirName,strlen(config.DirName)-1);
			char* auxbuffer;
     		//prendo il puntatore della prima occorrenza di "/" all'interno del messaggio. Il file può
      		//essere sia nella forma "./nome" oppure "nome" quindi il nome del file va modificato di conseguenza
      		//caso in cui non c'è il carattere "/"
			if((auxbuffer=strstr(Richiesta->data.buf,"/"))==NULL){

    			strncat(nomefile,"/",sizeof(char)+1);
    			strncat(nomefile,Richiesta->data.buf,strlen(Richiesta->data.buf)+1);

  			}

  			else{

    			strncat(nomefile,auxbuffer,strlen(auxbuffer)+1);

  			}

      		//creo un nuovo nome per il file da scaricare avente un "1" alla fine del nome
			char* nomenuovofile = malloc((strlen(nomefile)+2)*sizeof(char));
			memset(nomenuovofile,0,(strlen(nomefile)+2)*sizeof(char));
			strncpy(nomenuovofile,nomefile,strlen(nomefile));
			strncat(nomenuovofile,"1",sizeof(char)+1);
			printf("\t\t nomenuovofile: %s\n",nomenuovofile);

      		//prendo il nome della cartella
			char* nomecartella = malloc((strlen(config.DirName)+1)*sizeof(char));
			memset(nomecartella,0,(strlen(config.DirName)+1));
			strncpy(nomecartella,config.DirName,strlen(config.DirName)+1);
			printf("\t\t nuovacartella: %s\n",nomecartella);

			DIR * directoryFile;
      		//se la cartella non esiste rispondo con un messaggio di errore
			if ((directoryFile = opendir(nomecartella)) == NULL){

				if(errno == ENOENT){

					setHeader(&(risposta->hdr), OP_FAIL, "");
					sendHeader(fd, &(risposta->hdr));

				}
			}

      		//se la cartella esiste vedo se esiste il mittente
			if(risposta->hdr.op == OP_STANDARD){

					MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
					int fhash = FunzioneHash(mittente,elementi);
					new = RicercaHash(mittente,fhash,Tabellahash);
					MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);

          			//caso in cui il mittente non esiste
					if (new == NULL){

						setHeader(&(risposta->hdr), OP_NICK_UNKNOWN, mittente);
						sendHeader(fd, &(risposta->hdr));
					}
			}
				//in questo caso l'utente è presente
				if(risposta->hdr.op == OP_STANDARD){

          			//apro il vecchio file in modalità lettura
					old = fopen(nomefile,"r");
					FError( old == NULL, "errore nell'apertura del file il lettura");
				}

				if(risposta->hdr.op == OP_STANDARD){

					struct stat controllofile;
					FError((stat(nomefile, &controllofile) == -1), "errore nelle statistiche del file");
          			//apro il nuovo file in modalità scrittura
					nuovo = fopen(nomenuovofile,"w");
					FError( nuovo == NULL, "errore nell'apertura del file in scrittura");
					printf("\t\t ho aperto il file:%s\n",nomenuovofile);
					//la funzione fgetc() ritorna il carattere c letto come unsigned char con cast ad int oppure EOF in caso di fine file o di errore.
					int c;
          			//copio tutto il contenuto del vecchio file nel nuovo
					while(1) {
      					c = fgetc(old);
      					if( feof(old) ) {
         					break;
      					}
      				fprintf(nuovo,"%c", c);
  					}
					fclose(nuovo);
					fclose(old);
					closedir(directoryFile);
					setHeader(&(risposta->hdr), OP_OK, "");
				}
				free(nomecartella);
				free(nomefile);
				free(nomenuovofile);
			//aggiorno le statistiche
			MyLock(Mstatistiche);
			if (risposta->hdr.op == OP_OK){
				chattyStats.nfilenotdelivered--;
				chattyStats.nfiledelivered++;
			}
			else if(risposta->hdr.op != OP_OK) chattyStats.nerrors++;
			MyUnlock(Mstatistiche);

      printf("termino la GETFILE_OP\n");

		} break;

    //caso in cui si vuole avere la coda dei messaggi avuti mentre l'utente era offline
	case GETPREVMSGS_OP:{

      printf("inizio la GETPREVMSGS_OP\n");

			T_user_hash *new = NULL;
			int fhash;

      		// vedo se esiste il mittente
			if(risposta->hdr.op == OP_STANDARD){

					MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
					fhash = FunzioneHash(mittente,elementi);
					new = RicercaHash(mittente,fhash,Tabellahash);
					MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);

          			//se non esiste il mittente rispondo con un messaggio di errore
					if (new == NULL){

						setHeader(&(risposta->hdr), OP_NICK_UNKNOWN, "");
						sendHeader(fd, &(risposta->hdr));
					}
				}
			//in questo caso l'utente è presente
			if(risposta->hdr.op == OP_STANDARD){

				//conto il numero di messaggi presenti in lista

				unsigned long quanti = NumeroMessaggi(Tabellahash,fhash,mittente);
				printf("i messaggi in coda sono %ld\n", quanti);
        		// se non ci sono messaggi in coda
				if(quanti == 0){

					setHeader(&(risposta->hdr), OP_OK, "");
					setData(&(risposta->data), "" , (char*)&quanti, sizeof(size_t));
					int verificasendrequest = sendRequest(fd, risposta);
					if(verificasendrequest != 1){
						printf("\t\t la sendrequest di getprevmsgs_op non è andata a buon fine\n ");
					}

				}
        		//se c'è almeno un messaggio in coda
				else if(quanti > 0) {
					memset(risposta,0,sizeof(message_t));
					setHeader(&(risposta->hdr), OP_OK, "");
					setData(&(risposta->data), "" , (char*)&quanti, sizeof(size_t));
					sendRequest(fd, risposta);

					//ci sono messaggi in coda
					message_t* messaggiopendente;
					while( new->MessLista  != NULL ){
						if(new->MessLista == NULL){
							break;
						}
            			//estraggo i messaggi nella lista uno alla volta
						new->MessLista = EstraiMessaggio(new->MessLista, &messaggiopendente);
						// se è un file
						if(messaggiopendente->hdr.op == FILE_MESSAGE){
							printf("sono in FILE_MESSAGE\n");
							MyLock(Mstatistiche);
							chattyStats.nfiledelivered++;
							chattyStats.nfilenotdelivered--;
							MyUnlock(Mstatistiche);
						}
            			// se è un messaggio
						else if (messaggiopendente->hdr.op == TXT_MESSAGE){
							printf("sono in TXT_MESSAGE\n");
							MyLock(Mstatistiche);
							chattyStats.ndelivered++;
							chattyStats.nnotdelivered--;
							MyUnlock(Mstatistiche);
						}
						int test = sendRequest(fd,messaggiopendente);
						if(test != 1 ){
							printf("\t\tl'invio della sendrequest in getprevmsgs_op non è andato a buon fine\n");
							free(Richiesta->data.buf);
							Richiesta->data.buf = NULL;
						}
						if(messaggiopendente->data.buf != NULL){
							free(messaggiopendente->data.buf);
							messaggiopendente->data.buf = NULL;
						}
						if(messaggiopendente != NULL){
							free(messaggiopendente);
							messaggiopendente = NULL;
						}
					}
				}
        		// in caso di errore
				else{
					setHeader(&(risposta->hdr), OP_FAIL, "");
					sendHeader(fd, &(risposta->hdr));

				}
			}

			//aggiorno le statistiche solo in caso di errore
			MyLock(Mstatistiche);
			if(risposta->hdr.op != OP_OK) chattyStats.nerrors++;
			MyUnlock(Mstatistiche);

			printf("termino la GETPREVMSGS_OP\n");
		} break;

    // caso in cui si vuole sapere quali utenti sono online in questo momento
	case USRLIST_OP:{
      printf("inizio la USRLIST_OP\n");
			if(risposta->hdr.op == OP_STANDARD){
				char *buffer;
				MyLock(Mstatistiche);
				int lunghezza = chattyStats.nonline;
				MyUnlock(Mstatistiche);
				SettaListaUtentiOnline(Tabellahash,&buffer,lunghezza,elementi);
				setData(&(risposta->data), "",buffer, lunghezza * (MAX_NAME_LENGTH +1 ));
				setHeader(&(risposta->hdr) , OP_OK, "");
				sendRequest(fd,risposta);
				free(buffer);
			}
      printf("termino la USRLIST_OP\n");
	} break;

    //caso in cui voglio cancellare un utente registrato
	case UNREGISTER_OP:{

      printf("inizio la UNREGISTER_OP\n");
			T_user_hash* new;
			T_user_hash* nomeaux;
			int fhash;
			bool auxonline = FALSE;	//mi serve per vedere se l'utente è online oppure no

			if(risposta->hdr.op == OP_STANDARD){

          			//controllo se esiste il mittente del messaggio
					MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
					fhash = FunzioneHash(mittente,elementi);
					new = RicercaHash(mittente,fhash,Tabellahash);
					MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);

         			 //se il mittente non esiste rispondo con un OP_NICK_UNKNOWN
					if (new == NULL){

						setHeader(&(risposta->hdr), OP_NICK_UNKNOWN, "");
						sendHeader(fd, &(risposta->hdr));
					}
			}
			//in questo caso l'utente è presente e va eliminato
			if(risposta->hdr.op == OP_STANDARD){

        		//scorro la lista di trabocco nel caso in cui ci fossero più utenti in quella posizione della tabella hash
				nomeaux = Tabellahash[fhash];
				int trovatonome = 0;
				while(!trovatonome && nomeaux != NULL){
					if(strncmp(nomeaux->nome,mittente,strlen(mittente)+1) == 0) trovatonome = 1;
					else nomeaux = nomeaux->next;
					}
				if(trovatonome){
          			//se l'utente è online salvo il suo stato in una variabile che mi servirà per le statistiche
					if(nomeaux->online == TRUE) auxonline = TRUE;
					nomeaux->online = FALSE;
					nomeaux->fd = -1;
				}

        		//cancello effettivamente l'utente dalla tabella hash
				MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
				if(CancellaUtenteHash(Tabellahash,fhash,mittente) == 1)
					setHeader(&(risposta->hdr), OP_OK, "");
				else{
          			// se l'operazione non è andata a buon fine
					setHeader(&(risposta->hdr), OP_FAIL, "");
					sendHeader(fd, &(risposta->hdr));
				}
			MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
			}

			//aggiorno le statistiche
			MyLock(Mstatistiche);
			if(risposta->hdr.op != OP_OK) chattyStats.nerrors++;
			else{
				chattyStats.nusers--;
				if(auxonline == TRUE){

				 chattyStats.nonline--;
				}
			}

			MyUnlock(Mstatistiche);

      printf("termino la UNREGISTER_OP\n");

	} break;

    	// caso in cui voglio disconnettere un utente facendolo passare da online ad offline
	case DISCONNECT_OP:{

      printf("inizio la DISCONNECT_OP\n");

			T_user_hash* new = NULL;
			int fhash;

			if(risposta->hdr.op == OP_STANDARD){

          			//controllo se il mittente esiste
					MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
					fhash = FunzioneHash(mittente,elementi);
					new = RicercaHash(mittente,fhash,Tabellahash);
					MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);

					if (new == NULL) {

            			//se il mittente non esiste rispondo OP_NICK_UNKNOWN
						setHeader(&(risposta->hdr), OP_NICK_UNKNOWN, ""); //l'utente non è presente
						sendHeader(fd, &(risposta->hdr));

						}

					if (new != NULL && new->online == TRUE){

						 ; //l'utente è online, va disconnesso

					}

          			//se l'utente esiste ma è offline rispondo con OP_FAIL
					if(new!=NULL && new->online == FALSE){

						setHeader(&(risposta->hdr), OP_FAIL, "");
						sendHeader(fd, &(risposta->hdr));

				}
			}

				//caso in cui l'utente è registrato ed è online e quindi va disconnesso
				if(risposta->hdr.op == OP_STANDARD){
					MyLock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
					T_user_hash* nomeaux;
					fhash = FunzioneHash(mittente,elementi);
					nomeaux = Tabellahash[fhash];
					int trovatonome = 0;
          			//scorro la lista di trabocco nel caso in cui ci fosse più di un utente in quella posizione della tabella
					while(!trovatonome && nomeaux != NULL){
						if(strncmp(nomeaux->nome,mittente,strlen(mittente)+1) == 0) trovatonome = 1;
						else nomeaux = nomeaux->next;
					}
					if(trovatonome){
          			// metto l'utente offline
					nomeaux->online = FALSE;
					nomeaux->fd = -1;
					}
					MyUnlock(ArrayMutex[((FunzioneHash(mittente,elementi) % elementi) % nMutex)]);
					setHeader(&(risposta->hdr), OP_OK, "");
			}
			//aggiorno le statistiche

			MyLock(Mstatistiche);

			if(risposta->hdr.op != OP_OK) chattyStats.nerrors++;
			else chattyStats.nonline--;
			MyUnlock(Mstatistiche);

      printf("termino la DISCONNECT_OP\n");

	} break;

		case CREATEGROUP_OP: break;

		case ADDGROUP_OP: break;

		case DELGROUP_OP: break;

    // caso in cui l'operazione richiesta non esiste
	default:{
			setHeader(&(risposta->hdr), OP_FAIL, "");
			sendHeader(fd, &(risposta->hdr));
		} break;
    }
    return risposta;
}


int main(int argc, char *argv[]){

	pthread_t* pool;
	struct sockaddr_un sAddress;
	char* nomefileconfig = NULL;
	int i;
	int fd_num = 0;
	int fd_sk;
	fd_set rdset;

  //controllo il numero degli argomenti passati da linea di comando
	if(argc != 3 || (strcmp(argv[1],"-f")!=0)){
		fprintf(stderr, "programma non inizializzato a dovere\n" );
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

   //lettura file configurazione
	for(i = 0; i < argc; i++){

		if(strcmp(argv[i], "-f") == 0) nomefileconfig = argv[i+1]; // devo prendere l'argomento immediatamente dopo "-f" per il file config

	}

	FError(nomefileconfig == NULL, "manca il file di configurazione");
  	//leggo il nome il file di configurazione e lo salvo in una apposita struttura
	int check = leggiconfig(nomefileconfig, &config );
	FError(check == -1, "il file di configurazione non è corretto");

	//file statistiche
	FILE* filestatistiche = NULL;
	filestatistiche = fopen(config.StatFileName, "a");
	if (filestatistiche == NULL){
		fprintf(stderr, "errore nell'apertura del file delle statistiche\n" );
		exit(EXIT_FAILURE);
	}
  // registro i segnali
  signals_handler();
  //copio l'indirizzo dal file di configurazione
	strncpy(sAddress.sun_path,config.UnixPath, strlen(config.UnixPath)+1); //+1 per carattere di terminazione \0
	sAddress.sun_family=AF_UNIX;

  	// setto il numero di elementi massimi e il numero di messaggi massimi
	elementi = config.MaxConnections;
	LungHstyMess = config.MaxHistMsgs;

  //mi alloco un array che mi servirà per i mutex sulla tabella hash
  ArrayMutex = malloc(nMutex*sizeof(pthread_mutex_t));
	FError(ArrayMutex == NULL,"array di mutex");
	for (i=0; i<nMutex; i++)
		pthread_mutex_init(&ArrayMutex[i], NULL);

	//creo un nuovo socket
	fd_sk = socket(AF_UNIX, SOCK_STREAM, 0);

	FError(fd_sk == -1, "creazione del socket");
	//assegno l'indirizzo definito in precedenza al socket fd_sk
	FError((bind(fd_sk, (struct sockaddr*) &sAddress, sizeof(sAddress)) == -1), "bind");

	//segnalo che il socket fd_sk accetta connessioni fino ad un massimo definito nel file di configurazione
	FError((listen(fd_sk,config.MaxConnections) == -1),"listen");

	//coda
	Nmassimocoda = config.MaxConnections;
	printf("IL NUMERO MASSIMO DI CONNESSIONI È: %d\n",Nmassimocoda);
	if(Nmassimocoda == 0 || Nmassimocoda < 0) Nmassimocoda = 1;

 	 //creo una coda che mi servirà per i file descriptor
	coda = (Scoda*)malloc(sizeof(Scoda));
	if (coda == NULL){
		fprintf(stderr, "errore durante la creazione della coda degli fd\n" );
		exit(EXIT_FAILURE);
	}
	coda->array=(int*)malloc(Nmassimocoda*sizeof(int));
	coda->lunghezza = 0; //numero elementi attuali nella coda circolare
	coda->dimensione = Nmassimocoda; //dimensione massima di elementi nella coda circolare
	coda->testa = 0; //utile per l'indice della coda durante l'estrazione

  	//inizializzo la tabella hash
	Tabellahash = (T_user_hash**) malloc((2*elementi)*sizeof(T_user_hash*));
	for( i = 0 ; i < elementi; i++){
		Tabellahash[i] = NULL;
	}

	int fd;
	int conness;
	if(fd_sk> fd_num) fd_num = fd_sk;
	FD_ZERO(&fdset);
	FD_SET(fd_sk, &fdset);

  	//creo il pool di thread
	FError(((pool = malloc(config.ThreadsInPool * sizeof(pthread_t))) == NULL), "allocazione del pool di thread");
	for (i = 0; i < config.ThreadsInPool; i++){
    FError((pthread_create(&pool[i], NULL, &worker, NULL)) != 0, "threads del pool");
		fprintf(stdout, "avvio thread[%d] del pool\n",i);
	}

	while(!termina){

		MyLock(MSet);
		rdset = fdset;
		//preparo la maschera per la select; va inizializzato ogni volta perchè la select la modifica
		MyUnlock(MSet);
		struct timeval time = {0,1000};

		int ret = select(fd_num + 1, &rdset, NULL, NULL, &time );

		if ( ret >= 0 ){

			// select andata a buon fine


			for(fd = 0 ; fd <= fd_num ; fd++){
				//controllo se il bit corrispondente al file descriptor fd nella maschera
				//rdset era ad 1 

				if(FD_ISSET(fd, &rdset)){
					if (fd == fd_sk){

            			//se non ho raggiunto il massimo delle connessioni possibili accetto la nuova connessione
						MyLock(Mstatistiche);
						if(chattyStats.nonline < config.MaxConnections){

							conness = accept(fd_sk, NULL, 0);

							MyLock(MSet);

							FD_SET(conness, &fdset);

							MyUnlock(MSet);
							if(conness > fd_num) fd_num = conness;
						}
            MyUnlock(Mstatistiche);

					}

					else{
						printf("\t\t Arrivata una nuova richiesta: %d\n",fd);
						MyLock(MSet);
						FD_CLR(fd, &fdset);
						MyUnlock(MSet);
						MyLock(Mcoda);

							//inserisco il nuovo file descriptor nella coda
							int verificacoda = inseriscicoda(coda, fd);

							if(verificacoda == -1){
								fprintf(stderr,"l'inserimento non è andato a buon fine\n");
								exit(EXIT_FAILURE);
							}
              				//mando una signal per svegliare i thread in attesa all'interno del threadpool
							pthread_cond_signal(&Cthreadpool);


						MyUnlock(Mcoda);
					}
				}
				else continue;
			}
		}
    //se mi arriva segnale di SIGUSR1
    if(stampastatistiche == 1){
      StampaStatistiche();
      stampastatistiche = 0;
    }
    //se mi arriva un segnale di terminazione
    if(termina){
      MyLock(Mcoda);
      pthread_cond_broadcast(&(Cthreadpool));
      MyUnlock(Mcoda);
      break;
    }

	}

  	//libero le risorse
	for(int t = 0; t < config.ThreadsInPool; t++){
		pthread_join(pool[t],NULL);
	}

	free(pool);

	for (fd = 0; fd < fd_num; fd++){
		shutdown(fd,SHUT_RDWR);
	}

	fclose(filestatistiche);

	free(config.StatFileName);

	close(fd_sk);

	free(config.DirName);

	cancellacoda(coda);

	free(coda);

	CancellaTabellaUtenti(Tabellahash,elementi);

	FError(unlink(config.UnixPath) == -1, "eliminazione file socket");

	free(config.UnixPath);

	free(ArrayMutex);

	fflush(stdout);

	return 0;
}
