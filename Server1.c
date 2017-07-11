#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>

/*------------------------*/
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
	
	int libero;
	int qualeDato;
	struct Buffer* buffer;
	
	struct Client* next;
	struct Client* sfidante;
};
struct Server {
	char ip[20];
	int portaTCP;	
};

struct Server* server;
struct Client* attivi;

int sockListening;
struct sockaddr_in clientSockaddr,serverSockaddr;
int fd_max;
fd_set principale,copia;

char* comando[12]={"nick","port","!help","!who","!connect","!disconnect","!quit","!show map","!insert column","acc","ref","fin"};
/*---------------------------*/
void connessione(char* argv[]);
void multiplexing();
void nuovoClient(int);
void decifraComando(struct Client*);
int riceviDati(struct Client*);
void eliminaClient(struct Client*);
struct Client* ricercaClient(int);
struct Client* ricercaClientPerNome(char*);
int assegnaUsername(struct Client*,char*);
char* listaClient();
/*---------------------------*/
//inizio
int main(int argc,char* argv[]) {
	connessione(argv);
	multiplexing();
	for(;;) {
		copia=principale;
		if(select(fd_max+1,&copia,NULL,NULL,NULL)==-1) {
			printf("Errore nella select\n");
			exit(1);		
		}
		else {
			int i;
			for(i=0;i<=fd_max+1;i++) {
				if(FD_ISSET(i,&copia) && i!=0) {
					if(i==sockListening) {
						printf("connessione stabilita con il client\n");
						socklen_t addrlen=sizeof(clientSockaddr);
						int sockAppoggio;
						if((sockAppoggio=accept(sockListening,(struct sockaddr*)&clientSockaddr,&addrlen))==-1) {
							printf("Errore accept \n");				
						}
						else {
							FD_SET(sockAppoggio,&principale);
							if(sockAppoggio>fd_max) {
								fd_max=sockAppoggio;
							}
							nuovoClient(sockAppoggio);			
						}
					}
					else {
						struct Client* sender=ricercaClient(i);
						int ret;
						ret=riceviDati(sender);
						if(ret==0) {
							//sono arrivati i dati, cerco il comando ricevuto
							decifraComando(sender);
							//rialloco il buffer per la prossima ricezione
							free(sender->buffer->ricezione);
							sender->buffer->ricezione=malloc(sizeof(int));
						}
					}
				}
			}
		}
	}
	return 0;
}


void inviaDati(struct Client* client,char * dato) {
	int quantoInvio=strlen(dato)+1;
	int i;
	//invio la lunghezza del baffer
	client->buffer->invio=malloc(sizeof(int));
	sprintf(client->buffer->invio,"%d",quantoInvio);	
	client->buffer->byteDaInviare=sizeof(int);
	client->buffer->byteInviati=0;
	while(client->buffer->byteDaInviare>0) {
		if((i=send(client->sockTCP,(const void*)&client->buffer->invio[client->buffer->byteInviati],client->buffer->byteDaInviare,0))==-1) {
			printf("Errore con l'invidio dei dati \n");
		}
		else {
			client->buffer->byteDaInviare-=i;	
			client->buffer->byteInviati+=i;
		}
	}
	//risistemo variabili e invio il dato completo
	free(client->buffer->invio);
	client->buffer->invio=dato;
	client->buffer->byteDaInviare=quantoInvio;
	client->buffer->byteInviati=0;
	i=0;
	while(client->buffer->byteDaInviare>0) {
		if((i=send(client->sockTCP,(const void*)&client->buffer->invio[client->buffer->byteInviati],client->buffer->byteDaInviare,0))==-1) 		{
			printf("Errore con l'invio dei dati \n");
		}
		else {
			client->buffer->byteDaInviare-=i;	
			client->buffer->byteInviati+=i;
		}
	}
}


int riceviDati(struct Client* sender) {
	int ret;
	if((ret=recv(sender->sockTCP,&sender->buffer->ricezione[sender->buffer->byteRicevuti],sender->buffer->byteDaRicevere,0))<=0) {
		if(sender->libero==1) {
			inviaDati(sender->sfidante,"connDisc:");
			sender->libero=0;
			sender->sfidante->libero=0;
			printf("%s si è disconnesso da %s\n",sender->username,sender->sfidante->username);	
			printf("%s è libero\n%s è libero\n",sender->username,sender->sfidante->username);
		}
		if(sender->username!=NULL) {
			printf("%s si è scollegato\n",sender->username);
		}
		sender->username=NULL;
		eliminaClient(sender);
		return -1;
	}
	sender->buffer->byteDaRicevere-=ret;
	sender->buffer->byteRicevuti+=ret;

	
	if(sender->buffer->byteDaRicevere==0 && sender->qualeDato==0) {
		//sono all prima iterazione, dove ricevo la lunghezza del buffer da allocare
		sender->buffer->byteDaRicevere=atoi(sender->buffer->ricezione);
		free(sender->buffer->ricezione);
		//alloco il buffer della grandezza necessaria
		sender->buffer->ricezione=malloc(sender->buffer->byteDaRicevere*sizeof(char));
		sender->buffer->byteRicevuti=0;
		sender->qualeDato=1;
		return 1;
	}
	else {
		//seconda iterazione, dove ricevo il dato completo
		sender->qualeDato=0;
		sender->buffer->byteDaRicevere=sizeof(int);
		sender->buffer->byteRicevuti=0;
		return 0;	
	}	
	return -1;
}





