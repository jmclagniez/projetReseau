#include <iostream>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
using namespace std;
#include "../structure/structure.h"
#include "reseau/sock.h"
#include "reseau/sockdist.h"
#include "../structure/clientData/DonneeClient.h"
#include "../structure/clientData/TableauClient.h"
#include "threadData/DonneeThread.h"
#include "threadData/TableauThread.h"
#include "Serveur.h"

struct DescTableauClient
{
  TableauClient* donneeClients;
  int descClient;
  in_addr adresse;
};

struct protocoleEnvoieDonnee
{
  int proto;
  DonneeClient donnee;
};


Serveur::Serveur()
{
  brPublic = new Sock(SOCK_STREAM, 0);
  if(brPublic->good()){descBrPublic = brPublic->getsDesc();}
  else{cout<<"Probleme initialisation de la socket"<<endl; 
    exit(-1);}
  listen(descBrPublic,100);
}
Serveur::~Serveur()
{
  //close(*brPublic);
  delete brPublic;
}

TableauClient Serveur::getDonneeClient()
{
  return donneeClients;
}
TableauThread Serveur::getDonneeThread()
{
  return donneeThreads;
}
Sock* Serveur::getSock()
{
  return brPublic;
}
int Serveur::getDescBrPublic()
{
  return descBrPublic;
}
void Serveur::setDonneeClient(TableauClient donneeClient)
{
  this->donneeClients = donneeClient;
}
void Serveur::setDonneeThread(TableauThread donneeThread)
{
  this->donneeThreads = donneeThread;
}
void Serveur::setSock(Sock * brPublic)
{
  this->brPublic = brPublic;
}
void Serveur::setDescBrPublic(int descBrPublic)
{
  this->descBrPublic = descBrPublic;
}
void Serveur::init_serveur()
{
  lancer_serveur();
}

void Serveur::lancer_serveur()
{
  cout<<"Lancement Serveur"<<endl;
  while(true)
    {
      struct sockaddr_in brCircuitVirtuel;
      socklen_t lgrBrCircuitVirtuel = sizeof(struct sockaddr_in);
      int descBrCircuitVirtuel = accept(descBrPublic,(sockaddr *)&brCircuitVirtuel,&lgrBrCircuitVirtuel);
      if(descBrCircuitVirtuel == -1)
	{
	  perror("accept");
	  cout<<"Problème accept"<<endl;
	}
      else
	{
	  pthread_t id;
	  id = creationThreadClient(descBrCircuitVirtuel,brCircuitVirtuel);
	  if(id != -1)
	    {
	      cout<<"Voici l'id du client "<<descBrCircuitVirtuel<<endl;
	      donneeThreads.addDonneeThread(id,descBrCircuitVirtuel);
	    }
	  else
	    {
	      close(descBrCircuitVirtuel);
	    }
     	}
    }
}


pthread_t Serveur::creationThreadClient(int descBrCircuitVirtuel,struct sockaddr_in& adresse)
{
  pthread_t id;
  struct DescTableauClient* parametreThread = (struct DescTableauClient*)malloc(sizeof(struct DescTableauClient));
  parametreThread->donneeClients = &donneeClients;
  parametreThread->descClient = descBrCircuitVirtuel;
  parametreThread->adresse = adresse.sin_addr;
  if(pthread_create(&id,NULL,thread_client,(void*)parametreThread) != 0)
    {
      cout<<"Erreut thread creation"<<endl;
      return -1;
    }
  else
    {
      return id;
    }
}





//////////////////////////////FONCTI0N/////////////////////////////////////////

void* thread_client(void* par)
{
  struct DescTableauClient* parametreClient = (struct DescTableauClient*)par;
  cout<<parametreClient->descClient<<endl;
  int en_tete = -1;
  int isPresent;
  DonneeClient* donnee_client = NULL;
  while((isPresent = read(parametreClient->descClient,&en_tete,4) != 0 && isPresent != -1))
    {
      cout<<en_tete<<endl;
      switch(en_tete)
	{
	case 1:
	  {
	    int numero_port;
	    read(parametreClient->descClient,&numero_port,4);
	    cout<<"Le numero port du client est: "<<numero_port<<endl;
	    donnee_client = new DonneeClient(parametreClient->adresse,numero_port);
	    pthread_mutex_lock(&(parametreClient->donneeClients->getVerrou()));
	    parametreClient->donneeClients->pushClient(donnee_client);
	    pthread_mutex_unlock(&(parametreClient->donneeClients->getVerrou()));
	  }
	case 2:
	  {
	    cout<<"Envoie les informations des autres clients à notre client traitré dans le thread"<<endl;
	     pthread_mutex_lock(&(parametreClient->donneeClients->getVerrou()));
	     for(int i = 0 ; i < parametreClient->donneeClients->size() ; i++)
	       {
		 if(parametreClient->donneeClients->getDonnee(i) != donnee_client)
		   {
		     struct protocoleEnvoieDonnee protocoleDonnee_client;
		     protocoleDonnee_client.proto = 2;
		     protocoleDonnee_client.donnee = *(parametreClient->donneeClients->getDonnee(i));
		     write(parametreClient->descClient,&protocoleDonnee_client,sizeof(protocoleDonnee_client));
		   }
	       }
	  }
	}
    }


  //Suppresion client
  if(isPresent == -1)
    perror("read");
  cout<<"Client est partie"<<endl;
  close(parametreClient->descClient);
  pthread_mutex_lock(&(parametreClient->donneeClients->getVerrou()));
  int rang = -1;
  if(donnee_client != NULL)
    rang =  parametreClient->donneeClients->rangClient(donnee_client); 
  if(rang != -1)
    parametreClient->donneeClients->rmClient(rang);
  
  delete parametreClient;
  pthread_mutex_unlock(&(parametreClient->donneeClients->getVerrou()));
  pthread_exit(par);
}
