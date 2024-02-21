=================================TEMA-1 - APD===================================

Tema a constat in paralelizarea algorimtmului de image-processing "Marching
Squares". Pentru a realiza acest lucru, porning de la implementarea data, cea
secventiala, am folosit pthreads pentru a imparti sarcinile de procesare. Am
creat o functie pe care o atribui threadurilor la crearea lor, functia f, care
inglobeaza cei doi pasi principali ai algoritmului, cat si rescalarea imaginii
date. Pentru ca sunt multe argumente pe care threadurile trebuie sa le aiba, am
creat o structura de date thread_args. In main, inaintea de crearea threadurilor
aloc memorie pentru un array de asemenea structuri, pentru ca, la apelarea lui
pthread_create(), sa atribui fiecarui thread o structura de thread_args pe care
sa lucreze.

Important aici de mentionat ar fi ca un thread primeste imaginea citita(t->img).
De asemenea exista un element al structurii, scaled_image, care, va pointa catre
chiar aceeasi imagine citita in main, in cazul in care aceasta are dimensiunile
necesare, sau va fi pur si simplu alocat spatiu, pentru a fi folosit in functia f.

In f, verific daca este nevoie de rescalare, daca da, il aplic paralelizand
iteratia exterioara folosind start si end si salvez rezultatul rescalrii in urma
aplicarii functie sample_bicubic pe imaginea initiala, in scaled_image.
Pentru cei doi pasi de la MArching squares paralizez asemenea, cu start si end,
grija sa nu existe conflicte intre threaduri. Gridul folosit in acesti pasi este
trimis prin structura de date si alocat dinamic in main, inainte de a fi asignat
structurii. Intre cele cei parti ale functiei f(rescale, sample_grid si march),
folosesc bariere, pentru a asigura ca toate threadurile au facut pasul anterior
inainte de a-l face pe cel curent.

Bariera este si ea trimisa ca parametru prin structura si este initializata
corespunzator inainte de asignarea in structura.

La final, pentru a evita memory leaks, dezaloc toata memoria alocata dinamic,
folosind functia free din varianta secventiala si aplicand free pe structura de
date. Distrug si bariera pentru ca nu mai este nevoie de ea.