void decifraComando(struct Client* sender) {
	int b=strlen(sender->buffer->ricezione);
	struct Client* sfidante;
	struct Client* sfidato;
	int j;
	char comandoRicevuto[20];
	int risposta;
	char* lista;
	//separo il comando dal dato
	for(j=0;j<b;j++) {
	  if(sender->buffer->ricezione[j]==':') {
	   	break;
	  }
		comandoRicevuto[j]=sender->buffer->ricezione[j];
	}
	comandoRicevuto[j++]='\0';
	int k;
	
	char dato[20];
	//prendo il dato
	for(k=0;k<b;k++) {
	  dato[k]=sender->buffer->ricezione[j];
		j++;		
	}
	dato[k++]='\0';
	//printf("qui\n");
	//printf("%s\n",comandoRicevuto);
	//distinguo le azione da fare in base al comando ricevuto
	for(k=0;k<=11;k++) { 
			if((strcmp(comandoRicevuto,comando[k])==0)) {				
				break;
			}
	}
	switch(k) {
		case 0:
			//assegna username
			risposta=assegnaUsername(sender,dato);
			if(risposta==1) {
				inviaDati(sender,"ok");
				printf("%s si è connesso \n",dato);
			}
			else {
				inviaDati(sender,"no");
			}
			break;
		case 1:
			//assegna porta
			sender->portaUDP=atoi(dato);
			printf("%s è libero \n",sender->username);
			break;
		case 3:
			//!who
			printf("ricevuto comando !who\n");
			lista=listaClient();
			inviaDati(sender,lista);
			free(lista);
			break; 
		case 4:
			//ricevuto richiesta di connessione per partita. ( in questo caso sfidante è quello sfidato. Ho fatto un pò di confusione con i nomi)
			printf("richiesta di nuova partita da %s\n",sender->username);
			printf("richiesta per sfidare %s\n",dato);
			sfidato=ricercaClientPerNome(dato);
			//non c'è nessun client connesso con quel nome			
			if(sfidato==NULL) {
					inviaDati(sender,"er1:");
			}
			//lo sfidante scelto è già impegnato in partita	
			else if(sfidato->libero!=0) {
					inviaDati(sender,"er2:");
			}
			else {
				//contatto client
				char appoggio[30]="connReq:";
				strcat(appoggio,sender->username);				
				inviaDati(sfidato,appoggio);
			}				 
			break;
		case 5:
			//disconnessione
			//avviso lo sfidato che lo sfidante si è disconnesso
			inviaDati(sender->sfidante,"connDisc:");
			sender->libero=0;
			sender->sfidante->libero=0;
			printf("%s si è disconnesso da %s\n",sender->username,sender->sfidante->username);
			printf("%s è libero\n%s è libero\n",sender->username,sender->sfidante->username);
			break;
		case 6:
			//quit
			printf("%s si è scollegato\n",sender->username);
			eliminaClient(sender);
			break;
		case 9:
			//Lo sfidato ha accettato l'invito. Mando portaUDP e IP allo sfidante
			sfidante=ricercaClientPerNome(dato);
			printf("%s ha accettato di giocare con %s\npartita avviata\n",sender->username,sfidante->username);
			//sfidante è quello che ha sfidato per primo..li collego
			sfidante->libero=1;
			sender->libero=1;
			sender->sfidante=sfidante;
			sfidante->sfidante=sender;

			char appoggio[20]="ip:";
			strcat(appoggio,sender->ip);
			inviaDati(sfidante,appoggio);			
			char appoggio1[20];
			sprintf(appoggio1,"%d",sender->portaUDP);
			strcpy(appoggio,"port:");
			strcat(appoggio,appoggio1);
			inviaDati(sfidante,appoggio);
			
			strcpy(appoggio,"ip:");
			strcat(appoggio,sfidante->ip);
			inviaDati(sender,appoggio);			
			sprintf(appoggio1,"%d",sfidante->portaUDP);
			strcpy(appoggio,"port:");
			strcat(appoggio,appoggio1);
			inviaDati(sender,appoggio);
			break;
		case 10:
			//partita rifiutata
			sfidante=ricercaClientPerNome(dato);
			printf("%s ha rifiutato di giocare con %s\n",sender->username,sfidante->username);
			strcpy(appoggio,"connRef:");
			inviaDati(sfidante,appoggio);
			break;
		default:
			//fine partita
			printf("fine partita tra %s e %s \n",sender->username,sender->sfidante->username);
			sender->libero=0;
			sender->sfidante->libero=0;
	}
}

