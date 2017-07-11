#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#define RIGHE 6
#define COLONNE 7

/*---------------------------*/
struct Buffer {
	char* invio;
	char* ricezione;
	int byteInviati,byteDaInviare;
	int byteRicevuti,byteDaRicevere;
};
struct Client {
	char ip[20];
	char* username;
	int portaTCP,portaUDP;
	int sockTCP,sockUDP;
	struct sockaddr_in addressUdp;
	
	char simbolo;
	int libero;
	int qualeDato;
	struct Buffer* buffer;
	
	struct Client* next;
	struct Client* sfidante;
};
struct Server {
	char ip[20];
	int portaTCP;
	int qualeDato;
	struct Buffer* buffer;	
};
struct Server* server;
struct Client* sfidante;

int sock;
char mioUsername[40];
int portaUDP;
struct sockaddr_in address;
struct sockaddr_in addressUdp;


int fd_max;
fd_set principale,copia;

int stato_partita;
int turno;
char stato[2]={'>','#'};
int posizioneLibera[COLONNE]={ RIGHE-1,RIGHE-1,RIGHE-1,RIGHE-1,RIGHE-1,RIGHE-1,RIGHE-1};
char griglia[RIGHE][COLONNE]={ {' ',' ',' ',' ',' ',' ',' '}, 
																{' ',' ',' ',' ',' ',' ',' '},
																{' ',' ',' ',' ',' ',' ',' '},
																{' ',' ',' ',' ',' ',' ',' '},
																{' ',' ',' ',' ',' ',' ',' '},
																{' ',' ',' ',' ',' ',' ',' '}, };

char* comando[9]={"nick","port","!help","!who","!connect","!disconnect","!quit","!show_map","!insert"};
char* signal[10]={"connReq","ins","er1","er2","win","port","ip","connDisc","connRef","lose"};

char comandoInserito[30];
struct timeval timer={60,0};
/*---------------------------*/
//funzioni
void connessione(char *argv[]);
void multiplexing();
void mostraComandi();
int controllaComando(char*);
void login();
int verificaUser();
int riceviDatiServer();
void inviaDati(char *,int);
void decifraComando(char*);
void visualizzaStato();
void controllaMossa(char*);
void creaSocketUdp();
void inviaDatiClient(char*,int);
int riceviDatiClient();
void showMap();
void resettaPartita();
int controllaVittoria();
int controllaOrizzontale();
int controllaVerticale();
int controllaDiagonaleDx();
int controllaDiagonaleSx();
void resettaTempo();
/*---------------------------*/

int main(int argc, char*argv[]) {
	int val;
	printf("\n \n************ FORZA 4 ************* \n \n" );
	sfidante=malloc(sizeof(struct Client)); 
	connessione(argv);
	multiplexing();
	do {
	login();
	} while(verificaUser()==-1);
	mostraComandi();
	stato_partita=0;
	visualizzaStato();
	for(;;) {
		copia=principale;
		if((val=select((fd_max+1),&copia,NULL,NULL,&timer))==-1) {
			printf("errore nella select! \n");
			exit(1);
		}
		if((val==0) && (stato_partita==1) && (turno==1)) {
			printf("HAI PERSO! -tempo scaduto\n");
			inviaDati("!disconnect:",sock);
			resettaPartita();
			visualizzaStato();		
		}
		else {
			int i;
			for(i=0;i<fd_max+1;i++) {	
				if(FD_ISSET(i,&copia)) {
					if(i==sock) {
						int ret;
						ret=riceviDatiServer();
						if(ret==0) {
							//dati al completo
							decifraComando(server->buffer->ricezione);
							free(server->buffer->ricezione);
							server->buffer->ricezione=malloc(sizeof(int));
						}
						
					}
					else if(i==0) {
						resettaTempo();
						char *comandoIns=malloc(sizeof(char)*25);
						scanf("%s",comandoIns);
						int ret;
						ret=controllaComando(comandoIns);
						if(ret==1) {
			    		inviaDati(comandoIns,sock);
			    		free(comandoIns);
			    	}		
					}
					else if(i==sfidante->sockUDP) {
						resettaTempo();
						int ret;
						ret=riceviDatiClient();
						if(ret==0) {
							//dati al completo
							decifraComando(sfidante->buffer->ricezione);
							free(sfidante->buffer->ricezione);
							sfidante->buffer->ricezione=malloc(sizeof(int));
						}
					}		
				}
			}
		}
	}
	return 0;
}

