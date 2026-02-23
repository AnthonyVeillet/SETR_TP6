#!/bin/bash

# Ce script assume :
#   - Qu'il est exécuté dans le même répertoire que les fichiers exécutables
#   - Que les vidéos sont situés dans des dossiers *p (par exemple 160p) dans le même répertoire

sudo rm /dev/shm/mem*           # On retire les identifiants des zones mémoire partagées des précédentes exécutions
echo "[Script] 07_deuxVideosGris"
echo "[Script] Lancement decodeurs"
sudo ./decodeur 240p/02_Sintel.ulv /mem1 &
sudo ./decodeur 240p/01_ToS.ulv /mem2 &
echo "[Script] En attente de creation de /mem1 et /mem2"
while [ ! -f /dev/shm/mem1 ] || [ ! -f /dev/shm/mem2 ]
do
    sleep 0.05
done
echo "[Script] /mem1 et /mem2 crees, lancement convertisseur niveau de gris"
sudo ./convertisseur /mem1 /mem3 &
sudo ./convertisseur /mem2 /mem4 &

echo "[Script] En attente de creation de /mem3 et /mem4"
while [ ! -f /dev/shm/mem3 ] || [ ! -f /dev/shm/mem4 ]
do
    sleep 0.05
done
echo "[Script] /mem3 et /mem4 crees, lancement compositeur"
sudo ./compositeur /mem3 /mem4 &

wait;

