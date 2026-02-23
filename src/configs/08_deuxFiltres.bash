#!/bin/bash

# Ce script assume :
#   - Qu'il est exécuté dans le même répertoire que les fichiers exécutables
#   - Que les vidéos sont situés dans des dossiers *p (par exemple 160p) dans le même répertoire

sudo rm /dev/shm/mem*           # On retire les identifiants des zones mémoire partagées des précédentes exécutions
echo "[Script] 08_deuxFiltres"
echo "[Script] Lancement decodeurs"
sudo ./decodeur 160p/02_Sintel.ulv /mem1 &
sudo ./decodeur 160p/02_Sintel.ulv /mem5 &
echo "[Script] En attente de creation de /mem1 et /mem5"
while [ ! -f /dev/shm/mem1 ] || [ ! -f /dev/shm/mem5 ]
do
    sleep 0.05
done
echo "[Script] /mem1 et /mem5 crees, lancement convertisseur niveau de gris"
sudo ./convertisseur /mem1 /mem2 &
sudo ./convertisseur /mem5 /mem6 &
echo "[Script] En attente de creation de /mem2 et /mem6"
while [ ! -f /dev/shm/mem2 ] || [ ! -f /dev/shm/mem6 ]
do
    sleep 0.05
done
echo "[Script] /mem2 et /mem6 crees, lancement filtreur"
sudo ./filtreur -f 0 /mem2 /mem3 &
sudo ./filtreur -f 1 /mem6 /mem7 &
echo "[Script] En attente de creation de /mem3 et /mem7"
while [ ! -f /dev/shm/mem3 ] || [ ! -f /dev/shm/mem7 ]
do
    sleep 0.05
done
echo "[Script] /mem3 et /mem7 crees, lancement redimensionneur"
sudo ./redimensionneur -w 427 -h 240 -r 0 /mem3 /mem4 &
sudo ./redimensionneur -w 427 -h 240 -r 0 /mem7 /mem8 &
echo "[Script] En attente de creation de /mem4 et /mem8"
while [ ! -f /dev/shm/mem4 ] || [ ! -f /dev/shm/mem8 ]
do
    sleep 0.05
done
echo "[Script] /mem4 et /mem8 crees, lancement compositeur"
sudo ./compositeur /mem4 /mem8 &

wait;
