#ifndef FICHIERS_H
#define FICHIERS_H


int determineAction(char*, int,int);
int decouperFichier(const char*,int);
void ajouterPartition(char*,int,char*,int);
int suiteCharBuffer(char* buffer,char sep);
void regroupePartition(char*,int);
vector<int> partitionManquante(char*);
bool aPartition(char*, int);
#endif
