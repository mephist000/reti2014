/*COMPILAZIONE CON: gcc -lpthread*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

/*file con define e struct*/
#include "./thread.h"

#include "./function.c"

/*#define DEBUG*/


tcb* TABLE;
pthread_mutex_t mutexdata;
pthread_cond_t sync_cond;
pthread_cond_t sta_cond;
int sync_count = 0;

char mac_addr[NUMSTA+NUMAP][6]; /*matrice di indirizzi mac*/

/*FUNZIONI RELATIVE ALLA SELECT*/
/*
* ACK
* type_FLAG = 0
* payload = NULL
* lenght = 0
* mit, dest != NULL
*
* PACCHETTO NORMALE
* type_FLAG = 1
* payload != NULL
* lenght != 0
* mit, dest != NULL
*/

frame* MC_packet(char* payload, char* dest, int length, int type_FLAG);

void check_for_recv_send( Stato *pS ) {

	int i, n;

	for (i = 0; (i<=pS->maxi)&&(pS->nready>0); i++)  /* check all clients for data */
	{
		int sockfd;

		if ( (sockfd= pS->client[i]) < 0)
			continue;

		if (FD_ISSET(sockfd, &(pS->Rset))) { /* to be received */
			do {
		    		n = read(sockfd, pS->bufs_read[i].buf, 6);
			} while((n<0) && (errno==EINTR) );

			if(n==0) {
				/*connection closed by client */
				close(sockfd);
				pS->client[i] = -1;
		    	} else if(n<0) {
				/* connection error */
				printf("error detected from socket %d\n", sockfd);
				close(sockfd);
				pS->client[i] = -1;
			} else {
				pS->bufs_read[i].buf[n]=0;
				printf(ROSSO "received from socket %d %s\n" DEFAULTCOLOR , n, pS->bufs_read[i].buf);
				pS->bufs_read[i].len=n;
				pS->bufs_read[i].first=0;
			}
   			pS->nready--;
			continue; /* se dovevo leggere non devo spedire */
		}

		if (FD_ISSET(sockfd, &(pS->Wset))) { /* to be sent */
			do {
		    		n = send(sockfd, &(pS->bufs_write[i].buf[pS->bufs_write[i].first]), pS->bufs_write[i].len, MSG_NOSIGNAL|MSG_DONTWAIT );
			} while((n<0) && (errno==EINTR) );

			if(n<0) {
				/* connection error */
				printf("error detected from socket %d\n", sockfd);
				close(sockfd);
				pS->client[i] = -1;
			} else {
				printf("bytes sent using socket %d\n", n);
				pS->bufs_write[i].len -= n;
				if(pS->bufs_write[i].len==0)  {
					pS->bufs_write[i].first = 0;
					printf("packet sent back to the client using socket %d\n", sockfd);
				}
				else
					pS->bufs_write[i].first += n;
			}
			pS->nready--;
		}
		    
	} /* fine for(i=0,...) */
}

void init_stato (Stato *pS) {
	int i;
	pS->timeout = NULL;
	pS->maxi = -1; /* index into client[] array and into Rset and Wset */
	for (i = 0; i < FD_SETSIZE; i++) {
	    	pS->client[i] = -1; /* -1 indicates available entry */
		pS->bufs_read[i].len=0; /* no bytes */
		pS->bufs_read[i].first=0;
		pS->bufs_write[i].len=0; /* no bytes */
		pS->bufs_write[i].first=0;
	}
}

void setup_before_select(Stato *pS) {

	int i;

	FD_ZERO( &(pS->Rset) );
	FD_ZERO( &(pS->Wset) );
	pS->maxfd=pS->client[0];

	for (i=0; i<=pS->maxi; i++) {
		if( pS->client[i] >= 0 ) {
			if( pS->bufs_write[i].len==0 )  /* nulla da spedire, metto in lettura */
				FD_SET(pS->client[i], &(pS->Rset) );
			else /* roba da spedire, setto in scrittura */
				FD_SET(pS->client[i], &(pS->Wset) );
			if( pS->maxfd < pS->client[i] ) 
				pS->maxfd=pS->client[i];
		}
	}
}

