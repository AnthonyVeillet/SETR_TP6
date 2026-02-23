#!/bin/bash

# Ce script assume :
#   - Qu'il est exécuté dans le même répertoire que les fichiers exécutables
#   - Que les vidéos sont situés dans des dossiers *p (par exemple 160p) dans le même répertoire

sudo rm /dev/shm/mem*           # On retire les identifiants des zones mémoire partagées des précédentes exécutions
echo "[Script] 09_realtime4videos"
echo "[Script] Lancement decodeurs"
sudo ./decodeur -s RR 240p/02_Sintel.ulv /mem1 &
sudo ./decodeur 240p/01_ToS.ulv /mem3 &
sudo ./decodeur 240p/03_BigBuckBunny.ulv /mem4 &
sudo ./decodeur 240p/04_Caminandes.ulv /mem5 &
echo "[Script] En attente de creation de /mem1, /mem5, /mem3 et /mem4"
while [ ! -f /dev/shm/mem1 ] || [ ! -f /dev/shm/mem3 ] || [ ! -f /dev/shm/mem4 ] || [ ! -f /dev/shm/mem5 ]
do
    sleep 0.05
done
echo "[Script] /mem1, /mem5, /mem3 et /mem4 crees, lancement convertisseur niveau de gris"
sudo ./convertisseur -s RR /mem1 /mem2 &
echo "[Script] En attente de creation de /mem2"
while [ ! -f /dev/shm/mem2 ]
do
    sleep 0.05
done
echo "[Script] /mem2 cree, lancement compositeur"
sudo ./compositeur -s RR /mem2 /mem3 /mem4 /mem5 &

wait;

