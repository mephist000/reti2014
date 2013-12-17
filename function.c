/*Incluso function.c in thread02-bis.c, tolti gli include superflui*/


/*extern int findIndex(int ind);*/
extern char mac_addr[NUMSTA+NUMAP][6];

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


/*Funzione di impacchettamento da STA a MC, quindi:
	~ toDS = 1, fromDS = 0
	~ data = 1, datatype dipende (datatype==ACK_FLAG => il pacchetto e' un ack)
	~ scan = 0
	~ RTS = 0
*/


/*
	
indirizzi

• Address 1
– Recipient address
– if ToDS=0, then end station’s address
– if ToDS=1, BSSID (if FromDS=0) or bridge (if FromDS=1)
• Address 2
– Transmitter address
– if FromDS=0, then source station’s address
– If FromDS=1, BSSID (if ToDS=0) or bridge (if ToDS=1)
• Address 3
– If FromDS=ToDS=0, BSSID
– If FromDS=0, ToDS=1, final destination address
– If FromDS=1, ToDS=1, final destination address
• Address 4
– Original source address
– Set only when a frame is transmitted from one AP to
another, i.e., if FromDS=ToDS=1

*/

/*PROBLEMA SEQCTRL
Se il payload e' piu' grande della dimensione massima, va diviso in piu' pacchetti: deve farlo questa funzione? in questo caso dovrebbe restituire piu' di un frame, e l'invio dovrebbe essere in un ciclo*/

frame* MC_packet(char* payload, char* dest, int length, int type_FLAG) {

	frame* packet = (frame*)malloc(sizeof(frame));
	int duration, i;
	int index = findIndex(1); /*ricordiamo di rimuovere quell'intero dato a findindex che era per debug*/
	
	packet->data = 1;
	packet->data_type = type_FLAG;
	packet->toDS = 1;
	packet->fromDS = 0;
	packet->RTS = packet->scan = 0;
	
	if (length < 100) duration = 5; /*meno di 100 byte, durata 5 ms*/
	else duration = 20;

	packet->duration = duration;
	packet->pkt_len = length;

	/*addr1 = in questo caso (toDS = 1), l'indirizzo dell'AP*/
	for (i = 0; i < 6; i++) {
		packet->addr1[NUMSTA+NUMAP] = mac_addr[index][i];
	}
	/*addr2 = indirizzo della STA che sta trasmettendo*/
	for (i = 0; i < 6; i++) {
		packet->addr2[i] = mac_addr[index][i];
	}
	/*addr2 = destinazione finale*/
	for (i = 0; i < 6; i++) {
		packet->addr3[i] = dest[i];
	}	
	/*addr4 = da settare solo in comunicazione diretta tra nodi, quindi mai nel nostro caso*/
	for (i = 0; i < 6; i++) {
		packet->addr4[i] = 0;
	}
	/*	???
	seqctrl
	CRC - immagino vada settato inizialmente a 1
	*/
	for (i = 0; i < length; i++) {
		packet->payload_CRC[i] = payload[i];
	}
	return packet;
}




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
				Packet = MC_packet(NULL, packet_out->addr4, 0, ACK_FLAG);
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