/*FUNZIONI DI INIZIALIZZAZIONE STRUTTURE DATI*/
void mac_addr_initialize() {
	/*NB: PRESUPPONE ARCHITETTURA LITTLE-ENDIAN*/
	char mac_base[] = "114d4f4a445Z"; /*indirizzo fittizio: la Z finale viene sostituita dall'indice della sta/ap*/
	int i, j;
	long long unsigned int mac_lli;
	char* mac_lli_char;
	for (i = 0; i < NUMSTA+NUMAP; i++) {
			mac_base[11] = (char)(i + 48); /*offset della ASCII-table, conversione da intero a carattere*/
			mac_lli = strtoll(mac_base, NULL, 16); /*la stringa viene interpretata come un hex int*/
			mac_lli_char = (char*)(&mac_lli); /*l'intero viene interpretato come una sequenza di byte*/
			for (j = 0; j < 6; j++) { /*memorizzazione nella struttura dati globale, al contrario (little-endian)*/
				mac_addr[i][j] = mac_lli_char[5-j];
			}
	}
}

int findIndex(int ind) {
	int i;
	/*IF IND MINORE DI 100? - immagino fosse debugging*/
	if (ind < 100) { /*sincronizzazione iniziale*/ /*TODO: modificare flag per la find*/
		printf("APP %d CHIEDE MUTEX\n", ind);
		pthread_mutex_lock(&mutexdata);
		printf("APP %d HA OTTENUTO MUTEX\n", ind);
		if (sync_count < NUMSTA) {
			printf("APP %d RILASCIA MUTEX, VA IN WAIT\n", ind);
			pthread_cond_wait(&sync_cond, &mutexdata); /**/
			pthread_cond_signal(&sync_cond);
		}
		else    {
        	/* l'ultimo arrivato, quando il contatore e' == NUMSTA */
			pthread_cond_signal(&sync_cond);
		}
		printf("APP %d RILASCIA MUTEX\n", ind);
		pthread_mutex_unlock(&mutexdata);
	}
	/*Recupero dell'ID del thread chiamante*/
	pthread_t myID = pthread_self();
	/*Restituzione indice della coppia <sta, app> che contiene quell'ID*/
	for (i = 0; i < NUMSTA; i++) {
		if (TABLE[i].sta && ((TABLE[i].sta)->fd == myID)) return i;
		if (TABLE[i].app && ((TABLE[i].app)->fd == myID)) return i;
	}
	printf(VERDE "ERRORE: indice del thread non trovato.\n" DEFAULTCOLOR); /*caso anomalo*/
	return -1; /**/
	/*exit(0);*/
}

/*FUNZIONI PRINCIPALI*/
int Send(char* macDest, char* buffer, int length, int i){
	int index = findIndex(i);
	unsigned long int sp = TABLE[index].app->spfd;
	printf(BLU "PRIMA DELLA sEND" DEFAULTCOLOR);
	return send(sp, buffer, length, MSG_NOSIGNAL);
}

/*CODICI APPLICAZIONI/STA/AP*/
void *app1(void *index) {

	char ch[MAXSIZE] = "HODOR";

	printf("APP %d STA FACENDO LA SEND\n", *(int*)index);

	Send(ch, ch, 6, *(int*)index);

	printf("APP %d HA FINITO LA SEND\n", *(int*)index);

	/*do {
		ris2 = read(fd, &ch, 6);
	} while((ris2<0) && (errno == EINTR));
	*/
	printf("APP %d STA PER TERMINARE\n", *(int*)index);
	return NULL;
}

