#include "sock.h"
#include "sockdist.h"
#include <fstream>
#include <stdlib.h>
#include <vector>
#include "string.h"
#include "unistd.h"
#include "pthread.h"
#include "sys/types.h"
#include "../structure/structure.h"
#include "fichiers.h"

#include "client.h"
pthread_mutex_t mutexInfoFichier = PTHREAD_MUTEX_INITIALIZER;

#define TAILLE_PARTITION 1000

Client::Client()
{
  setDescSockServeur(-2); // Pour vérifier si le descripteur est utilisé

  char port[6];
  fstream infoClientFile("infoClient.txt", fstream::in);
  infoClientFile.getline(port,6);
  infoClientFile.close();

  setPort(atoi(port));

  sockPub = new Sock(SOCK_STREAM,(short)getPort(),0);

  if(sockPub->good()) setDescSockPub(sockPub->getsDesc());
  else
    {
      perror("ConstructeurClient");
      exit(1);
    }
  cout << "Création de la socket publique réussi" << endl;
}

Client::~Client()
{
  close(descSockPub);
  close(descSockServeur);

  delete sockPub;
  delete sockServeur;
  delete sockDistServeur;
}

void Client::lancerPortEcoute()
{
  if(pthread_create(&idThServ, NULL, threadPortEcoute, &descSockPub) != 0)
    {
      perror("LancerPortEcoute");
    }
}

void Client::fermeturePortEcoute()
{
  pthread_cancel(idThServ);
  if(pthread_join(idThServ, NULL) != 0)
    {
      perror("FermeturePortEcoute");
    }
  cout << "Fin d'execution du port d'écoute" << endl;
}
				 		   

void Client::connexionServeur()
{
  if(descSockServeur != -2)
    {
      deconnexionServeur();
    }
  sockServeur = new Sock(SOCK_STREAM, 0);
  if(sockServeur->good()) setDescSockServeur(sockServeur->getsDesc());
  else
    {
      perror("ConnexionServeur");
    }
  cout << "Création de la socket serveur réussi" << endl;
 
  char port[6];

  fstream infoServeurFile("infoServeur.txt", fstream::in);
  infoServeurFile.getline(adresse,255);
  infoServeurFile.getline(port,6);
  infoServeurFile.close();

  serveurPort = atoi(port);

  sockDistServeur = new SockDist(adresse, (short)serveurPort);
  adrSockPub = sockDistServeur->getAdrDist();
  lgAdrSockPub = sizeof(struct sockaddr_in);
  
  cout << "Tentative de connexion au serveur \"" << adresse << ":" << serveurPort << "\"..." << endl;

  int connexion = connect(getDescSockServeur(), (struct sockaddr *)adrSockPub, lgAdrSockPub);

  if(connexion != 0)
    {
      cout << "La connexion à " << adresse << ":" << serveurPort << " a échoué !" << endl;
    }
  
  cout << "Connexion établie avec le serveur" << endl;  
  
  struct protocoleEnvoiePort monPort;
  monPort.proto = 1;
  monPort.port = numeroPort;
  
  write(descSockServeur,&monPort,sizeof(struct protocoleEnvoiePort));
  struct tableauDescServeur* descServeur=(struct tableauDescServeur*)malloc(sizeof(struct tableauDescServeur));;
  descServeur->donneeClients = &donneeClients;
  descServeur->descServeur = descSockServeur;
  cout<<"non thread "<<	  descSockServeur<<endl;
  if(pthread_create(&idThServPrin,NULL,threadReceptionServeurPrin, descServeur) != 0)
    {
      perror("ConnexionServeur");
    }
}

void Client::deconnexionServeur()
{
  close(descSockServeur);
  descSockServeur = -2;
  
  pthread_cancel(idThServPrin);
  if(pthread_join(idThServPrin, NULL) != 0)
    {
      perror("Deconnexion du serveur");
    }
  cout << "Deconnexion du serveur" << endl;
}