void resettaTempo() {
	timer.tv_sec=60;
}


void showMap() {
	int r,c;
	printf("\n        a     b     c     d     e     f     g     \n");
	printf("      _____ _____ _____ _____ _____ _____ _____ \n");
	for(r=0;r<RIGHE;r++) {
		printf("     |     |     |     |     |     |     |     |\n");
		for(c=0;c<COLONNE;c++) {
			if(c==0) 
				printf("  %d  ",RIGHE-r);
			printf("|  %c  ",griglia[r][c]);
		}
		printf("|\n");
		printf("     |_____|_____|_____|_____|_____|_____|_____|\n");
	}
	printf("\n");
}


void creaSocketUdp() {
	int i=socket(AF_INET, SOCK_DGRAM, 0);
	if(i==-1) {
		printf("Errore creazione socket UDP \n");
		exit(1);
	}
	sfidante->sockUDP=i;	
	memset(&addressUdp,0,sizeof(addressUdp));
	addressUdp.sin_family=AF_INET;
	addressUdp.sin_port=htons(portaUDP);
	inet_aton("127.0.0.1",&addressUdp.sin_addr);
	if(bind(i,(struct sockaddr*)&addressUdp,sizeof(addressUdp))==-1) {
			printf("Errore nella bind UDP \n");
			exit(1);
	}	
		
	memset(&sfidante->addressUdp,0,sizeof(sfidante->addressUdp));
	sfidante->addressUdp.sin_family=AF_INET;
	sfidante->addressUdp.sin_port=htons(sfidante->portaUDP);	
	inet_aton(sfidante->ip,&sfidante->addressUdp.sin_addr);
	
	sfidante->buffer=malloc(sizeof(struct Buffer));
	sfidante->buffer->byteDaRicevere=sizeof(int);
	sfidante->buffer->byteRicevuti=0;
	sfidante->buffer->byteDaInviare=sizeof(int);
	sfidante->buffer->byteInviati=0;
	sfidante->buffer->ricezione=malloc(sizeof(int));
	
	FD_SET(i,&principale);
	if(i>fd_max) {
		fd_max=i;
	}
	resettaTempo();
}


void inviaDatiClient(char * dato,int sock) {
	int quantoInvio=strlen(dato)+1;
	int i;
	socklen_t dimensione=sizeof(sfidante->addressUdp);
	//invio il dato piccolo
	sfidante->buffer->invio=malloc(sizeof(int));
	sprintf(sfidante->buffer->invio,"%d",quantoInvio);	
	sfidante->buffer->byteDaInviare=sizeof(int);
	sfidante->buffer->byteInviati=0;
	while(sfidante->buffer->byteDaInviare>0) {
		if((i=sendto(sock,(const void*)&sfidante->buffer->invio[sfidante->buffer->byteInviati],sfidante->buffer->byteDaInviare,0,(struct sockaddr*)&sfidante->addressUdp,dimensione))==-1) {
			perror("Errore con l'invidio dei dati \n");
		}
		else {
			sfidante->buffer->byteDaInviare-=i;	
			sfidante->buffer->byteInviati+=i;
		}
	}
	//invio il dato grosso
	free(sfidante->buffer->invio);
	sfidante->buffer->invio=dato;	
	sfidante->buffer->byteDaInviare=quantoInvio;
	sfidante->buffer->byteInviati=0;
	while(sfidante->buffer->byteDaInviare>0) {
		if((i=sendto(sock,(const void*)&sfidante->buffer->invio[sfidante->buffer->byteInviati],sfidante->buffer->byteDaInviare,0,(struct sockaddr*)&sfidante->addressUdp,sizeof(sfidante->addressUdp)))==-1) 		{
			printf("Errore con l'invio dei dati \n");
		}
		else {
			sfidante->buffer->byteDaInviare-=i;	
			sfidante->buffer->byteInviati+=i;
		}
	}
}


