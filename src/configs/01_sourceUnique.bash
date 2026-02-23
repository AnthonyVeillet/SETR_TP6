#!/bin/bash

# Ce script assume :
#   - Qu'il est exécuté dans le même répertoire que les fichiers exécutables
#   - Que les vidéos sont situés dans des dossiers *p (par exemple 160p) dans le même répertoire

sudo rm /dev/shm/mem*           # On retire les identifiants des zones mémoire partagées des précédentes exécutions
echo "[Script] 01_sourceUnique"
echo "[Script] Lancement decodeur"
sudo ./decodeur 240p/02_Sintel.ulv /mem1 &
echo "[Script] En attente de creation de /mem1"
while [ ! -f /dev/shm/mem1 ]
do
    sleep 0.05
done
echo "[Script] /mem1 cree, lancement compositeur"
sudo ./compositeur /mem1 &
wait;

