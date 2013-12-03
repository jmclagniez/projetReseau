#include <iostream>
#include <vector>
using namespace std;

#include "reseau/sock.h"
#include "reseau/sockdist.h"
#include "threadData/DonneeThread.h"
#include "threadData/TableauThread.h"
#include "clientData/DonneeClient.h"
#include "clientData/TableauClient.h"
#include "Serveur.h"

int main()
{
  Serveur* s = new Serveur();
  s->init_serveur();
  delete s;
  return 0;
}
