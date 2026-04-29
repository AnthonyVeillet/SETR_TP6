/******************************************************************************
 * Laboratoire 3 / TP6
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 *
 * Version simplifiee TP6 : retrait de toutes les fonctions de traitement
 * d'image (filtrage, redimensionnement, conversion gris, helpers internes).
 ******************************************************************************/
#define _GNU_SOURCE
#include "utils.h"

/* DEBUT Tony TP6 V2 */
/* Fichier simplifie pour TP6.
 * Retraits :
 *   - macros min/max (utilisees uniquement par _convolve / highpassFilter)
 *   - lowpassFilter, highpassFilter
 *   - resizeNearestNeighborInit, resizeNearestNeighbor
 *   - resizeBilinearInit, resizeBilinear, resizeDestroy
 *   - convertToGray
 *   - enregistreImage
 *   - helpers internes : _convolve, _permuteRGB, _permuteRGB_char,
 *     _unpermuteRGB, _createGaussianKernel, _destroyKernel,
 *     _ul_nearestneighbors_regulargrid, _ul_bilinear_regulargrid,
 *     _createGrid, _createGridFloat, macros XORSWAP
 *
 * Conserves : appliquerOrdonnancement, parseSchedOption, parseDeadlineParams,
 *             initProfilage, evenementProfilage. Comportement strictement
 *             identique a la version originale.
 */
/* FIN Tony TP6 V2 */


// Applique les paramètres d'ordonnancement au processus courant
int appliquerOrdonnancement(const struct SchedParams* p, const char* nomProg)
{
    if (!p) return -1;

    if (p->modeOrdonnanceur == ORDONNANCEMENT_NORT) {
        return 0;
    }

    if (p->modeOrdonnanceur == ORDONNANCEMENT_RR) {
        struct sched_param sp;
        memset(&sp, 0, sizeof(sp));
        sp.sched_priority = 99;

        if (sched_setscheduler(0, SCHED_RR, &sp) != 0) {
            fprintf(stderr, "[%s] ERREUR sched_setscheduler(RR): %s\n",
                    nomProg ? nomProg : "prog", strerror(errno));
            return -1;
        }

        printf("Mode d'operation du scheduler modifie avec succes pour %s (RR).\n",
               nomProg ? nomProg : "prog");
        return 0;
    }

    if (p->modeOrdonnanceur == ORDONNANCEMENT_FIFO) {
        struct sched_param sp;
        memset(&sp, 0, sizeof(sp));
        sp.sched_priority = 99;

        if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
            fprintf(stderr, "[%s] ERREUR sched_setscheduler(FIFO): %s\n",
                    nomProg ? nomProg : "prog", strerror(errno));
            return -1;
        }

        printf("Mode d'operation du scheduler modifie avec succes pour %s (FIFO).\n",
               nomProg ? nomProg : "prog");
        return 0;
    }

    if (p->modeOrdonnanceur == ORDONNANCEMENT_DEADLINE) {
        fprintf(stderr, "[%s] DEADLINE pas géré ici (à implémenter via sched_setattr)\n",
                nomProg ? nomProg : "prog");
        return -1;
    }

    return -1;
}

// Parse l'option -s
int parseSchedOption(const char* arg, struct SchedParams* params) {
    if (strcmp(arg, "NORT") == 0) {
        params->modeOrdonnanceur = ORDONNANCEMENT_NORT;
    } else if (strcmp(arg, "RR") == 0) {
        params->modeOrdonnanceur = ORDONNANCEMENT_RR;
    } else if (strcmp(arg, "FIFO") == 0) {
        params->modeOrdonnanceur = ORDONNANCEMENT_FIFO;
    } else if (strcmp(arg, "DEADLINE") == 0) {
        params->modeOrdonnanceur = ORDONNANCEMENT_DEADLINE;
    } else {
        params->modeOrdonnanceur = ORDONNANCEMENT_NORT;
        printf("Mode d'ordonnancement %s non valide, defaut sur NORT\n", arg);
        return -1;
    }
    return 0;
}

// Parse l'option -d
int parseDeadlineParams(char* arg, struct SchedParams* params) {
    int paramIndex = 0;
    char* splitString = strtok(arg, ",");
    while (splitString != NULL) {
        unsigned int value = (unsigned int)atoi(splitString);
        if (paramIndex == 0) {
            params->runtime = value;
        } else if (paramIndex == 1) {
            params->deadline = value;
        } else {
            params->period = value;
            break;
        }
        paramIndex++;
        splitString = strtok(NULL, ",");
    }
    return 0;
}


void initProfilage(InfosProfilage *dataprof, const char *chemin_enregistrement){
    if(PROFILAGE_ACTIF == 0){
        return;
    }
    dataprof->fd = fopen(chemin_enregistrement, "w+");
    dataprof->derniere_sauvegarde = 0;
    dataprof->dernier_etat = ETAT_INDEFINI;

    dataprof->data = (char*)calloc(PROFILAGE_TAILLE_INIT, sizeof(char));
    memset(dataprof->data, 0, PROFILAGE_TAILLE_INIT * sizeof(char));

    dataprof->length = PROFILAGE_TAILLE_INIT;
    dataprof->pos = 0;
}

void evenementProfilage(InfosProfilage *dataprof, unsigned int type){
    if(PROFILAGE_ACTIF == 0){
        return;
    }

    struct timespec temps_courant;
    clock_gettime(CLOCK_MONOTONIC, &temps_courant);
    double multiplier = 1000000000.;
    int c;

    if(type == dataprof->dernier_etat){
        return;
    }

    dataprof->dernier_etat = type;

    if(dataprof->pos + 100 > dataprof->length){
        dataprof->data = (char*)realloc(dataprof->data, dataprof->length*2 * sizeof(char));
        dataprof->length *= 2;
    }

    c = sprintf(dataprof->data + dataprof->pos, "%u,%f\n", type, multiplier * temps_courant.tv_sec + temps_courant.tv_nsec);
    dataprof->pos += c;

    if(dataprof->derniere_sauvegarde == 0 || temps_courant.tv_sec - dataprof->derniere_sauvegarde > PROFILAGE_INTERVALLE_SAUVEGARDE_SEC){
        dataprof->derniere_sauvegarde = temps_courant.tv_sec;
        fwrite(dataprof->data, dataprof->pos, 1, dataprof->fd);
        fflush(dataprof->fd);
        dataprof->pos = 0;
    }
}