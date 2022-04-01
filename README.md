# Progetto-Sistemi-operativi
Progetto universitario Sistemi Operativi anno accademico 2018-2019


Lo scopo del progetto e lo sviluppo in C di un server concorrente che implementa una chat. Gli utenti della chat possono scambiarsi messaggi testuali e/o files collegandosi al server con un programma client. Un messaggio testuale e una stringa non vuota avente una lunghezza massima stabilita nel le di congurazione del server (vedi Sez. 2.2). Un file di qualsiasi tipo può essere scambiato tra due client purchè la size non sia superiore a quanto stabilito nel le di congurazione del server (vedi Sez. 2.2). Ogni utente della chat è identificato univocamente da un nickname alfanumerico. L'utente si connette al server, invia richieste e riceve risposte utilizzando un client. Uno o più processi client comunicano con il server per inviare richieste di vario di tipo. Le richieste gestite dal server sono le seguenti:


1. registrazione di un nuovo utente (ossia registrazione di un nuovo nickname) (REGISTER OP);
2. richiesta di connessione per un utente già registrato (CONNECT OP);
3. invio di un messaggio testuale (POSTTXT OP) ad un nickname;
4. invio di un messaggio testuale a tutti gli utenti registrati (POSTTEXTALL OP);
5. richiesta di invio di un file (POSTFILE OP) ad un nickname;
6. richiesta di recupero di un file inviato da un altro nickname (GETFILE OP);
7. 7. richiesta di recupero degli ultimi messaggi inviati al client (GETPREVMSGS OP);
8. richiesta della lista di tutti i nickname connessi (USRLIST OP);
9. richiesta di deregistrazione di un nickname (UNREGISTER OP);
10. richiesta di disconnessione del client (DISCONNECT OP).