//ricezione
int riceviDatiClient() {
	int ret;
	socklen_t dimensione=sizeof(sfidante->addressUdp);
	if((ret=recvfrom(sfidante->sockUDP,&sfidante->buffer->ricezione[sfidante->buffer->byteRicevuti],sfidante->buffer->byteDaRicevere,0,(struct sockaddr*)&sfidante->addressUdp,&dimensione))<=0) {
		printf("C'è stato qualche errore nell'invio \n");
		close(sfidante->sockUDP);
		exit(1);		
	}
	
	sfidante->buffer->byteDaRicevere-=ret;
	sfidante->buffer->byteRicevuti+=ret;
	if(sfidante->buffer->byteDaRicevere==0 && sfidante->qualeDato==0) {
		//sono all prima iterazione
		sfidante->buffer->byteDaRicevere=atoi(sfidante->buffer->ricezione);
		free(sfidante->buffer->ricezione);
		sfidante->buffer->ricezione=malloc(sfidante->buffer->byteDaRicevere*sizeof(char));
		sfidante->buffer->byteRicevuti=0;
		sfidante->qualeDato=1;
		return 1;
	}
	else {
		//seconda iterazione
		sfidante->qualeDato=0;
		sfidante->buffer->byteDaRicevere=sizeof(int);
		sfidante->buffer->byteRicevuti=0;
		return 0;	
	}	
	return -1;
}



void visualizzaStato() {
	if(stato_partita!=-1) {
 		printf("%c ",stato[stato_partita]);
 		fflush(stdout);
 	}
}

void decifraComando(char* ricezione) {
	char risposta[20];
	int appoggio;
	int b=strlen(ricezione);
	int j,r,c;
	char comandoRicevuto[10];
	for(j=0;j<b;j++) {
	  if(ricezione[j]==':') {
	   	break;
	  }
		comandoRicevuto[j]=ricezione[j];
	}
	comandoRicevuto[j++]='\0';
	int k;
	char dato[20];
	for(k=0;k<b;k++) {
	  dato[k]=ricezione[j];
		j++;		
	}
	for(k=0;k<=9;k++) { 
			if((strcmp(comandoRicevuto,signal[k])==0)) {				
				break;
			}
	}
	switch(k) {
		case 0:
			printf("Richiesta connessione da %s \n",dato);
			printf("Vuoi accettare la sfida? 'Si' 'No' \n");
			visualizzaStato();
			scanf("%s",risposta);
			if((strcmp(risposta,"Si")==0) || (strcmp(risposta,"si")==0)) {
				strcpy(risposta,"acc:");
				strcat(risposta,dato);
				inviaDati(risposta,sock);
				appoggio=strlen(dato);
				sfidante->username=malloc(sizeof(char)*appoggio);
				strcpy(sfidante->username,dato);
				stato_partita=1;
				turno=0;
			}
			else {
				strcpy(risposta,"ref:");
				strcat(risposta,dato);
				inviaDati(risposta,sock);
				visualizzaStato();
			}
			break;
		case 1:
			//aggiorno la mappa
			r=posizioneLibera[dato[0]-97];
			c=dato[0]-97;
			posizioneLibera[dato[0]-97]--;
			if(sfidante->simbolo=='O') {
				griglia[r][c]='O';
			}
			else {
				griglia[r][c]='X';
			}			
			turno=1;
			showMap();
			printf("\nL'utente ha mosso. Tocca a te \n");
			visualizzaStato();
			break;		
		case 2:
			printf("Impossibile Connettersi, utente inesistente \n");
			visualizzaStato();
			turno=0;
			free(sfidante->username);
			break;		
		case 3:
			printf("Impossibile Connettersi, l'utente è già occupato in un'altra partita \n");
			visualizzaStato();
			turno=0;
			free(sfidante->username);
			break;
		case 4:
			break;
		case 5:
			//ricezione porta
			appoggio=atoi(dato);
			sfidante->portaUDP=appoggio;
			printf("Connesso con: %s\n",sfidante->username);
			stato_partita=1;
			
			if(turno==1) {
				printf("Sei il primo a muovere \n");
				sfidante->simbolo='O';
			  printf("Il tuo simbolo è 'X'\n");
			}
			else {
				printf("Aspetta il tuo turno \n");
				sfidante->simbolo='X';
				printf("Il tuo simbolo è 'O'\n");
			}
			visualizzaStato();		
			creaSocketUdp();
			break;
		case 6:
			//ricezione ip
   		strcpy(sfidante->ip,dato);
			break;
		case 7:
			//disconnessione
			printf("HAI VINTO! -l'utente si è disconnesso \n");
			resettaPartita();
			visualizzaStato();
			break;
		case 8:
			printf("L'utente non ha accettato \n");
			turno=0;
			free(sfidante->username);
			visualizzaStato();
			break;
		case 9:
			printf("***HAI PERSO***\n");
			resettaPartita();
			visualizzaStato();
			break;
		default:
			printf("Client connessi: %s \n",server->buffer->ricezione);
			visualizzaStato();	
	}
}