void Client::envoyerFichier(char* nomFichier)
{ 
  struct envoieFichier *f = (struct envoieFichier *)malloc(sizeof(struct envoieFichier));
  strcpy(f->nomFichier,nomFichier);
  f->donneeClients = &donneeClients;
  f->adrSockPub = adrSockPub;
  f->lgAdrSockPub = lgAdrSockPub;

  pthread_t envoieFichier;
  if(pthread_create(&envoieFichier,NULL,threadEnvoyerFichier,f) != 0)
    {
      perror("EnvoieFichier");
    }
  cout << "Fin envoie fichier" << endl;
}

void Client::recupererFichier(char* nomFichier)
{
  if(descSockServeur == -2)
    {
      cout << "Vous n'etes pas connecté au serveur" << endl;
    }
  else
    {
      vector<int> partitionManquant = partitionManquante(nomFichier);
      struct protocoleRecupereFichier2 req;
      req.proto = 3;
      strcpy(req.nom,nomFichier);
      req.taille = strlen(req.nom);
      cout << partitionManquant.size() << endl;
      for(int i = 0; i < partitionManquant.size(); i++)
	{
	  req.part = partitionManquant[i];
	  cout << req.nom << " "<<  req.taille << " " << req.part << endl;
	  write(descSockServeur,&req,sizeof(int)*3+req.taille*sizeof(char));	  
	}      
    }
}

void Client::rafraichirClient()
{
  pthread_mutex_lock(&(donneeClients.getVerrou()));
  while(!(donneeClients.getDonnee().empty()))
  {
    donneeClients.rmClient(0);
  }
  pthread_mutex_unlock(&(donneeClients.getVerrou()));
  
  struct protocoleRecupereClient prot;
  prot.proto = 2;

  write(descSockServeur,&prot,sizeof(prot));
}

void Client::setDescSockServeur(int desc)
{
  descSockServeur = desc;
}

int Client::getDescSockServeur()
{
  return descSockServeur;
}

void Client::setDescSockPub(int desc)
{
  descSockPub = desc;
}

int Client::getDescSockPub()
{
  return descSockPub;
}

void Client::setPort(int port)
{
  numeroPort = port;
}

int Client::getPort()
{
  return numeroPort;
}

///////////////////////// Fonction //////////////////////////

void *threadPortEcoute(void *par)
{
  int *descSockPub = (int *)par;
  TableauClient donneeClients;
  
  if(listen(*descSockPub,100) != 0)
    {
      cout << "Erreur création de la file d'attente" << endl;
      exit(1);
    }
  cout << "Création de la file d'attente réussi" << endl;

  struct sockaddr_in sockCV;
  socklen_t lgSockCV = sizeof(struct sockaddr_in);
  int descSockCV;

  while(true)
    {
      pthread_t *idThClient;
      if((descSockCV = accept(*descSockPub, (struct sockaddr *)&sockCV, &lgSockCV)) < 0)
	{
	  cout << "Problème accept() du port d'écoute" << endl;
	  perror("accept");
	}
      else
	{
	  cout << "Nouvelle connection" << endl;
	  idThClient = creationThreadClient(&donneeClients,descSockCV,sockCV);
	  if((*idThClient) == -1)
	    {
	      cout<<"Problème pthread_t"<<endl;
	      close(descSockCV);
	    }
	  
	}
      
      
    }
  
  pthread_exit(NULL);
}

void *threadClient(void *par)
{
  cout<<"Il y a un nouveau client de connecté"<<endl;
  struct DescTableauClient* parametreClient = (struct DescTableauClient*)par;
  int en_tete = -1;
  int isPresent;
  DonneeClient* donnee_client = new DonneeClient(parametreClient->adresse,parametreClient->descClient);
  pthread_mutex_lock(&(parametreClient->donneeClients->getVerrou()));
  parametreClient->donneeClients->pushClient(donnee_client);
  pthread_mutex_unlock(&(parametreClient->donneeClients->getVerrou()));
  while((isPresent = read(parametreClient->descClient, &en_tete,4) != 0 && isPresent != -1))
    {
      switch(en_tete)
	{
	case 1: 
	  {
	    recuperationPartition(parametreClient->descClient);
	    break;
	  }
	case 3:
	  {
	    receptionPartitionManquante(parametreClient->descClient);
	    break;
	  }
	}
    }
  suppresionClient(donnee_client,parametreClient,isPresent);
  pthread_exit(par);
}

