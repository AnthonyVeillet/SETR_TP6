/******************************************************************************
* Laboratoire 3 / TP6
* GIF-3004 Systèmes embarqués temps réel
* Hiver 2026
* Marc-André Gardner
*
* Programme compositeur (version simplifiee TP6 - mono-flux)
*
* Recupere UN flux video a partir d'un espace memoire partage et l'affiche
* directement dans le framebuffer de la carte graphique.
*
* IMPORTANT : CE CODE ASSUME QUE LE FLUX RECU EST EN 427x240 (1 ou 3 canaux,
* BGR si 3 canaux). TOUTE AUTRE TAILLE ENTRAINERA UN COMPORTEMENT INDEFINI.
*
* Le code permettant l'affichage est inspire de celui presente sur le blog
* Raspberry Compote, par J-P Rosti, publie sous la licence CC-BY 3.0.
* Merci a Yannick Hold-Geoffroy pour l'aide apportee pour la gestion
* du framebuffer.
******************************************************************************/

/* DEBUT Tony TP6 V2 */
/* Version mono-flux pour le TP6.
 * Retraits par rapport a la version multi-flux (TP3) :
 *   - logique pour 2, 3 et 4 entrees (parsing, init memoire, switch resolution)
 *   - tableaux [4] (zones, w, h, ch, fps, lastFrameBGR, etc.) -> scalaires
 *   - nbrActifs (toujours = 1)
 *   - imageGlobale et tony_v1_getImageGlobale (plus utiles : on ecrit
 *     directement dans le framebuffer car 1 seul flux)
 *   - branches total==2/3/4 dans ecrireImage (gardee uniquement total==1)
 *
 * Conserves a l'identique : synchronisation mutex (attenteLecteurAsync +
 * signalLecteur), conversion gris->BGR locale, double-buffering page-flip
 * via FBIOPAN_DISPLAY, FPS cap anti-drift, format de stats.txt
 * ("[X.X] Entree 1: moy=Y.Y fps, max=Z.Z ms"), strategie d'attente
 * (usleep/sched_yield), ordonnancement.
 */
/* FIN Tony TP6 V2 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <sched.h>

// Allocation memoire, mmap et mlock
#include <sys/mman.h>

// Gestion des ressources et permissions
#include <sys/resource.h>

// Mesure du temps
#include <time.h>

// Obtenir la taille des fichiers
#include <sys/stat.h>

// Controle de la console
#include <linux/fb.h>
#include <linux/kd.h>

// Gestion des erreurs
#include <err.h>
#include <errno.h>

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


/* DEBUT Tony TP6 V2 */
/* ecrireImage simplifiee pour le cas mono-flux : on copie directement dans
 * le framebuffer ligne par ligne, sans imageGlobale ni offsets.
 *
 * Hypotheses simplificatrices :
 *   - 1 seule image a afficher par cycle (plus de position/total)
 *   - donnees deja en BGR 3 canaux (la conversion gris->BGR est faite en
 *     amont dans la boucle principale)
 */
static void ecrireImage(unsigned char* fb, size_t hauteurFB, int fbLineLength,
                        int currentPage,
                        const unsigned char *dataBGR,
                        size_t hauteurSource, size_t largeurSource)
{
    unsigned char *currentFramebuffer = fb + currentPage * fbLineLength * hauteurFB;

    for (unsigned int ligne = 0; ligne < hauteurSource; ligne++) {
        memcpy(currentFramebuffer + ligne * fbLineLength,
               dataBGR + ligne * largeurSource * 3,
               largeurSource * 3);
    }
}

/* Effectue la page-flip apres avoir dessine la frame courante.
 * En mono-flux, plus besoin de copier un imageGlobale ; on fait juste l'ioctl.
 */
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
/* FIN Tony TP6 V2 */

// Fonction helper
static inline int lecture_pret(int r) {
    return (r == 1);
}

