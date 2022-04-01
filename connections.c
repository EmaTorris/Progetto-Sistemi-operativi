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
 * @file connections.c
 * @brief Implementazione delle funzioni definite in connections.h
 */

#if !defined(UNIX_PATH_MAX)
#define UNIX_PATH_MAX 64
#define MAX_RETRIES 10
#define MAX_SLEEPING 3
#include "message.h"
#include "connections.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <config.h>

/*
funzione presa direttamente dal libro
*/
ssize_t leggitutto(int fd, void* buf, size_t nbyte){

	ssize_t nread = 0, n;

	do{
		if((n = read(fd, &((char* )buf)[nread], nbyte - nread)) == -1){
			if(errno == EINTR)
				continue;
			else
				return -1;
		}
		if ( n == 0 )
			return nread;
		nread += n;
	} while(nread < nbyte);
	return nread;
}

/*
funzione presa direttamente dal libro
*/
ssize_t scrivitutto(int fd, const void* buf, size_t nbyte){
	ssize_t nwritten = 0, n;

	do{
		if(( n = write(fd, &((const char*)buf)[nwritten], nbyte - nwritten)) == -1){
			if (errno == EINTR)
				continue;
			else
				return -1;
		}
		nwritten += n;
	}	while(nwritten < nbyte);
	return nwritten;
}

/**
 * @file  connection.h
 * @brief Contiene le funzioni che implementano il protocollo
 *        tra i clients ed il server
 */

/**
 * @function openConnection
 * @brief Apre una connessione AF_UNIX verso il server
 *
 * @param path Path del socket AF_UNIX
 * @param ntimes numero massimo di tentativi di retry
 * @param secs tempo di attesa tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 *         -1 in caso di errore
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs){
	if(secs > MAX_SLEEPING) secs = MAX_SLEEPING;
	if(ntimes > MAX_RETRIES) ntimes = MAX_RETRIES;

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	int i=0;
	struct sockaddr_un address;
	if(fd == -1){
		perror("creazione socket per connessione con il server");
		return -1;
	}
	address.sun_family = AF_UNIX;
	strncpy(address.sun_path, path, strlen(path) + 1);

	//provo a connettermi in caso di esito negativo provo fino a ntimes volte
	while(i < ntimes){
		fprintf(stdout,"connessione dal client verso il server in corso....\n");
		if ((connect(fd, (struct sockaddr*)&address, sizeof(address))) == -1){
			i++;
			if(i < ntimes) sleep(secs);
		}
		else {
			fprintf(stdout,"connessione stabilita\n");
			return fd;
		}
	}
	return -1;
}

/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readHeader(long connfd, message_hdr_t *hdr){
  int letti = read(connfd, hdr, sizeof(message_hdr_t));
	if(letti < 0){
		perror("dimensione della read dell'header");
		fprintf(stdout,"file %s riga %d\n", __FILE__, __LINE__);
		return -1;
	}
	return 1;
}

/**
 * @function sendHeader
 * @brief Invia l'header del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore all'header del messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendHeader(long connfd, message_hdr_t *msg){
  int scritti = write(connfd, msg, sizeof(message_hdr_t));
	if(scritti < 0){
		perror("dimensione della read dell'header");
		fprintf(stdout,"file %s riga %d\n", __FILE__, __LINE__);
		return -1;
	}
	return 1;
}

/**
 * @function readData
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readData(long fd, message_data_t *data){

	int valorediritornoRec = read(fd, &(data->hdr), sizeof(message_data_hdr_t));
	if (valorediritornoRec < 0){
		perror("read valore di ritorno destinatario messaggio");
		fprintf(stdout,"file %s riga %d valorediritornoRec %d\n", __FILE__, __LINE__,valorediritornoRec);
		return -1;
	}
  int lunghezza = data->hdr.len;
	if(lunghezza > 0){
	data->buf = malloc((lunghezza)*sizeof(char));
  memset(data->buf,0,lunghezza * sizeof(char));
	int n = 0;
	char *aux = data->buf;
	while(lunghezza > 0){

		n = read(fd, aux, lunghezza);

		if(n < 0){

			perror("read");
			fprintf(stdout,"file %s riga %d\n", __FILE__, __LINE__ );
			return -1;

		}
      aux = aux + n;
			lunghezza = lunghezza - n;

	}
		}
    else if(lunghezza == 0){
      data->buf = NULL;
    }
	return 1;
}

/**
 * @function readMsg
 * @brief Legge l'intero messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readMsg(long fd, message_t *msg){

	int valorediritornoHeader = readHeader(fd, &(msg->hdr));
	if(valorediritornoHeader < 0 ) return -1;
	int valorediritornoBody = readData(fd, &(msg->data));
	if(valorediritornoBody < 0 ) return -1;
	return 1;
}

/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendRequest(long fd, message_t *msg){
	int valorediritorno = sendHeader(fd, &(msg->hdr));
	if(valorediritorno < 0){
		fprintf(stdout,"errore sendheader file %s riga %d\n", __FILE__, __LINE__);
		return -1;
	}

	valorediritorno = sendData(fd, &(msg->data));
	if(valorediritorno < 0){
		fprintf(stdout, "errore senddata file%s riga %d\n",__FILE__, __LINE__ );
		return -1;
	}
	return 1;
}

/**
 * @function sendData
 * @brief Invia il body del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendData(long fd, message_data_t *msg){
  int valorediritornoRec = write(fd, &(msg->hdr), sizeof(message_data_hdr_t));
	if (valorediritornoRec < 0 ){
		perror("write valore di ritorno destinatario messaggio");
		fprintf(stdout,"file %s riga %d valorediritornoRec %d\n", __FILE__, __LINE__,valorediritornoRec);
		return -1;
	}

	int lunghezza = msg->hdr.len;
	int n = 0;
	char* aux = msg->buf;
	while(lunghezza > 0){
		n = write(fd, aux, lunghezza);

		if(n < 0){

			perror("write");
			fprintf(stdout,"file %s riga %d\n", __FILE__, __LINE__ );
			return -1;

		}
      aux = aux + n;
			lunghezza = lunghezza - n;

	}

	return 1;
	}


#endif /* CONNECTIONS_C_ */