void *threadEnvoyerFichier(void *par)
{  
  struct envoieFichier * f = (struct envoieFichier *)par;
  struct sockaddr_in *adrSockPub = f->adrSockPub;
  int lgAdrSockPub = f->lgAdrSockPub;  
  pthread_mutex_lock(&mutexInfoFichier);
  fstream infoClientFile("fichiers/infoFichiers.txt", fstream::in);
  char fileName[255];
  bool continuer = true;
  char * tok;
  while(continuer && infoClientFile.getline(fileName,255))
    {
      tok = strtok(fileName,"=");
      if(strcmp(tok,f->nomFichier) == 0)
	{ 
	  continuer = false;
	  pthread_mutex_unlock(&mutexInfoFichier);
	  
	  char cheminFichierEnvoi[255];
	  strcpy(cheminFichierEnvoi,"fichiers/");
	  strcat(cheminFichierEnvoi,f->nomFichier);
	  cout << "Envoie du fichier : " << cheminFichierEnvoi << endl;

	  int nbPartition = decouperFichier(cheminFichierEnvoi,TAILLE_PARTITION);

	  strcat(cheminFichierEnvoi,".dos/");
	  strcat(cheminFichierEnvoi,f->nomFichier);

	  pthread_mutex_lock(&f->donneeClients->getVerrou());
	  if(f->donneeClients->getDonnee().size() == 0)
	    {
	      cout << "Il n'y a pas de pairs connecté (rafraichir liste client ?)" << endl;
	      pthread_mutex_unlock(&f->donneeClients->getVerrou());
	      pthread_exit(par);
	    }
	  TableauClient listeClients;
	  DonneeClient * nouveauClient = new DonneeClient();
	  for(int it = 0; it < f->donneeClients->getDonnee().size(); it++)
	    { 
	      *nouveauClient = DonneeClient(f->donneeClients->getDonnee(it)->getIp(),
					    f->donneeClients->getDonnee(it)->getPort(),
					    f->donneeClients->getDonnee(it)->getDesc());

	      listeClients.pushClient(nouveauClient);
	    }
	  pthread_mutex_unlock(&f->donneeClients->getVerrou());

	  int client = 0;

	  for(int i = 0; i < nbPartition; i++)
	    {
	      if(client > listeClients.getDonnee().size()-1)
		{
		  client = 0;
		}

	      char nomFichierPart[255];
	      char cheminFichierEnvoiPart[255];
	      strcpy(cheminFichierEnvoiPart,cheminFichierEnvoi);
	      strcpy(nomFichierPart,f->nomFichier);

	      char partition[5];
	      sprintf(partition,"%d",i);
	      strcat(cheminFichierEnvoiPart,partition);
	      
	      strcat(nomFichierPart,partition);

	      cout << cheminFichierEnvoiPart << endl;
	      fstream fichierEnvoi(cheminFichierEnvoiPart, fstream::in);
	      if(!fichierEnvoi.good())
		{
		  fichierEnvoi.close();
		  perror("OuvertureFichier");
		}
	      else
		{
		  fichierEnvoi.seekg(0,fichierEnvoi.end);
		  int taille = fichierEnvoi.tellg();
		  fichierEnvoi.seekg(0,fichierEnvoi.beg);
		  		  
		  char * buffer = (char *)malloc(taille*sizeof(char));
		  struct protocoleEnvoieFichier p1;
		  p1.proto = 1;
		  p1.part = i;
		  p1.nbPartition = nbPartition;
		  p1.taille_nom = strlen(nomFichierPart);
		  p1.taille_fichier = taille;
		  strcpy(p1.n,nomFichierPart);
		  char c;
		  for(int i=strlen(nomFichierPart); i< taille+strlen(nomFichierPart); i++)
		    {
		      c = fichierEnvoi.get();
		      p1.n[i] = c;	
		    }
		  fichierEnvoi.close();
		  
		  Sock sockClient = Sock(SOCK_STREAM, 0);
		  if(sockClient.good()) listeClients.getDonnee(client)->setDesc(sockClient.getsDesc());
		  else
		    {
		      perror("ConnexionClient");
		      exit(1);
		    }
		  SockDist sockDistClient = SockDist(inet_ntoa(listeClients.getDonnee(client)->getIp()), (short)listeClients.getDonnee(client)->getPort());
		  adrSockPub = sockDistClient.getAdrDist();
		  lgAdrSockPub = sizeof(struct sockaddr_in);
		  
		  int connexion = connect(listeClients.getDonnee(client)->getDesc(),(struct sockaddr *)adrSockPub, lgAdrSockPub);
		  if(connexion != 0)
		    {
		      listeClients.getDonnee().erase(listeClients.getDonnee().begin() + client);
		      perror("Connexion pair echoue");
		      continue;
		    }
		  
		  write(listeClients.getDonnee(client)->getDesc(),&p1,5*sizeof(int)+strlen(nomFichierPart)+taille*sizeof(char));
		  cout << "Partition envoyé au client " << client << endl;
		  //perror("write");
		  free(buffer);
		  client++;
		}
	      cout << "Fichier envoyé" << endl;
	    }
	}
    }
  if(continuer)
    pthread_mutex_unlock(&mutexInfoFichier);//Si on a pas trouvé le fichier
}

 void recuperationPartition(int desc)
 {
   int part;
   read(desc,&part,4);
   int nbPartition;
   read(desc,&nbPartition,4);
   int taille_nom;
   read(desc,&taille_nom,4);
   int taille_fichier;
   read(desc,&taille_fichier,4);
   char nom[taille_nom+1];
   read(desc,nom,taille_nom);
   nom[taille_nom]='\0';
   char fichier[taille_fichier+1];
   read(desc,fichier,taille_fichier);
   fichier[taille_fichier]='\0';
   ecriturePartition(part,nom,fichier,taille_fichier,nbPartition);
   if(taille_nom < 10){
   cout<<"//////////////////////////////////"<<endl;
   cout<<"taille nom fichier "<<taille_nom<<endl;
   cout<<"NOM "<<nom<<endl;
   cout<<"taille fichier"<<taille_fichier<<endl;
   cout<<"//////////////////////////////////"<<endl;}
 }