int main(int argc, char* argv[])
{
    /* DEBUT Tony TP6 V2 */
    /* Mono-flux : une seule entree, une seule zone memoire partagee. */
    char *entree = NULL;
    struct memPartage zone = {0};
    uint32_t largeurVideo = 0, hauteurVideo = 0, canauxVideo = 0, fpsVideo = 0;
    /* FIN Tony TP6 V2 */

    // On desactive le buffering pour les printf()
    setbuf(stdout, NULL);

    // Initialise le profilage (no-op si PROFILAGE_ACTIF=0 dans utils.h)
    char signatureProfilage[128] = {0};
    char* nomProgramme = (argv[0][0] == '.') ? argv[0]+2 : argv[0];
    snprintf(signatureProfilage, 128, "profilage-%s-%u.txt", nomProgramme, (unsigned int)getpid());
    InfosProfilage profInfos;
    initProfilage(&profInfos, signatureProfilage);

    evenementProfilage(&profInfos, ETAT_INITIALISATION);

    // Code lisant les options sur la ligne de commande
    struct SchedParams schedParams = {0};
    unsigned int runtime, deadline, period;

    if(argc < 2){
        printf("Nombre d'arguments insuffisant\n");
        return -1;
    }

    /* DEBUT Tony TP6 V2 */
    /* Parsing simplifie : un seul fichier d'entree attendu. */
    if(strcmp(argv[1], "--debug") == 0){
        printf("Mode debug selectionne pour le compositeur\n");
        entree = (char*)"/mem1";
        runtime = 10;
        deadline = 20;
        period = 25;
        (void)runtime; (void)deadline; (void)period;

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

        while ((c = getopt(argc, argv, "s:d:")) != -1){
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
    /* FIN Tony TP6 V2 */

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

    /* DEBUT Tony TP6 V2 */
    /* Resolution figee a 427x240 (1 seul flux) - plus de switch sur nbrActifs. */
    vinfo.bits_per_pixel = 24;
    vinfo.xres = 427;
    vinfo.yres = 240;
    /* FIN Tony TP6 V2 */

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

    /* DEBUT Tony TP6 V2 */
    /* Etat mono-flux : tous les tableaux [4] sont remplaces par des scalaires. */
    size_t pixels = (size_t)largeurVideo * (size_t)hauteurVideo;

    size_t maxFrameIn = 427u * 240u * 3u;
    if (prepareMemoire(maxFrameIn, maxFrameIn) != 0) {
        printf("Erreur prepareMemoire\n");
        return -1;
    }

    struct rlimit rl = { .rlim_cur = RLIM_INFINITY, .rlim_max = RLIM_INFINITY };
    setrlimit(RLIMIT_MEMLOCK, &rl);
    mlockall(MCL_CURRENT | MCL_FUTURE);

    // Buffer BGR constant (3 canaux) - copie locale pour ne pas garder le mutex
    // pendant l'affichage
    unsigned char* lastFrameBGR = (unsigned char*)tempsreel_malloc(pixels * 3u);
    if (!lastFrameBGR) {
        perror("tempsreel_malloc lastFrameBGR");
        return -1;
    }

    int gotFrame = 0;
    int newFrame = 0;

    // FPS cap (periode)
    double fpsCap = (double)fpsVideo;
    double framePeriod = 1.0 / fpsCap;
    double nextDisplay = debutCompositeur;

    // Stats fenetre 5 sec
    double winStart = debutCompositeur;
    double lastDisplayed = -1.0;
    double maxDt = 0.0;
    int framesWin = 0;

    double nextStatsWrite = debutCompositeur + 5.0;

    int currentPage = 0;
    /* FIN Tony TP6 V2 */

    while(1) {
        evenementProfilage(&profInfos, ETAT_TRAITEMENT);
        double now = get_time();

        /* DEBUT Tony TP6 V2 */
        // ------------------------
        // 1) POLL NON-BLOQUANT : recuperer la nouvelle frame (1 seule source)
        // ------------------------
        evenementProfilage(&profInfos, ETAT_ATTENTE_MUTEXLECTURE);
        int r = attenteLecteurAsync(&zone);
        evenementProfilage(&profInfos, ETAT_TRAITEMENT);

        if (lecture_pret(r)) {
            // Mutex lecteur lock. On copie puis on libere.
            if (canauxVideo == 3) {
                // BGR -> BGR
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

            gotFrame = 1;
            newFrame = 1;
        }

        // ------------------------
        // 2) AFFICHAGE avec cap FPS
        // ------------------------
        int besoinFlush = 0;

        if (gotFrame && now >= nextDisplay) {
            ecrireImage(fbp, vinfo.yres, finfo.line_length,
                        currentPage,
                        lastFrameBGR,
                        hauteurVideo, largeurVideo);
            besoinFlush = 1;

            // Stats : compter seulement si nouvelle frame depuis le dernier affichage
            if (newFrame) {
                framesWin++;

                if (lastDisplayed > 0.0) {
                    double dt = now - lastDisplayed;
                    if (dt > maxDt) maxDt = dt;
                }
                lastDisplayed = now;
                newFrame = 0;
            }

            // Prochain affichage (anti-drift)
            do { nextDisplay += framePeriod; } while (nextDisplay <= now);
        }

        if (besoinFlush) {
            flushFramebuffer(fbfd, &vinfo, &currentPage);
        }

        // ------------------------
        // 3) stats.txt toutes 5 sec
        //    Format inchange : "[X.X] Entree 1: moy=Y.Y fps, max=Z.Z ms"
        // ------------------------
        if (now >= nextStatsWrite) {
            double elapsed = now - debutCompositeur;
            double winDur = now - winStart;
            double moy = (winDur > 0.0) ? ((double)framesWin / winDur) : 0.0;

            fprintf(fstats, "[%.1f] Entree 1: moy=%.1f fps, max=%.1f ms\n",
                    elapsed, moy, maxDt * 1000.0);

            // reset fenetre 5 sec
            winStart = now;
            framesWin = 0;
            maxDt = 0.0;
            nextStatsWrite += 5.0;
        }

        // ------------------------
        // 4) eviter CPU 100% (sleep jusqu'au prochain event)
        // ------------------------
        double nextEvent = nextStatsWrite;
        if (nextDisplay < nextEvent) nextEvent = nextDisplay;

        now = get_time();
        if (nextEvent > now) {
            double sleepSec = nextEvent - now;
            if (sleepSec > 0.001) {
                evenementProfilage(&profInfos, ETAT_ENPAUSE);
                usleep((unsigned int)((sleepSec - 0.0005) * 1e6));
            } else {
                evenementProfilage(&profInfos, ETAT_ENPAUSE);
                sched_yield();
            }
        } else {
            evenementProfilage(&profInfos, ETAT_ENPAUSE);
            sched_yield();
        }
        /* FIN Tony TP6 V2 */
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