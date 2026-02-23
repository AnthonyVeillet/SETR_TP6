#!/bin/bash

# Ce script assume :
#   - Qu'il est exécuté dans le même répertoire que les fichiers exécutables
#   - Que les vidéos sont situés dans des dossiers *p (par exemple 160p) dans le même répertoire

sudo rm /dev/shm/mem*           # On retire les identifiants des zones mémoire partagées des précédentes exécutions

echo "[Script] 06_troisPetitesVideosRedimensionnees"
echo "[Script] Lancement decodeurs"
sudo ./decodeur 160p/03_BigBuckBunny.ulv /mem1 &
sudo ./decodeur 160p/05_Llama_drama.ulv /mem3 &
sudo ./decodeur 160p/01_ToS.ulv /mem5 &
echo "[Script] En attente de creation de /mem1, /mem3 et /mem5"
while [ ! -f /dev/shm/mem1 ] || [ ! -f /dev/shm/mem3 ] || [ ! -f /dev/shm/mem5 ]
do
    sleep 0.05
done
echo "[Script] /mem1, /mem3 et /mem5 crees, lancement redimensionneurs"
sudo ./redimensionneur -w 427 -h 240 -r 1 /mem1 /mem2 &
sudo ./redimensionneur -w 427 -h 240 -r 1 /mem3 /mem4 &
sudo ./redimensionneur -w 427 -h 240 -r 1 /mem5 /mem6 &
echo "[Script] En attente de creation de /mem2, /mem4 et /mem6"
while [ ! -f /dev/shm/mem2 ] || [ ! -f /dev/shm/mem4 ] || [ ! -f /dev/shm/mem6 ]
do
    sleep 0.05
done
echo "[Script] /mem2, /mem4 et /mem6 crees, lancement compositeur"
sudo ./compositeur /mem2 /mem4 /mem6 &
wait;

