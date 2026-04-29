/*****************************************************************************
*
* Fonctions utilitaires, laboratoire 3, SETR
*
* Marc-André Gardner, Hiver 2026
*
* Version simplifiee TP6 :
*   Toutes les fonctions de traitement d'image (filtrage, redimensionnement,
*   conversion gris, sauvegarde PPM) ont ete retirees car les exécutables qui
*   les utilisaient (filtreur, redimensionneur, convertisseurgris) sont retires
*   du build pour le TP6. Seules les fonctions necessaires a decodeur et
*   compositeur sont conservees.
******************************************************************************/
#ifndef UTILS_H
#define UTILS_H

/* DEBUT Tony TP6 V2 */
/* Fichier simplifie pour TP6.
 * Retraits : Kernel, ResizeGrid, lowpassFilter, highpassFilter,
 *            convertToGray, resize* (init/destroy/nn/bilinear),
 *            enregistreImage, ainsi que tous les helpers internes
 *            (_convolve, _permuteRGB, _createGaussianKernel, etc.).
 * Conserves : ordonnancement (parseSchedOption, parseDeadlineParams,
 *             appliquerOrdonnancement) et profilage (initProfilage,
 *             evenementProfilage), tous deux utilises par decodeur et
 *             compositeur.
 */
/* FIN Tony TP6 V2 */

// Permet de protéger le header lorsqu'il est inclus par un fichier C++
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>
#include <sched.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "allocateurMemoire.h"

#define ORDONNANCEMENT_NORT 0
#define ORDONNANCEMENT_RR 1
#define ORDONNANCEMENT_FIFO 2
#define ORDONNANCEMENT_DEADLINE 3

// Structure contenant les paramètres d'ordonnancement
struct SchedParams {
    int modeOrdonnanceur;
    unsigned int runtime;   // en millisecondes
    unsigned int deadline;  // en millisecondes
    unsigned int period;    // en millisecondes
};

// Parse l'option -s (NORT, RR, FIFO, DEADLINE)
int parseSchedOption(const char* arg, struct SchedParams* params);

// Parse l'option -d (runtime,deadline,period en millisecondes)
int parseDeadlineParams(char* arg, struct SchedParams* params);

// Applique les paramètres d'ordonnancement au processus courant
int appliquerOrdonnancement(const struct SchedParams* params, const char* nomProgramme);

#define ETAT_INDEFINI 0
#define ETAT_INITIALISATION 10
#define ETAT_ATTENTE_MUTEXLECTURE 20
#define ETAT_TRAITEMENT 30
#define ETAT_ATTENTE_MUTEXECRITURE 40
#define ETAT_ENPAUSE 50

// Mettre a zero pour desactiver le profilage
#define PROFILAGE_ACTIF 0
#define PROFILAGE_INTERVALLE_SAUVEGARDE_SEC 4
#define PROFILAGE_TAILLE_INIT 30 * 5 * 30 * PROFILAGE_INTERVALLE_SAUVEGARDE_SEC * 4


typedef struct{
    char *data;
    size_t length;
    unsigned int pos;
    uint64_t derniere_sauvegarde;
    unsigned int dernier_etat;
    FILE* fd;
} InfosProfilage;

void initProfilage(InfosProfilage *dataprof, const char *chemin_enregistrement);
void evenementProfilage(InfosProfilage *dataprof, unsigned int type);


#ifdef __cplusplus
}
#endif

#endif