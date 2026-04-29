/******************************************************************************
* Laboratoire 3 / TP6
* GIF-3004 Systèmes embarqués temps réel
* Hiver 2026
* Marc-André Gardner
*
* Programme compositeur (version TP6 V3 - mono-flux optimisee)
*
* Recupere UN flux video a partir d'un espace memoire partage et l'affiche
* directement dans le framebuffer de la carte graphique.
******************************************************************************/

/* DEBUT Tony TP6 V3 */
/* Optimisations vs V2 (mono-flux non-optimisee) :
 *
 * (1) PRINCIPALE - Synchronisation bloquante :
 *     attenteLecteurAsync (polling avec sleep+yield) -> attenteLecteur
 *     Le compositeur dort sur la condvar entre frames (CPU = 0 %), reveille
 *     uniquement quand le decodeur signale. Gain enorme en consommation,
 *     objectif principal du TP6.
 *
 * (2) Suppression du FPS cap interne :
 *     fpsCap, framePeriod, nextDisplay, anti-drift loop -> retires.
 *     Le decodeur dicte le rythme via la condvar (producer/consumer).
 *
 * (3) Suppression de la stratégie sleep/sched_yield :
 *     plus de calcul de nextEvent ni de usleep/sched_yield, on dort
 *     directement sur pthread_cond_wait dans attenteLecteur.
 *
 * (4) Suppression de gotFrame/newFrame/besoinFlush :
 *     devenus inutiles - apres attenteLecteur, on a TOUJOURS une nouvelle
 *     frame a afficher.
 *
 * (5) ecrireImage optimisee :
 *     memcpy unique si fbLineLength == largeurSource*3 (pas de padding).
 *     Sinon ligne par ligne comme avant. Pre-calcul de bytesPerLine.
 *
 * (6) Retrait du profilage TP3 :
 *     evenementProfilage, initProfilage, InfosProfilage retires du
 *     compositeur. PROFILAGE_ACTIF=0 dans utils.h les rendait deja no-op,
 *     retrait pour clarte du code TP6.
 *
 * (7) Includes inutilises retires : sys/types.h, sys/stat.h, linux/kd.h,
 *     err.h, errno.h.
 *
 * Conserves a l'identique : lecture mutex puis copie locale + signal
 * (libere le decodeur rapidement), conversion gris->BGR, double-buffering
 * page-flip via FBIOPAN_DISPLAY, format stats.txt, ordonnancement.
 */
/* FIN Tony TP6 V3 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/time.h>

#include <sched.h>

// Allocation memoire, mmap et mlock
#include <sys/mman.h>

// Gestion des ressources et permissions
#include <sys/resource.h>

// Mesure du temps
#include <time.h>

// Controle de la console (framebuffer)
#include <linux/fb.h>

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"
#include <getopt.h>


// Fonction permettant de recuperer le temps courant sous forme double
double get_time()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return (double)t.tv_sec + (double)(t.tv_usec)*1e-6;
}


/* DEBUT Tony TP6 V3 */
/* ecrireImage : copie BGR -> framebuffer.
 * Optimisation : si fbLineLength == largeurSource*3 (pas de padding entre
 * lignes du framebuffer), un seul memcpy au lieu de hauteurSource petits.
 */
static void ecrireImage(unsigned char* fb, size_t hauteurFB, int fbLineLength,
                        int currentPage,
                        const unsigned char *dataBGR,
                        size_t hauteurSource, size_t largeurSource)
{
    unsigned char *currentFramebuffer = fb + currentPage * fbLineLength * hauteurFB;
    const size_t bytesPerLine = largeurSource * 3;

    if ((size_t)fbLineLength == bytesPerLine) {
        memcpy(currentFramebuffer, dataBGR, bytesPerLine * hauteurSource);
    } else {
        for (unsigned int ligne = 0; ligne < hauteurSource; ligne++) {
            memcpy(currentFramebuffer + ligne * fbLineLength,
                   dataBGR + ligne * bytesPerLine,
                   bytesPerLine);
        }
    }
}