void *sta1(void *index) {
	int rc;
	PARAMETRI *par;
	pthread_t *app;
	int fds[2];
	int ris, ris2;
	char ch[MAXSIZE];
	struct sockaddr_in SLocal, Serv;
	char string_remote_ip_address[100] = "127.0.0.1";
	short int remote_port_number;
	int Ssocketfd, OptVal, msglen, Fromlen, blockIndex;
	int n, i, nread, nwrite, len, j;
	char buf[MAXSIZE];
	char msg[MAXSIZE];
	int APPindex = *(int*)index;
	int myindex = APPindex;
	PARAMETRI* STA;
	Stato S_STA;
	/*struct sockaddr_in Con[2];  applicazione e MC*/

	remote_port_number = NUMPORT;

	ris = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
	
	printf("STAZIONE %d PARTE\n", myindex);

	Ssocketfd = socket(AF_INET, SOCK_STREAM, 0);

	/* avoid EADDRINUSE error on bind() */
	OptVal = 1;
	ris2 = setsockopt(Ssocketfd, SOL_SOCKET, SO_REUSEADDR, (char*)&OptVal, sizeof(OptVal));

	memset (&SLocal, 0, sizeof(SLocal) );
	SLocal.sin_family = AF_INET;
	/* indicando INADDR_ANY viene collegato il socket all'indirizzo locale IP     */
	/* dell'interaccia di rete che verrà utilizzata per inoltrare i dati          */
	SLocal.sin_addr.s_addr =	htonl(INADDR_ANY);         /* wildcard */
	SLocal.sin_port = htons(0);
	ris2 = bind(Ssocketfd, (struct sockaddr*) &SLocal, sizeof(SLocal));


	memset ( &Serv, 0, sizeof(Serv) );
	Serv.sin_family = AF_INET;
	Serv.sin_addr.s_addr = inet_addr(string_remote_ip_address);
	Serv.sin_port =	htons(remote_port_number);

	ris = connect(Ssocketfd, (struct sockaddr*) &Serv, sizeof(Serv));
	if (ris == -1) perror("ERRORE CONNECT:");
	/*TODO: CONTROLLO ERRORE*/

	while ( n=read(Ssocketfd, &(buf[0]), 1 )<1);
	printf(VERDE "STAZIONE %d RICEVE ACK!" DEFAULTCOLOR, myindex);

	/*CREAZIONE E SINCRONIZZAZIONE APP*/
	/*Mutua esclusione*/

	/*Inizializzazione campi struttura degli indici globale della stazione e dell'app*/
	TABLE[*((int*)index)].sta = (PARAMETRI*)malloc(sizeof(PARAMETRI));
	STA = TABLE[*((int*)index)].sta;
	STA->fd = pthread_self();
	STA->spfd = fds[0];
	TABLE[*((int*)index)].app = (PARAMETRI*)malloc(sizeof(PARAMETRI));
	TABLE[*((int*)index)].app->spfd = fds[1];
	app = &(TABLE[*((int*)index)].app->fd);
	*app = 0;
	
	rc = pthread_create(app, NULL, app1, &APPindex);

	printf("STAZIONE %d CHIEDE MUTEX\n", myindex);
	pthread_mutex_lock(&mutexdata);
	sync_count++;
	if (sync_count == NUMSTA) pthread_cond_signal(&sync_cond);
	printf("STAZIONE %d RILASCIA MUTEX\n", myindex);
	pthread_mutex_unlock(&mutexdata);
	/*TODO: CONTROLLO ERRORE*/

	printf("STAZIONE %d SUPERA IL CHECKPOINT\n", myindex);

	/*INIZIO CODICE STA: SELECT*/

	init_stato(&S_STA);

	S_STA.client[0] = Ssocketfd;
	S_STA.client[1] = fds[0];

	printf("\n\n %d %d \n\n", S_STA.client[0], S_STA.client[1]);

	S_STA.maxi = 2;

	j = 0;
	while (j < (NUMSTA*3)) {
		do {
			setup_before_select(&S_STA);
			/* solo per debug */
			printf("PRIMA DI SELECT");
			printf(" maxfd+1= %d  Rset= ", S_STA.maxfd+1);
			for (i=0; i<FD_SETSIZE; i++) {
				if ( FD_ISSET( i, &(S_STA.Rset) ) ) { printf("%d ", i); } 
			}
			printf("   Wset= ");
			for (i=0; i<FD_SETSIZE; i++) { 
				if ( FD_ISSET( i, &(S_STA.Wset)) ) { printf("%d ", i); }
			}
			printf("\n");
			/* fine solo per debug */
			S_STA.nready = select(S_STA.maxfd+1, &(S_STA.Rset), &(S_STA.Wset), NULL, NULL);
			/* solo per debug */
			/*myerrno=errno;*/
			printf("nready=%d\n", S_STA.nready);
			/*errno=myerrno;*/
			/* fine solo per debug */
		} while( (S_STA.nready<0) && (errno==EINTR) );
		check_for_recv_send(&S_STA);
		/*sleep(1);*/
		j++;
	}


	do {
		ris2 = read(STA->spfd, &msg, 6);
	} while((ris2<0) && (errno == EINTR));
	printf("STAZIONE %d RICEVE %s DALLA SUA APP\n", myindex, msg);


	/*INIVIO A MC*/
	len = strlen(msg)+1;
	nwrite = 0;
	fflush(stdout);
	printf(ARANCIONE "STAZIONE %d INVIA A MC \n" DEFAULTCOLOR , myindex);
	while( (n = write(Ssocketfd, &(msg[nwrite]), len-nwrite)) > 0 ) nwrite += n;

	nread=0;
	fflush(stdout);
	/*while( (len>nread) && ((n=read(Ssocketfd, &(buf[nread]), len-nread )) >0)) {
		nread += n;
		fflush(stdout);
	}*/

	/*RICEZIONE DA MC
	printf("STAZIONE %d RICEVE DA MC: %s\n", myindex, buf);

	for (i = 0; i < nread; i++) send(fds[0], &(buf[i]), 1, MSG_NOSIGNAL);*/

	close(Ssocketfd);

	pthread_join(*app, (void*) NULL);
	printf("APP %d TERMINATA\n", myindex);
}
