void ecriturePartition(int part, char* nom, char* fichier, int taille,int nbPartition)
 {
   //cout<<"Le fichier "<<fichier<<endl;
   pthread_mutex_lock(&(mutexInfoFichier));
   int action = determineAction(nom,part,nbPartition);
   if(action)
     {
       ajouterPartition(nom,part,fichier,taille);
       
     }
   pthread_mutex_unlock(&(mutexInfoFichier));
 }
 void ecriturePartitionLeTempsQueLautreMecSeConnecteSurLeServeurCreeParLeClient(int part, char* nom, char* fichier)
 {
   //Prendre le verrou sur le fichier infofichiers.txt
   //Chercher le nom de fichier dans se fichier 
   //Si le fichier existe pas dans infofichier:
   ////L'ajouter dans infoFichier et crée le dossier correspondant
   //Sinon
   ////Regarder si la partition existe deja
   ////Si la partition existe
   //////S'arreter
   ////Sinon
   //////Ajouter la partition dans le dossier et dans infofichier.txt*
   /*pthread_mutex_lock(&(mutexInfoFichier));
   int action = choixAction(nom);
   if(action)
     {
     }
     pthread_mutex_unlock(&(mutexInfoFichier));*/
 }

void portIpClient(TableauClient* t, int desc)
{
  in_addr ip;
  int port;
  read(desc,&port,4);
  read(desc,&ip,4);
  DonneeClient * d = new DonneeClient(ip,port,-2);
  pthread_mutex_lock(&(t->getVerrou()));
  if(!(t->appartient(d)))
  {
    d->toString();
    t->pushClient(d);
  }
  else
    {
      delete d;
    }
  pthread_mutex_unlock(&(t->getVerrou()));
  
}

