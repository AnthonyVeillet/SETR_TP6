#!/bin/bash

# Ce script assume :
#   - Qu'il est exécuté dans le même répertoire que les fichiers exécutables
#   - Que les vidéos sont situés dans des dossiers *p (par exemple 160p) dans le même répertoire

sudo rm /dev/shm/mem*           # On retire les identifiants des zones mémoire partagées des précédentes exécutions
echo "[Script] 05_grosseVideo"
echo "[Script] Lancement decodeur"
sudo ./decodeur 480p/02_Sintel.ulv /mem1 &
echo "[Script] En attente de creation de /mem1"
while [ ! -f /dev/shm/mem1 ]
do
    sleep 0.05
done
echo "[Script] /mem1 cree, lancement redimensionneur"
sudo ./redimensionneur -w 427 -h 240 -r 0 /mem1 /mem2 &
echo "[Script] En attente de creation de /mem2"
while [ ! -f /dev/shm/mem2 ]
do
    sleep 0.05
done
echo "[Script] /mem2 cree, lancement compositeur"
sudo ./compositeur /mem2 &
wait;