int main(int argc,char** argv) {

	struct sockaddr_in Local, Cli[NUMSTA+NUMAP];
	short int local_port_number;
	int socketfd, newsocketfd[NUMSTA+NUMAP], OptVal, ris, i, j;
	unsigned int len[NUMSTA+NUMAP];
	int n, nread, nwrite, rc;
	char buf[MAXSIZE];
	PARAMETRI *par;
	pthread_t vthreads[NUMSTA+NUMAP];
	Stato S;
	
	int Tindex[NUMSTA];

	/*Inizializzazione della struttura dati che contiene gli indirizzi mac (fittizzi) delle stazioni/ap*/
	/*ogni stazione/ap accede al proprio indirizzo attraverso il proprio indice*/
	/*NB: PRESUPPONE ARCHITETTURA LITTLE-ENDIAN*/
	mac_addr_initialize();	
	
	/*DEBUG - controllo indirizzi mac*/
	for (i = 0; i < NUMSTA+NUMAP; i++) {
		printf("INDIRIZZO n%d:\t", i);
		for (n = 0; n < 6; n++) {
			printf("%hhx", mac_addr[i][n]);
		}
		printf("\n");
	}
	/*fine debug indirizzi mac*/

	#ifdef DEBUG

	pthread_mutex_init(&mutexdata, NULL);
	pthread_cond_init(&sync_cond, NULL);

	/*Inizializzazione strutture dati*/
	
	TABLE = (tcb*)calloc(NUMSTA, sizeof(tcb));

	/*TCP CONNECTION*/

	local_port_number = NUMPORT;

	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	/*if (socketfd == SOCKET_ERROR) {
		printf ("socket() failed, Err: %d \"%s\"\n", errno,strerror(errno));
		exit(1);
	}*/

	memset(&Local, 0, sizeof(Local));
	Local.sin_family = AF_INET;
	/* indicando INADDR_ANY viene collegato il socket all'indirizzo locale IP     */
	/* dell'interaccia di rete che verrà utilizzata per inoltrare il datagram IP  */
	Local.sin_addr.s_addr =	htonl(INADDR_ANY);         /* wildcard */
	Local.sin_port = htons(local_port_number);
	ris = bind(socketfd, (struct sockaddr*)&Local, sizeof(Local));
	/*if (ris == SOCKET_ERROR) {
		printf ("bind() failed, Err: %d \"%s\"\n",errno,strerror(errno));
		exit(1);
	}*/

	ris = listen(socketfd, NUMSTA+NUMAP);
	if (ris == SOCKET_ERROR) {
		printf ("listen() failed, Err: %d \"%s\"\n",errno,strerror(errno));
		exit(1);
	}

	/*THREADS CREATION - STATIONS*/

	par = (PARAMETRI*)malloc(sizeof(PARAMETRI));

	/*if(!pPar) {
		perror("malloc failed: ");
		exit(0);
	}*/

	par->fd = 0;

	/*if (rc){
		printf("ERROR; return code from pthread_create() is %d\n",rc);
		exit(-1);
	}
	else
		printf("Created thread JOINABLE ID %d\n", (int) vthreads[0] );*/
	

	for (i = 0; i < NUMSTA; i++) {
		Tindex[i] = i;
		rc = pthread_create(&(vthreads[i]), NULL, sta1, &(Tindex[i]));
		/*sleep(1);*/
		if (rc) {
			printf("ERROR; return code from pthread_create() is %d\n",rc);
			exit(-1);
		}
	}

	init_stato(&S);

	for (i = 0; i < NUMSTA; i++) {
		len[i] = sizeof(Cli[i]);
		do {
			memset(&(Cli[i]), 0, sizeof(Cli[i]));
			S.client[i] = accept(socketfd, (struct sockaddr*) &(Cli[i]), &(len[i]));
			printf(ROSSO "client iesimo %d\n" DEFAULTCOLOR, S.client[i]);
		} while ((S.client[i] < 0) && (errno == EINTR));
		if (S.client[i] > 0){
			S.maxi++;
			/*SINCRONIZZAZIONE CON STAZIONI*/
			buf[0] = 'a';
			n = 0;
			do {
			n = send(S.client[i], buf, 1, MSG_NOSIGNAL);
			} while(n <= 0);/*RIPROVA FINCHE' NON RIESCE A INVIARE L'ACK*/
			n = 0;
		}
	}

	/*INIZIO MEZZO CONDIVISO*/

	/*SELECT*/
	j = 0;
	while (j < (NUMSTA*3)) {
		do {
			setup_before_select(&S);
			/* solo per debug */
			/*printf("PRIMA DI SELECT");
			printf(" maxfd+1= %d  Rset= ", S.maxfd+1);*/
			for (i=0; i<FD_SETSIZE; i++) {
				if ( FD_ISSET( i, &(S.Rset) ) ) { printf("%d ", i); } 
			}
			/*printf("   Wset= ");
			for (i=0; i<FD_SETSIZE; i++) { 
				if ( FD_ISSET( i, &(S.Wset)) ) { printf("%d ", i); }
			}*/
			printf("\n");
			/* fine solo per debug */
			S.nready = select(S.maxfd+1, &(S.Rset), &(S.Wset), NULL, NULL);
			/* solo per debug */
			/*myerrno=errno;*/
			printf("nready=%d\n", S.nready);
			/*errno=myerrno;*/
			/* fine solo per debug */
		} while( (S.nready<0) && (errno==EINTR) );
		check_for_recv_send(&S);
		/*sleep(1);*/
		j++;
	}

	/*while ((n = read(newsocketfd, &(buf[nread]), MAXSIZE)) > 0) {
		printf("letti %d bytes    tot=%d\n", n, n+nread);
		fflush(stdout);
		nread += n;
		if(buf[nread-1]=='\0') break; /* fine stringa *//*
	}*/
	/*
	for(n = 0; n < nread - 1; n++) buf[n] = buf[n]+2;

	printf("MC TRASLA: %s\n", buf);

	nwrite = 0;
	fflush(stdout);

	while((n = write(newsocketfd, &(buf[nwrite]), nread - nwrite)) > 0) nwrite += n;

	/* chiusura */
	/*close(newsocketfd);*/

	/*ATTESA TERMINAZIONE STAZIONI*/
	pthread_join(vthreads[0], (void*) NULL);
	pthread_join(vthreads[1], (void*) NULL);

	close(socketfd);

	printf("STAZIONI TERMINATE\n");

	pthread_mutex_destroy(&mutexdata);
	pthread_cond_destroy(&sync_cond);


	#endif
	return 0;
}
