#define MAXSIZE 256

#define NUMSTA 9
#define NUMAP 1
#define HEADER_LENGTH 37 /*Lunghezza del header del frame fino al payload*/
#define IS_MC 0
#define IS_APP 1

#define SOCKET_ERROR   ((int)-1)
#define SIZEBUF 1000000

#define NUMPORT 6510

#define DEFAULTCOLOR "\033[0m"
#define ROSSO  "\033[22;31m"
#define VERDE  "\033[22;32m"
#define ARANCIONE  "\033[22;33m"

#define GREEN "\033[0;0;32m"
#define WHITE   "\033[0m"
#define RED "\033[0;0;31m"
#define BLU "\033[0;0;34m"
#define ORANGE "\033[0;0;33m"

typedef struct { 
	char buf[MAXSIZE];
	int first;
	int len;
} buffer;

typedef struct stato {
	int nready; /* risultato select */
	int maxfd;
    	fd_set Rset;
	fd_set Wset;
    	int maxi;
    	int client[FD_SETSIZE];
    	buffer bufs_read[FD_SETSIZE];
	buffer bufs_write[FD_SETSIZE];
    	/* per connect 
    	struct sockaddr_in Serv;
    	char string_remote_ip_address[100];
    	unsigned short int remote_port_number;*/
	struct timeval* timeout;
} Stato;

/*Struttura dati globale contente i parametri delle coppie <stazione, applicazione>*/

typedef struct {
	unsigned long int fd;
	unsigned long int spfd;
} PARAMETRI;

typedef struct {
	PARAMETRI* app;
	PARAMETRI* sta;
} tcb; /*thread control block*/

/*Pacchetto dall'APP alla STA - può andare????*/
typedef struct {
	uint16_t payload_len;
	char addr_dest[6];
	char payload[2000];
} __attribute__ ((__packed__)) local_frame;

typedef struct {
	/*control*/
	uint8_t data;
	uint8_t data_type;
	uint8_t toDS;
	uint8_t fromDS;
	uint8_t RTS;
	uint8_t scan;
	/*fine control*/
	uint8_t duration;
	uint16_t pkt_len;
	/*addresses*/
	char addr1[6];
	char addr2[6];
	char addr3[6];
	char addr4[6];
	/*fine addresses*/
	uint32_t seqctrl;
	char payload_CRC[2001];
} __attribute__ ((__packed__)) frame;