int verificaUser() {
	while((riceviDatiServer()!=0));
	if((strcmp(server->buffer->ricezione,"ok"))==0) {
		server->buffer->ricezione=malloc(sizeof(int));
		char* udp=malloc(50*sizeof(char));
		char appoggio1[30]="port:";
		int i=0;
		while(i==0) {
			printf("Inserisci la Porta UDP di ascolto : ");
			scanf("%s",udp);
			if(atoi(udp)>1024 && atoi(udp)<65535) {	
				i=1;
				portaUDP=atoi(udp);
				printf("inserito %d\n",portaUDP);
				break;
			}
			printf("Porta inserita non valida \n");
		}	
		strcat(appoggio1,udp);
		inviaDati(appoggio1,sock);
		free(udp);
		return 0;	
	}
	else {
		printf("Nome Utente non valido \n");
		free(server->buffer->ricezione);
		server->buffer->ricezione=malloc(sizeof(int));
		return -1;
	}	
}


//ricezione Server
int riceviDatiServer() {
	int ret;
	if((ret=recv(sock,&server->buffer->ricezione[server->buffer->byteRicevuti],server->buffer->byteDaRicevere,0))<=0) {
		printf("C'è stato qualche errore nella ricezione \n");
		close(sock);
		FD_CLR(sock,&principale);
		exit(1);		
	}
	server->buffer->byteDaRicevere-=ret;
	server->buffer->byteRicevuti+=ret;
	if(server->buffer->byteDaRicevere==0 && server->qualeDato==0) {
		//sono all prima iterazione
		server->buffer->byteDaRicevere=atoi(server->buffer->ricezione);
		free(server->buffer->ricezione);
		server->buffer->ricezione=malloc(server->buffer->byteDaRicevere*sizeof(char));
		server->buffer->byteRicevuti=0;
		server->qualeDato=1;
		return 1;
	}
	else {
		//seconda iterazione
		server->qualeDato=0;
		server->buffer->byteDaRicevere=sizeof(int);
		server->buffer->byteRicevuti=0;
		return 0;	
	}	
	return -1;
}

//fare funzione invio Client
void inviaDati(char * dato,int sock) {
	int quantoInvio=strlen(dato)+1;
	int i;
	//invio il dato piccolo
	server->buffer->invio=malloc(sizeof(int));
	sprintf(server->buffer->invio,"%d",quantoInvio);	
	server->buffer->byteDaInviare=sizeof(int);
	server->buffer->byteInviati=0;
	while(server->buffer->byteDaInviare>0) {
		if((i=send(sock,(const void*)&server->buffer->invio[server->buffer->byteInviati],server->buffer->byteDaInviare,0))==-1) {
			printf("Errore con l'invidio dei dati \n");
		}
		else {
			server->buffer->byteDaInviare-=i;	
			server->buffer->byteInviati+=i;
		}
	}
	//invio il dato grosso
	free(server->buffer->invio);
	server->buffer->invio=dato;
	
	server->buffer->byteDaInviare=quantoInvio;
	server->buffer->byteInviati=0;
	while(server->buffer->byteDaInviare>0) {
		if((i=send(sock,(const void*)&server->buffer->invio[server->buffer->byteInviati],server->buffer->byteDaInviare,0))==-1) 		{
			printf("Errore con l'invio dei dati \n");
		}
		else {
			server->buffer->byteDaInviare-=i;	
			server->buffer->byteInviati+=i;
		}
	}
}

