#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include "thread.h"
extern frame* MC_packet(char* payload, char* dest, int lenght, int type_FLAG);
extern char** mac_addr;
extern int findIndex(int ind);
/*void one_recv_send(Stato *pS) {

	for (i = 0; (i <= pS->maxi)&&(pS->nready > 0); i++) { /* check all clients for data */

/*		int sockfd;
		
		if (FD_ISSET(sockfd, &(pS->Rset))) { /*e' in lettura*/
/*			do {
		    		n = read(sockfd, pS->bufs[i].buf, 6);
			} while((n<0) && (errno==EINTR) );
			/*TODO: CONTROLLO CRC, ADDRESS*/
/*			pS->nready--;
			continue; /* se dovevo leggere non devo spedire */
/*		}
		if (FD_ISSET(sockfd, &(pS->Wset))) { /*e' in scrittura*/
/*		}
}


//--------------------------------------------------------------------------------------------
/*Funzione che dato il pacchetto ricevuto dal MC lo controlla(CRC, dest, ACK....)*
/*TEST MC
passiamo un pacchetto compilato a mano - addr1,add3 = addr_sta0 
	1) index_crc deve accedere all'indice giusto ->1pacchetto con tutto payload = 0 tranne l'ultima pos = 1. (crc) 
		deve entrare nell'if
	2) solo la STA0 deve passare il controllo - 2finti thread la chiamano entrambi
	3) l'app deve ricevere il payload - stampa del contenuto del buffer dopo strncpy
	4)MC deve ricevere ack - stampa del pacchetto prima della strncpy e dopo

TEST APP
stampa del contenuto del buffer dopo strncpy
*/
void read_handler(int* msg, Stato *pS, int FLAG) 
{
	frame* Packet;
	frame* packet_out;
	local_frame* packet_local;

	if(FLAG == IS_MC) 
	{
		packet_out = (frame *)msg;
		/*se il CRC indica che il pacchetto non é corrotto*/
		short int index_crc = (short int)(*packet_out).pkt_len - HEADER_LENGTH - 1;/*indice del vettore in cui si trova 
									il CRC ( e dimensione finale del payload)*/
		/*se il CRC indica che il pacchetto non é corrotto*/
		if(packet_out->payload_CRC[index_crc] != 0)
		{
		
			/*Trovo il mio indice*/
			int index = findIndex(1);
	
			/*Controllo indirizzi ->(1 = me)(1 = 3) - devono essere entrambe vere(strncmp = 0)*/
			if(!(strncmp(packet_out->addr1, mac_addr[index], 6) || strncmp(packet_out->addr1, packet_out->addr3, 6)))
			{/*Gli indirizzi sono uguali - il pacchetto é per me- piglio il payload e setto il buffer per la scrittura in app*/
				buffer * Dest = pS->bufs_write;
				/*setto il buffer per la scrittura all' app*/
				strncpy(&(Dest[1].buf[(Dest[1].first + Dest[1].len)]), packet_out->payload_CRC, index_crc);
				Dest[1].len += index_crc;
				/*creo il pacchetto di ACK*/
				Packet = MC_packet(NULL, packet_out->addr4, 0, 0);
				/*setto il buffer per la scrittura all'MC*/
				strncpy(&(Dest[0].buf[(Dest[0].first + Dest[0].len)]), (char*)Packet, Packet->pkt_len);
				Dest[0].len += Packet->pkt_len;
			}
		}
	}
	if(FLAG == IS_APP) 
	{
		packet_local = (local_frame *)msg;
		/*Trovo il mio indice*/
		int index = findIndex(1);
		buffer * Dest = pS->bufs_write;
		/*creo il frame da inviare all' MC*/
		Packet = MC_packet(packet_local->payload, packet_local->addr_dest, packet_local->payload_len, 1);
		/*setto il buffer per la scrittura all'MC*/
		strncpy(&(Dest[0].buf[(Dest[0].first + Dest[0].len)]), (char*)Packet, Packet->pkt_len);
		Dest[0].len += Packet->pkt_len;
			
		
	}


}