char* listaClient() {
	int len=0;
	struct Client* p=attivi;
	while(p!=NULL) {
		len+=strlen(p->username)+1;
		p=p->next;
	}
	char* dato=malloc(len);
	memset(dato,'\0',len);
	p=attivi;
	len=0;
	while(p!=NULL) {
		len+=strlen(p->username);
		strcat(dato,p->username);
		strcat(dato," ");
		p=p->next;
		len++;
	}
	dato[len]='\0';
	return dato;
}
int assegnaUsername(struct Client* client,char* dato) { 
	struct Client *p=attivi;
	while(p!=NULL) {
			if(p->username!=NULL) {
				if((strcmp(p->username,dato))==0) {
						return -1;
				}
			}
			p=p->next;
	}
	int lunghezza=strlen(dato)+1;	
	client->username=(char*)malloc(sizeof(char)*lunghezza);
	strcpy(client->username,dato);
	return 1;
}
void eliminaClient(struct Client* eliminare) {
	struct Client *p=attivi;
	struct Client *precedente=NULL;
	while(p!=NULL) {
			if(p->sockTCP==eliminare->sockTCP)
				break;
			precedente=p;	
			p=p->next;
	}
	if(precedente==NULL) {
		attivi=attivi->next;
	}
	else {
		precedente->next=p->next;
	}
	close(eliminare->sockTCP);
	FD_CLR(eliminare->sockTCP,&principale);
	free(eliminare->username);
	free(eliminare);
}
struct Client* ricercaClient(int i) {
	struct Client *p=attivi;
	while(p->next!=NULL) {
		if(p->sockTCP==i) {		
			break;
		}
		else {
			p=p->next;
		}
	}
	return p;
}

struct Client* ricercaClientPerNome(char* nome) {
	struct Client *p=attivi;
	while(p!=NULL) {
		if((strcmp(p->username,nome)==0)) {	
			break;
		}
		else {
			p=p->next;
		}
	}
	return p;
}


void nuovoClient(int sockAppoggio) {
	struct Client* nuovoC=malloc(sizeof(struct Client));
	nuovoC->buffer=malloc(sizeof(struct Buffer));
	nuovoC->buffer->byteDaRicevere=sizeof(int);
	nuovoC->buffer->byteRicevuti=0;
	nuovoC->buffer->byteDaInviare=sizeof(int);
	nuovoC->buffer->byteInviati=0;
	nuovoC->buffer->ricezione=malloc(sizeof(int));
	nuovoC->sockTCP=sockAppoggio;
	nuovoC->qualeDato=0;
	nuovoC->libero=0;
	strcpy(nuovoC->ip,inet_ntoa(clientSockaddr.sin_addr));
	
	if(attivi==NULL) {
		attivi=nuovoC;
		attivi->next=NULL;
	}
	else {
		nuovoC->next=attivi;
		attivi=nuovoC;
	} 
}
void multiplexing() {
	FD_ZERO(&principale);
	FD_ZERO(&copia);
	FD_SET(sockListening,&principale);
	fd_max=sockListening;
}
void connessione(char* argv[]) {
	sockListening=socket(AF_INET,SOCK_STREAM,0);
	if(sockListening==-1) {
		printf("Errore creazione socket principale \n");
		exit(1);
	}	
	memset(&serverSockaddr,0,sizeof(serverSockaddr));
	serverSockaddr.sin_family=AF_INET;	
	struct Server* server=malloc(sizeof(struct Server));
	strcpy(server->ip,argv[1]);
	server->portaTCP=atoi(argv[2]);	
	serverSockaddr.sin_port=htons(server->portaTCP);
	inet_pton(AF_INET,(const char*) server->ip,&serverSockaddr.sin_addr.s_addr);
	printf("Ip: %s,porta: %d\n",server->ip,server->portaTCP);
	int ret;
	ret=bind(sockListening,(struct sockaddr*)&serverSockaddr,sizeof(serverSockaddr));
	if(ret==-1) {
		printf("Errore nella bind \n");
		exit(1);
	}	
	ret=listen(sockListening,10);
	if(ret==-1) {
		printf("Errore nella Listen \n");
		exit(1);
	}
}