void login() {
	char appoggio[30]="nick:";
	printf("Inserisci Username per Loggarti : ");
	scanf("%s",comandoInserito);
	strcat(appoggio,comandoInserito);
	inviaDati(appoggio,sock);
	//nel caso venga accettato
	strcpy(mioUsername,comandoInserito);
}

int controllaComando(char* comandoInserito) {
	int i,str;
	for(i=2;i<=8;i++) { 
			if(strcmp(comandoInserito,comando[i])==0) {
				break;
			}
	}
	switch(i) {
				case 2:
					//!help
					mostraComandi();
					visualizzaStato();
					break;
				case 5: 					
					if(stato_partita==0) {
						printf("Non hai in corso nessuna partita \n");
					}
					else {
						inviaDati("!disconnect:",sock);
						resettaPartita();
					}
					visualizzaStato();
					break;
				case 6: 
					//!quit. Devo avvisare il client che mi disconnetto e cancellarmi
					if(stato_partita!=0)
						inviaDati("!disconnect:",sock);
					inviaDati("!quit:",sock);
					free(server);
					exit(1);
					break;
				case 4: 
					//!connect
					str=strlen(comandoInserito);
					comandoInserito[str]=':';
					comandoInserito[str+1]='\0';
					char appoggio[25];
					appoggio[0]='\0';
					scanf("%s",appoggio);
					if((strcmp(appoggio,mioUsername)==0)) {
						printf("Impossibile sfidare se stessi \n");
						visualizzaStato();
					}
					else if(stato_partita==1) {
						printf("Sei già in partita! \n");
						visualizzaStato();
					}
					else {
						strcat(comandoInserito,appoggio);
						inviaDati(comandoInserito,sock);
					//mi salvo chi voglio sfidare nel caso accetti
						turno=1;
						sfidante->username=malloc(sizeof(char)*strlen(appoggio));
						strcpy(sfidante->username,appoggio);
					}
					if(comandoInserito!=NULL)
						free(comandoInserito);
					break;
				case 7: 
					showMap();
					visualizzaStato();
					break;
				case 8: 
					//faccio insert colonna
					scanf("%s",appoggio);
					if(stato_partita==0) {
						printf("Comando attivo solo in partita \n");
					}
					else if(turno==0) {
						printf("Aspetta il tuo turno \n");
					}
					else{
					controllaMossa(appoggio);
					}
					visualizzaStato();
					break;
				case 9:
					printf("Comando non trovato \n");
					visualizzaStato();
					break;			
				default :
					return 1;
	}
	return -1;
}
void controllaMossa(char* appoggio) {
	char risposta[20];
	strcpy(risposta,"ins:");
	strcat(risposta,appoggio);
	if(appoggio[0]-97<0 || appoggio[0]-97>COLONNE) {
		printf("Mossa non consentita \n");
		return;
	}
	int r=posizioneLibera[appoggio[0]-97];
	if(r<0) {
		printf("Mossa non consentita \n");
		return;
	}
	int c=appoggio[0]-97;
	if(sfidante->simbolo=='O') {
		griglia[r][c]='X';
	}
	else {
		griglia[r][c]='O';
	}
	posizioneLibera[c]--;
	inviaDatiClient(risposta,sfidante->sockUDP);	
	turno=0;
	c=controllaVittoria();
	if(c==1){
		// lo dico al client e al server
		//resetto tutto
		inviaDatiClient("lose:",sfidante->sockUDP);	
		inviaDati("fin:",sock);
		showMap();
		visualizzaStato();
		printf("***HAI VINTO***\n");
		resettaPartita();
	}
	else {
		showMap();
		printf("Aspetta il tuo turno \n");
	}
}