Per ogni messaggio di richiesta il server risponde al client con un messaggio di esito dell'operazione richiesta e con l'eventuale risposta. Ad esempio, se si vuole inviare un messaggio ad un utente non esistente (cioè il cui nickname non è stato mai registrato), il server risponderà con un messaggio di errore opportunamento codificato (vedere file ops.h nel kit del progetto) oppure risponderà con un messaggio di successo (codificato con il codice OP OK { vedere il file ops.h contenente i codici delle operazioni di richiesta e di risposta). Il sistema deve prevedere la creazione e gestione di gruppi di utenti. Un gruppo ha un nome (groupname) e più utenti possono essere registrati ad un gruppo. Sono implementate le seguente richieste:

1. richiesta di creazione di un gruppo (CREATEGROUP OP) con un dato nome (groupname);
2. richiesta di aggiunta di un nickname registrato ad un groupname (ADDGROUP OP);
3. richiesta di cancellazione di un nickname registrato da un groupname (DELGROUP OP);

Un qualsiasi utente può creare un nuovo gruppo che non sia stato già registrato. Solamente l'utente che ha creato il gruppo può cancellare un groupname. All'atto della creazione di un gruppo, il nickname che ne fa richiesta, è automaticamente aggiunto al gruppo. Altri utenti possono aggiungersi e cancellarsi a/da un gruppo in
ogni momento con l'operazione ADDGROUP OP e DELGROUP OP. Solo un utente registrato al gruppo può inviare e ricevere messaggi al/dal gruppo. E' possibile inviare agli utenti di un gruppo sia messaggi testuali che file di qualsiasi tipo con le stesse regole e restrizioni viste in precedenza per gli utenti.
La comunicazione tra client e server avviene tramite socket di tipo AF UNIX. Il path da utilizzare per la creazione del socket AF UNIX da parte del server, deve es-
sere specificato nel file di configurazione utilizzando l'opzione UnixPath (vedi Sez. 2.2).

L'obiettivo è minimizzare possibili interazioni indesiderate fra esecuzioni diverse sulla stessa macchina del laboratorio. Nello sviluppo del codice del server si è tenuto in considerazione che il server deve essere in grado di gestire efficientemente fino ad alcune centinaia di connessioni di client contemporanee senza che questo comporti un rallentamento evidente nell'invio e ricezione dei messaggi/files dei clients. Inoltre il sistema è stato progettato per gestire alcune decine di migliaia di utenti registrati alla chat (questo aspetto ha impatto sulle strutture dati utilizzate per memorizzare le informazioni).
Il server al suo interno implementa un sistema multi-threaded in grado di gestire concorrentemente sia nuove connessioni da nuovi client che gestire le richieste su connessioni già stabilite. Un cliente può sia inviare una sola richiesta per connessione oppure, inviare più richieste sulla stessa connessione (ad esempio: postare un messaggio ad un utente, inviare un file ad un nickname, inviare un messaggio a tutti gli utenti, recuperare file inviati da altri client, etc...). Il numero massimo di connessioni concorrenti che il server chatterbox gestisce è stabilito dall'opzione di configurazione MaxConnections. Se tale numero viene superato, dovrà essere ritornato al client un opportuno messaggio di errore (vedere la codifiche dei messaggi di errore nel file ops.h). Il numero di thread utilizzati per gestire le connessioni è specificato nel file di configurazione con l'opzioni ThreadsInPool. Tale numero specifica la dimensione di un pool di threads, sempre attivi per tutta la durata dell'esecuzione del server e che il server chatterbox utilizzerà per gestire le connessioni dei client. Tale valore non può essere 0.
Un thread appartenente al pool gestisce una sola richiesta alla volta inviata da uno dei client. Non appena conclude la gestione della richiesta, controlla se ci sono
altre richieste da servire dalla coda delle richieste ed in caso affermativo ne prende una da gestire. Se non ci sono richiesta da servire, il thread si mette in attesa di essere svegliato per gestire una nuova richiesta.
Non appena il server accetta una nuova connessione da un client che ha inviato una richiesta di CONNECT, il server invia al client la lista di tutti gli utenti connessi in quel determinato istante (il formato della lista inviata dal server al client è lo stesso del messaggio di risposta alla richiesta USRLIST OP).
Il server può essere terminato in ogni momento della sua esecuzione inviando un segnale di SIGINT, SIGTERM o SIGQUIT (vedi Sez. 2.2).

**2.2 Esecuzione e file di configurazione**
Il server chatterbox viene attivato da shell con il comando
$ ./chatterbox -f chatty.conf
dove chatty.conf fie il file di configurazione contenente tutti i parametri relativi alla configurazione del server. Il formato del file di configurazione è schematizzato in Fig. 1,
_path utilizzato per la creazione del socket AF_UNIX_


UnixPath = /tmp/chatty_socket


_numero massimo di connessioni concorrenti gestite dal server_


MaxConnections = 32


_numero di thread nel pool_


ThreadsInPool = 8


_dimensione massima di un messaggio testuale (numero di caratteri)_


MaxMsgSize = 512


_dimensione massima di un file accettato dal server (kilobytes)_


MaxFileSize = 1024


_numero massimo di messaggi che il server 'ricorda' per ogni client_


MaxHistMsgs = 16


_directory dove memorizzare i files da inviare agli utenti_


DirName = /tmp/chatty


_file nel quale verranno scritte le statistiche_


StatFileName = /tmp/chatty_stats.txt


Figure 1: Formato del file testuale chatty.conf
Nel momento in cui un client si connette al server (CONNECT OP) invia il proprio nickname. Se il nickname non è stato precedentemente registrato con una operazione di
(REGISTER OP) il server non accetta il client tra quelli che possono inviare messaggi e ritorna un messaggio di errore ( vedi il file ops.h nel kit del progetto). Se il nickname è registrato, il server risponde al client con la lista di tutti gli utenti in quel momento connessi. La lista degli utenti connessi viene inviata dal server al client anche all'atto della registrazione di un nickname dato che l'operazione di registrazione implica che il client sia connesso. Se il cliet, con nickname1 vuole inviare un file ad un altro client con nickname2, allora preparerà una richiesta di tipo POSTFILE OP. Il server, riceve il file e lo memorizza nella directory specificata nel file di configurazione con il nome DirName ed informa nickname2 che è disponibile per il download un file inviato da nickname1. La notifica avviene inviando come identificatore il nome del file. Se nickname2 vuole recuperare il file dovrà inviare una richiesta GETFILE OP inviando nella richiesta il nome del file ricevuto in precedenza.

I messaggi possono essere notificati anche a client non connessi al server. Se ad esempio un client vuole inviare un messaggio a tutti gli utenti registrati (POST-
TEXT ALL), gli utenti che risultano connessi al server al momento dell'arrivo del messaggio (cioè sono online), riceveranno il messaggio immediatamente", mentre gli
utenti che non sono connessi al server potranno ricevere tali messaggi solo dietro esplicita richiesta di tipo GETPREVMSGS OP. Il server chatterbox memorizza una history di al più MaxHistMsgs messaggi (testuali o files) per ogni client registrato. I messaggi vanno sempre memorizzati nella history, anche se il messaggio viene consegnato al destinatario (il concetto di history è lo stesso del comando bash history).Un client può in ogni momento, oltre ad inviare messaggi testuali e file, disconnettersi (DISCONNECT OP), chiedere la lista degli utenti connessi (USRLIST OP),deregistrarsi (UNREGISTER OP) ed in quest'ultimo caso tutti i messaggi pendenti
e non ancora inviati al client destinatario dovranno essere eliminati. La disconnessione avviene quando il client chiude la connessione verso il server.

**2.3 Gestione dei segnali**
Oltre ai segnali SIGINT o SIGTERM o SIGQUIT che determinano la terminazione di chatterbox , il server gestisce il segnale SIGUSR1. Se viene inviato un segnale di tipo
SIGUSR1 il server stampa le statistiche correnti nel file associato all'opzione StatFileName presente nel file di configurazione secondo il formato descritto nella sezione seguente. Se tale opzione è vuota o non è presente, il segnale ricevuto viene ignorato.

**2.4 Statistiche**
Alla ricezione del segnale SIGUSR1 il server appende nel file associato all'opzione StatFileName le statistiche del server. Le stampa nel file ha il formato
<timestamp - statdata> dove timestamp è l'istante temporale in cui le statistiche vengono inserite nel file e statdata è una sequenza di numeri (separati da uno spazio) che riportano i valori monitorati durante il tempo di vita del server. I dati statistici prodotti nel file sono riportati in Fig. 2.
n. di utenti registrati
n. di clienti online
n. di messaggi consegnati
n. di messaggi da consegnare
n. di file consegnati
n. di file da consegnare
n. di messaggi di errore
Figure 2: Dati statistici prodotti nel file associato all'opzione StatFileName