void recherchePartition(int desc)
{
  int part;
  int taille;
  int port;
  in_addr ip;
  read(desc,&part,4);
  read(desc,&taille,4);
  read(desc,&port,4);
  read(desc,&ip,4);
  
  char nom[taille+1];
  read(desc,nom,taille);
  nom[taille] ='\0';
  struct RecherchePartition* r =(struct RecherchePartition*)malloc(sizeof(struct RecherchePartition));
  r->part = part;
  r->taille = taille;
  r->port = port;
  r->ip = ip;
  strcpy(r->nom,nom);
  //cout << r->part << " " << r->taille << " " <<  r->port << " " << r->port << " " << r->ip << " " << r->nom << endl;
  pthread_t id;
  if(pthread_create(&id,NULL,threadRecherchePartition,(void*)r) != 0)
    {
      perror("pthread_create");
    }
}
void *threadRecherchePartition(void *par)
{
  struct RecherchePartition* rP= (struct RecherchePartition*)par;
  pthread_mutex_lock(&mutexInfoFichier);
  if(aPartition(rP->nom,rP->part))
    {
      cout << "coucou"<< endl;
      //Se connecter au mec
      //Envoyer partition
    }
  pthread_mutex_unlock(&mutexInfoFichier);
  
}
void *threadReceptionServeurPrin(void *par)
{
  struct tableauDescServeur *  descServeur = (struct tableauDescServeur *)par;
  int desc = descServeur->descServeur;
  TableauClient *t = descServeur->donneeClients;
  int proto;
  int fermeture;
  while((fermeture = read(desc,&proto,4)) > 0)
    {
      switch(proto)
	{
	case 2:
	  {
	    portIpClient(t,desc);
	    break;
	  }
	case 3:
	  {
	    recherchePartition(desc);
	    break;
	  }
	}
    }
  if(fermeture == 0)
    {
      perror("ThreadReceptionServeurPrin");
    }
  if(fermeture == -1)
    {
      perror("ThreadReceptionServeurPrin");
    }
  free(descServeur);
}

pthread_t* creationThreadClient(TableauClient* donneeClients,int descBrCircuitVirtuel,struct sockaddr_in& adresse)
{
  pthread_t* id = (pthread_t*)malloc(sizeof(pthread_t));
  struct DescTableauClient* parametreThread = (struct DescTableauClient*)malloc(sizeof(struct DescTableauClient));
  parametreThread->donneeClients = donneeClients;
  parametreThread->descClient = descBrCircuitVirtuel;
  parametreThread->adresse = adresse.sin_addr;
  if(pthread_create(id,NULL,threadClient,(void*)parametreThread) != 0)
    {
      cout<<"Erreut thread creation"<<endl;
      return NULL;
    }
  else
    {
      return id;
    }
}

void suppresionClient(DonneeClient* donnee_client,struct DescTableauClient* parametreClient,int isPresent)
{
  cout<<"Suppresion client"<<endl;
  if(isPresent == -1)
    perror("read");
  close(parametreClient->descClient);
  pthread_mutex_lock(&(parametreClient->donneeClients->getVerrou()));
  int rang = -1;
  if(donnee_client != NULL)
    rang =  parametreClient->donneeClients->rangClient(donnee_client); 
  cout<<"rang : "<<rang<<endl;
  if(rang != -1)
    parametreClient->donneeClients->rmClient(rang);
  pthread_mutex_unlock(&(parametreClient->donneeClients->getVerrou()));
  free(parametreClient);
}

void receptionPartitionManquante(int desc)
{
  int part;
  int nbPartition = -1;
  int taille_nom;
  int taille_fichier;
  read(desc,&part,4);
  read(desc,&taille_nom,4);
  read(desc,&taille_fichier,4);
  char nom[taille_nom+1];
  char fichier[taille_fichier+1];
  read(desc,nom,taille_nom);
  read(desc,fichier,taille_fichier);
  nom[taille_nom]='\0';
  fichier[taille_fichier]='\0';
  ecriturePartition(part,nom,fichier,taille_fichier,nbPartition);
  

  
  char p[255];
  sprintf(p,"%d",part);
  char partChar[255] = "\\";
  strcat(partChar,p);
  strcat(partChar,"\\");
  char n[taille_nom];
  strcpy(n,nom);
  n[strlen(nom) - strlen(p)] = '\0';
  if(partitionManquante(n).empty())
    {
      regroupePartition(n);
    }
}