/* Page-flip apres avoir dessine la frame courante. */
static void flushFramebuffer(int fbfd, struct fb_var_screeninfo *vinfoPtr,
                             int *currentPagePtr)
{
    int currentPage = *currentPagePtr;
    vinfoPtr->yoffset = currentPage * vinfoPtr->yres;
    vinfoPtr->activate = FB_ACTIVATE_VBL;
    if (ioctl(fbfd, FBIOPAN_DISPLAY, vinfoPtr)) {
        printf("Erreur lors du changement de buffer (double buffering inactif)!\n");
    }
    *currentPagePtr = (currentPage + 1) % 2;
}
/* FIN Tony TP6 V3 */


int main(int argc, char* argv[])
{
    char *entree = NULL;
    struct memPartage zone = {0};
    uint32_t largeurVideo = 0, hauteurVideo = 0, canauxVideo = 0, fpsVideo = 0;

    setbuf(stdout, NULL);

    // Code lisant les options sur la ligne de commande
    struct SchedParams schedParams = {0};

    if (argc < 2) {
        printf("Nombre d'arguments insuffisant\n");
        return -1;
    }

    if (strcmp(argv[1], "--debug") == 0) {
        printf("Mode debug selectionne pour le compositeur\n");
        entree = (char*)"/mem1";

        printf("Initialisation compositeur, entree=%s, mode d'ordonnancement=%i\n",
               entree, schedParams.modeOrdonnanceur);

        if (initMemoirePartageeLecteur(entree, &zone) != 0) {
            printf("Erreur d'initialisation memoire partagee lecteur\n");
            return -1;
        }
    }
    else {
        int c;
        opterr = 0;

        while ((c = getopt(argc, argv, "s:d:")) != -1) {
            switch (c) {
                case 's': parseSchedOption(optarg, &schedParams); break;
                case 'd': parseDeadlineParams(optarg, &schedParams); break;
                default: continue;
            }
        }

        if (argc - optind != 1) {
            printf("Usage: %s [-s SCHED] [-d r,d,p] fichier_entree\n", argv[0]);
            return -1;
        }

        entree = argv[optind];
        printf("Initialisation compositeur, entree=%s, mode d'ordonnancement=%i\n",
               entree, schedParams.modeOrdonnanceur);

        if (initMemoirePartageeLecteur(entree, &zone) != 0) {
            printf("Erreur d'initialisation memoire partagee lecteur\n");
            return -1;
        }
    }

    largeurVideo = zone.header->infos.largeur;
    hauteurVideo = zone.header->infos.hauteur;
    canauxVideo  = zone.header->infos.canaux;
    fpsVideo     = zone.header->infos.fps;

    if (fpsVideo == 0) {
        printf("Le nombre de FPS du video doit etre superieur a 0\n");
        return -1;
    }
    if (canauxVideo != 1 && canauxVideo != 3) {
        printf("Format de video non supporte, seulement les formats gris ou BGR\n");
        return -1;
    }
    if (largeurVideo != 427 || hauteurVideo != 240) {
        printf("Format de video non supporte, seulement les videos en 427x240 sont supportees\n");
        return -1;
    }
    (void)fpsVideo; // utilisee uniquement pour validation, plus pour le timing

    // Changement de mode d'ordonnancement
    if (appliquerOrdonnancement(&schedParams, "compositeur") != 0) {
        printf("Erreur appliquerOrdonnancement\n");
        return -1;
    }

    // Initialisation des structures necessaires a l'affichage
    long int screensize = 0;
    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        perror("Erreur lors de l'ouverture du framebuffer ");
        return -1;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_var_screeninfo orig_vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Erreur lors de la requete d'informations sur le framebuffer ");
    }

    memcpy(&orig_vinfo, &vinfo, sizeof(struct fb_var_screeninfo));

    /* Resolution figee a 427x240 (1 seul flux). */
    vinfo.bits_per_pixel = 24;
    vinfo.xres = 427;
    vinfo.yres = 240;

    vinfo.xres_virtual = vinfo.xres;
    vinfo.yres_virtual = vinfo.yres * 2;
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo)) {
        perror("Erreur lors de l'appel a ioctl ");
    }

    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Erreur lors de l'appel a ioctl (2) ");
    }

    screensize = finfo.smem_len;
    unsigned char *fbp = (unsigned char*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

    if (fbp == MAP_FAILED) {
        perror("Erreur lors du mmap de l'affichage ");
        return -1;
    }

    //=====================================
    // Definition des elements avant boucle
    //=====================================

    FILE *fstats = fopen("stats.txt", "w");
    if (fstats == NULL) {
        perror("fopen stats.txt");
        return -1;
    }
    setbuf(fstats, NULL);

    double debutCompositeur = get_time();

    size_t pixels = (size_t)largeurVideo * (size_t)hauteurVideo;

    size_t maxFrameIn = 427u * 240u * 3u;
    if (prepareMemoire(maxFrameIn, maxFrameIn) != 0) {
        printf("Erreur prepareMemoire\n");
        return -1;
    }

    struct rlimit rl = { .rlim_cur = RLIM_INFINITY, .rlim_max = RLIM_INFINITY };
    setrlimit(RLIMIT_MEMLOCK, &rl);
    mlockall(MCL_CURRENT | MCL_FUTURE);

    // Buffer BGR local : on copie la zone partagee dedans pour relacher le
    // mutex le plus vite possible et laisser le decodeur produire la frame
    // suivante en parallele de l'affichage.
    unsigned char* lastFrameBGR = (unsigned char*)tempsreel_malloc(pixels * 3u);
    if (!lastFrameBGR) {
        perror("tempsreel_malloc lastFrameBGR");
        return -1;
    }

    /* DEBUT Tony TP6 V3 */
    // Etat des stats : reduit aux variables strictement necessaires.
    // Plus de fpsCap/framePeriod/nextDisplay : la cadence est dictee par
    // la condvar (decodeur = producteur).
    double winStart = debutCompositeur;
    double lastDisplayed = -1.0;
    double maxDt = 0.0;
    int framesWin = 0;
    double nextStatsWrite = debutCompositeur + 5.0;

    int currentPage = 0;
    /* FIN Tony TP6 V3 */

    while(1) {
        /* DEBUT Tony TP6 V3 */
        // ------------------------
        // 1) ATTENTE BLOQUANTE : se reveille uniquement quand le decodeur
        //    signale qu'une frame est prete. CPU dort entre frames.
        //    Au retour, le mutex est LOCKE.
        // ------------------------
        attenteLecteur(&zone);

        double now = get_time();

        // Copie locale puis signal pour liberer le decodeur au plus vite.
        if (canauxVideo == 3) {
            memcpy(lastFrameBGR, zone.data, pixels * 3u);
        } else {
            // GRIS -> BGR (canauxVideo == 1, deja valide en init)
            const unsigned char* src = zone.data;
            unsigned char* dst = lastFrameBGR;
            for (size_t p = 0; p < pixels; p++) {
                unsigned char g = src[p];
                *dst++ = g; *dst++ = g; *dst++ = g;
            }
        }
        signalLecteur(&zone);

        // ------------------------
        // 2) AFFICHAGE
        // ------------------------
        ecrireImage(fbp, vinfo.yres, finfo.line_length,
                    currentPage,
                    lastFrameBGR,
                    hauteurVideo, largeurVideo);

        flushFramebuffer(fbfd, &vinfo, &currentPage);

        // ------------------------
        // 3) STATS - chaque frame compte (plus de gating sur newFrame)
        // ------------------------
        framesWin++;
        if (lastDisplayed > 0.0) {
            double dt = now - lastDisplayed;
            if (dt > maxDt) maxDt = dt;
        }
        lastDisplayed = now;

        // ------------------------
        // 4) stats.txt toutes 5 sec
        //    Format inchange : "[X.X] Entree 1: moy=Y.Y fps, max=Z.Z ms"
        // ------------------------
        if (now >= nextStatsWrite) {
            double elapsed = now - debutCompositeur;
            double winDur = now - winStart;
            double moy = (winDur > 0.0) ? ((double)framesWin / winDur) : 0.0;

            fprintf(fstats, "[%.1f] Entree 1: moy=%.1f fps, max=%.1f ms\n",
                    elapsed, moy, maxDt * 1000.0);

            winStart = now;
            framesWin = 0;
            maxDt = 0.0;
            nextStatsWrite += 5.0;
        }
        /* FIN Tony TP6 V3 */
    }

    // cleanup (jamais atteint avec while(1), garde par securite)
    munmap(fbp, screensize);

    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo)) {
        printf("Error re-setting variable information.\n");
    }
    close(fbfd);
    fclose(fstats);

    return 0;
}