void resettaPartita() {
	int i,l;
	for(i=0;i<RIGHE;i++)
		for(l=0;l<COLONNE;l++)
			griglia[i][l]=' ';
	for(i=0;i<COLONNE;i++)
		posizioneLibera[i]=RIGHE-1;		
	stato_partita=0;
	close(sfidante->sockUDP);
	FD_CLR(sfidante->sockUDP,&principale);
	fd_max=sock;
	free(sfidante->username);
}


int controllaVittoria() {
	int i,l;
	for(i=0;i<RIGHE;i++) {
		for(l=0;l<COLONNE;l++) {
			int n=0;
			n+=controllaOrizzontale(i,l);
			n+=controllaVerticale(i,l);
			n+=controllaDiagonaleDx(i,l);
			n+=controllaDiagonaleSx(i,l);
			if(n>0)
				return 1;
			}
		}
	return 0;	
}

int controllaOrizzontale(int r,int c) {
	int i;
	char simbolo=griglia[r][c];
	if(simbolo==' ')
		return 0;
	if(c-3<0)
		return 0;
	for(i=1;i<4;i++) {
		if(griglia[r][c-i]!=simbolo)
			return 0;
	}
	return 1;
}

int controllaVerticale(int r,int c) {
	int i;
	char simbolo=griglia[r][c];
	if(simbolo==' ')
		return 0;
	if(r-3<0)
		return 0;
	for(i=1;i<4;i++) {
		if(griglia[r-i][c]!=simbolo)
			return 0;
	}
	return 1;
}

int controllaDiagonaleSx(int r,int c) {
	int i;
	char simbolo=griglia[r][c];
	if(simbolo==' ')
		return 0;
	if(r+3>RIGHE || c-3<0)
		return 0;
	for(i=1;i<4;i++) {
		if(griglia[r+i][c-i]!=simbolo)
			return 0;
	}
	return 1;
}

int controllaDiagonaleDx(int r,int c){
	int i;
	char simbolo=griglia[r][c];
	if(simbolo==' ')
		return 0;
	if(c+3>COLONNE || r+3>RIGHE)
		return 0;
	for(i=1;i<4;i++) {
		if(griglia[r+i][c+i]!=simbolo)
			return 0;
	}
	return 1;
}


void mostraComandi() {
	printf("\n");
	printf("!help                   ->mostra l'elenco comandi \n");
	printf("!who                    ->mostra l'elenco dei client connessi al server \n");
	printf("!connect [nome client]  ->avvia una partita con l'utente \n");
	printf("!disconnect             ->disconnette il client dall'attuale partita \n");
	printf("!quit                   ->disconnette il client dal server \n");
	printf("!show_map               ->mostra la mappa di gioco \n");
	printf("!insert [column]        ->inserisce il gettone in column \n");
	printf("\n");
	return;
}
void connessione(char* argv[]) {
	stato_partita=-1;
	server=malloc(sizeof(struct Server));
	server->buffer=malloc(sizeof(struct Buffer));
	server->buffer->byteDaRicevere=sizeof(int);
	server->buffer->byteRicevuti=0;
	server->buffer->byteDaInviare=sizeof(int);
	server->buffer->byteInviati=0;
	server->buffer->ricezione=malloc(sizeof(int));
	server->qualeDato=0;
	sock=socket(AF_INET,SOCK_STREAM,0);
	if(socket(AF_INET,SOCK_STREAM,0)==-1) {
		printf("Errore creazione socket principale \n");
		exit(1);
	}
	memset(&address,0,sizeof(address));	
	address.sin_family=AF_INET;
	server->portaTCP=atoi(argv[2]);
	address.sin_port=htons(server->portaTCP);
	inet_pton(AF_INET,argv[1],&address.sin_addr.s_addr);
	int ret;
	ret=connect(sock,(struct sockaddr*)&address,sizeof(address));
	if(ret==-1) {
		printf("Errore nella connect \n");
		exit(1);
	}
}

void multiplexing() {
	FD_ZERO(&principale);
	FD_ZERO(&copia);
	FD_SET(sock,&principale);
	FD_SET(0,&principale);
	fd_max=sock;
}

