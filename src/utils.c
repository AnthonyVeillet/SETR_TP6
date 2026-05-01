/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 * 
 * Fichier implémentant les fonctions utilitaires telles que déclarées dans utils.h
 ******************************************************************************/
// La ligne ci-dessous indique à la libc que nous voulons les extensions supplémentaires
// permettant, entre autres, l'accès à sched_setattr
#define _GNU_SOURCE
#include "utils.h"

/* DEBUT Tony V1 */
#include <limits.h>
#include <sys/syscall.h>
#include <linux/sched/types.h>

#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE 6
#endif

static int sched_setattr_wrapper(pid_t pid, const struct sched_attr *attr, unsigned int flags)
{
#ifdef SYS_sched_setattr
    return (int)syscall(SYS_sched_setattr, pid, attr, flags);
#else
    (void)pid;
    (void)attr;
    (void)flags;
    errno = ENOSYS;
    return -1;
#endif
}

static int ms_to_ns_checked(unsigned int ms, uint64_t *ns)
{
    if (!ns || ms == 0) {
        errno = EINVAL;
        return -1;
    }

    *ns = (uint64_t)ms * 1000000ULL;
    return 0;
}
/* FIN Tony V1 */

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))


// Applique les paramètres d'ordonnancement au processus courant
/* DEBUT Tony V1 */
int appliquerOrdonnancement(const struct SchedParams* p, const char* nomProg)
{
    const char* prog = nomProg ? nomProg : "prog";

    if (!p) {
        fprintf(stderr, "[%s] ERREUR ordonnancement: paramètres nuls\n", prog);
        return -1;
    }

    if (p->modeOrdonnanceur == ORDONNANCEMENT_NORT) {
        return 0;
    }

    if (p->modeOrdonnanceur == ORDONNANCEMENT_RR ||
        p->modeOrdonnanceur == ORDONNANCEMENT_FIFO) {
        struct sched_param sp;
        memset(&sp, 0, sizeof(sp));
        sp.sched_priority = 99;

        int policy = (p->modeOrdonnanceur == ORDONNANCEMENT_RR) ? SCHED_RR : SCHED_FIFO;
        const char* policyName = (policy == SCHED_RR) ? "RR" : "FIFO";

        if (sched_setscheduler(0, policy, &sp) != 0) {
            fprintf(stderr, "[%s] ERREUR sched_setscheduler(%s, priorité 99): %s\n",
                    prog, policyName, strerror(errno));
            return -1;
        }

        printf("Mode d'operation du scheduler modifie avec succes pour %s (%s, priorité 99).\n",
               prog, policyName);
        return 0;
    }

    if (p->modeOrdonnanceur == ORDONNANCEMENT_DEADLINE) {
        if (p->runtime == 0 || p->deadline == 0 || p->period == 0) {
            fprintf(stderr,
                    "[%s] ERREUR DEADLINE: utiliser -d runtime,deadline,period en ms, valeurs > 0\n",
                    prog);
            return -1;
        }

        if (!(p->runtime <= p->deadline && p->deadline <= p->period)) {
            fprintf(stderr,
                    "[%s] ERREUR DEADLINE: contraintes invalides (%u <= %u <= %u requis)\n",
                    prog, p->runtime, p->deadline, p->period);
            return -1;
        }

        uint64_t runtime_ns = 0;
        uint64_t deadline_ns = 0;
        uint64_t period_ns = 0;

        if (ms_to_ns_checked(p->runtime, &runtime_ns) != 0 ||
            ms_to_ns_checked(p->deadline, &deadline_ns) != 0 ||
            ms_to_ns_checked(p->period, &period_ns) != 0) {
            fprintf(stderr, "[%s] ERREUR DEADLINE: conversion ms -> ns impossible: %s\n",
                    prog, strerror(errno));
            return -1;
        }

        struct sched_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.size = sizeof(attr);
        attr.sched_policy = SCHED_DEADLINE;
        attr.sched_priority = 0;
        attr.sched_runtime = runtime_ns;
        attr.sched_deadline = deadline_ns;
        attr.sched_period = period_ns;

        if (sched_setattr_wrapper(0, &attr, 0) != 0) {
            fprintf(stderr,
                    "[%s] ERREUR sched_setattr(DEADLINE runtime=%u ms, deadline=%u ms, period=%u ms): %s\n",
                    prog, p->runtime, p->deadline, p->period, strerror(errno));
            return -1;
        }

        printf("Mode d'operation du scheduler modifie avec succes pour %s "
               "(DEADLINE runtime=%u ms, deadline=%u ms, period=%u ms).\n",
               prog, p->runtime, p->deadline, p->period);
        return 0;
    }

    fprintf(stderr, "[%s] ERREUR ordonnancement: mode inconnu (%d)\n",
            prog, p->modeOrdonnanceur);
    return -1;
}
/* FIN Tony V1 */

// Parse l'option -s (type d'ordonnanceur: NORT, RR, FIFO, DEADLINE)
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

// Parse l'argument suivant l'option -d (runtime,deadline,period en millisecondes)
/* DEBUT Tony V1 */
int parseDeadlineParams(char* arg, struct SchedParams* params) {
    if (!arg || !params) {
        return -1;
    }

    unsigned int valeurs[3] = {0, 0, 0};
    int count = 0;
    char* saveptr = NULL;
    char* token = strtok_r(arg, ",", &saveptr);

    while (token != NULL) {
        if (count >= 3) {
            fprintf(stderr, "Option -d invalide: trop de paramètres, attendu runtime,deadline,period\n");
            return -1;
        }

        errno = 0;
        char* endptr = NULL;
        unsigned long value = strtoul(token, &endptr, 10);

        if (errno != 0 || endptr == token || *endptr != '\0' || value > UINT_MAX) {
            fprintf(stderr, "Option -d invalide: valeur '%s' non numérique\n", token);
            return -1;
        }

        valeurs[count++] = (unsigned int)value;
        token = strtok_r(NULL, ",", &saveptr);
    }

    if (count != 3) {
        fprintf(stderr, "Option -d invalide: attendu runtime,deadline,period en millisecondes\n");
        return -1;
    }

    params->runtime = valeurs[0];
    params->deadline = valeurs[1];
    params->period = valeurs[2];
    return 0;
}
/* FIN Tony V1 */


/* Convolution with repeat mode */
void _convolve(const unsigned int height, const unsigned int width, const float* input, const Kernel kern, float* output){
    int i, j;
    unsigned int x, y;
    float coeff, data;
    float sum;

    // Short forms of the image dimensions g
    const int iw = width;
    const int kw = kern.width, kh = kern.height;
    const int ow = width;

    // Kernel half-sizes and number of elements g
    const int kw2   = kw/2,        kh2 = kh/2;

    // Iterate over pixels of image
    for(y=0; y<height; y++){
        for(x=0; x<width; x++){
            sum = 0;

            // Iterate over elements of kernel
            for(i=-kh2; i<=kh2; i++){
                for(j=-kw2; j<=kw2; j++){
                    data = input[min(max(y + i, 0), height - 1)*iw + min(max(j + x, 0), width - 1)];
                    coeff = kern.data[(i + kh2)*kw + (j + kw2)];
                    sum += data * coeff;
                }
            }

            output[y*ow + x] = sum;
        }
    }
}


/* Helpers */

void _permuteRGB(const unsigned int in_height, const unsigned int in_width,
                float *input_cont, const unsigned int n_channels, const unsigned char *input)
{
    for (unsigned int k = 0; k < n_channels; ++k) {
        for (unsigned int i = 0; i < in_height; ++i) {
            for (unsigned int j = 0; j < in_width; ++j) {
                input_cont[(in_height*in_width)*k + i*in_width + j] = (float)(input[(i*in_width+j)*n_channels+k]);
            }
        }
    }
}


void _permuteRGB_char(const unsigned int in_height, const unsigned int in_width,
                    unsigned char *input_cont, const unsigned int n_channels, const unsigned char *input)
{
    for (unsigned int k = 0; k < n_channels; ++k) {
        for (unsigned int i = 0; i < in_height; ++i) {
            for (unsigned int j = 0; j < in_width; ++j) {
                input_cont[(in_height*in_width)*k + i*in_width + j] = input[(i*in_width+j)*n_channels+k];
            }
        }
    }
}


void _unpermuteRGB(const unsigned int out_height, const unsigned int out_width,
                float *output_cont, const unsigned int n_channels,
                unsigned char *output)
{
    for (unsigned int k = 0; k < n_channels; ++k) {
        for (unsigned int i = 0; i < out_height; ++i) {
            for (unsigned int j = 0; j < out_width; ++j) {
                output[(i*out_width + j)*n_channels + k] = (unsigned char)(output_cont[(out_height*out_width)*k + i*out_width + j]);
            }
        }
    }
}

Kernel _createGaussianKernel(const unsigned int height, const unsigned int width,
                            const float stdv)
{
    float r, s = 2.0 * stdv * stdv;
    float sum = 0.0;
    Kernel kern;
    kern.width = width;
    kern.height = height;
    kern.data = (float*)tempsreel_malloc(width*height*sizeof(float));
    if(kern.data == NULL){
        fprintf(stderr, "[_createGaussianKernel] Erreur d'allocation memoire avec tempsreel_malloc pour le kernel gaussien (pointeur nul)\n");
        exit(EXIT_FAILURE);
    }

    const int max_x = (height - 1) / 2;
    const int min_x = -(height / 2);
    const int max_y = (width - 1) / 2;
    const int min_y = -(width / 2);

    for (int x = min_x; x <= max_x; x++) {
        for(int y = min_y; y <= max_y; y++) {
            r = sqrt(x*x + y*y);
            kern.data[(x - min_x)*width + (y - min_y)] = (exp(-(r*r)/s))/(M_PI * s);
            sum += kern.data[(x - min_x)*width + (y - min_y)];
        }
    }

    // Normalize the kernel
    for(unsigned int i = 0; i < height*width; ++i) {
        kern.data[i] /= sum;
    }

    return kern;
}

void _destroyKernel(Kernel *kern)
{
    tempsreel_free(kern->data);
}

/* Filtering */

void lowpassFilter(const unsigned int height, const unsigned int width, const unsigned char *input, unsigned char *output,
                const unsigned int kernel_size, float sigma, const unsigned int n_channels)
{
    float *input_cont = (float*)tempsreel_malloc(height * width * n_channels * sizeof(float));
    if(input_cont == NULL){
        fprintf(stderr, "[lowpassFilter] Erreur d'allocation memoire avec tempsreel_malloc pour input_cont (pointeur nul)\n");
        exit(EXIT_FAILURE);
    }
    float *output_cont = (float*)tempsreel_malloc(height * width * n_channels * sizeof(float));
    if(output_cont == NULL){
        fprintf(stderr, "[lowpassFilter] Erreur d'allocation memoire avec tempsreel_malloc pour output_cont (pointeur nul)\n");
        exit(EXIT_FAILURE);
    }

    Kernel kern = _createGaussianKernel(kernel_size, kernel_size, sigma);

    _permuteRGB(height, width, input_cont, n_channels, input);

    for (unsigned int i = 0; i < n_channels; ++i) {
        _convolve(height, width, input_cont+(height*width)*i, kern, output_cont+(height*width)*i);
    }

    _unpermuteRGB(height, width, output_cont, n_channels, output);

    _destroyKernel(&kern);
    tempsreel_free(input_cont);
    tempsreel_free(output_cont);
}

void highpassFilter(const unsigned int height, const unsigned int width, const unsigned char *input, unsigned char *output,
                const unsigned int kernel_size, float sigma, const unsigned int n_channels)
{
    unsigned char *filtered = (unsigned char*)tempsreel_malloc(height * width * n_channels * sizeof(unsigned char));
    if(filtered == NULL){
        fprintf(stderr, "[highpassFilter] Erreur d'allocation memoire avec tempsreel_malloc pour filtered (pointeur nul)\n");
        exit(EXIT_FAILURE);
    }
    
    lowpassFilter(height, width, input, filtered, kernel_size, sigma, n_channels);

    for (unsigned int i = 0; i < height * width * n_channels; ++i ) {
        output[i] = min(abs(input[i] - filtered[i])*2, 255);
    }

    tempsreel_free(filtered);
}



/* Resize */

void _ul_nearestneighbors_regulargrid(const unsigned char *ya, const unsigned int stride, const unsigned int *x1,
            const unsigned int *x2, const unsigned int sz, unsigned char *y)
{
    for (unsigned int i = 0; i < sz; ++i) {
        y[i] = ya[x1[i]*stride + x2[i]];
    }
}

void _ul_bilinear_regulargrid(const unsigned char *ya, const unsigned int stride, const float *x1,
            const float *x2, const unsigned int sz, unsigned char *y)
{
    float l, r, t, b;
    float tmp1, tmp2;

    for (unsigned int i = 0; i < sz; ++i) {
        l = floor(x2[i]);
        r = ceil(x2[i]+1e-4);
        t = floor(x1[i]);
        b = ceil(x1[i]+1e-4);

        tmp1 = (r - x2[i])*(float)ya[(int)t*stride + (int)r] + (x2[i] - l)*(float)ya[(int)t*stride + (int)l];
        tmp2 = (r - x2[i])*(float)ya[(int)b*stride + (int)r] + (x2[i] - l)*(float)ya[(int)b*stride + (int)l];
        y[i] = (unsigned char)((x1[i] - t)*tmp1 + (b - x1[i])*tmp2);
    }
}


void _createGrid(const unsigned int height, const unsigned int width, const float target_x, const float target_y, unsigned int *data_i, unsigned int *data_j)
{
    float step_x = target_x / (float)height;
    float step_y = target_y / (float)width;

    for (unsigned int i = 0; i < height; ++i) {
        for (unsigned int j = 0; j < width; ++j) {
            data_i[i*width + j] = (unsigned int)(step_x*i);
            data_j[i*width + j] = (unsigned int)(step_y*j);
        }
    }
}

void _createGridFloat(const unsigned int height, const unsigned int width, const float target_x, const float target_y, float *data_i, float *data_j)
{
    float step_x = target_x / (float)height;
    float step_y = target_y / (float)width;

    for (unsigned int i = 0; i < height; ++i) {
        for (unsigned int j = 0; j < width; ++j) {
            data_i[i*width + j] = step_x*i;
            data_j[i*width + j] = step_y*j;
        }
    }
}

#define XORSWAP_UNSAFE(a, b)	((a)^=(b),(b)^=(a),(a)^=(b))
#define XORSWAP(a, b)   ((&(a) == &(b)) ? (a) : ((a)^=(b),(b)^=(a),(a)^=(b)))


ResizeGrid resizeNearestNeighborInit(const unsigned int out_height, const unsigned int out_width, const unsigned int in_height, const unsigned int in_width)
{
    ResizeGrid retval;
    memset(&retval, 0, sizeof(retval));
    retval.i = (unsigned int*)tempsreel_malloc(out_height * out_width * sizeof(unsigned int));
    if(retval.i == NULL){
        fprintf(stderr, "[resizeNearestNeighborInit] Erreur d'allocation memoire avec tempsreel_malloc pour retval.i (pointeur nul)\n");
        exit(EXIT_FAILURE);
    }
    retval.j = (unsigned int*)tempsreel_malloc(out_height * out_width * sizeof(unsigned int));
    if(retval.j == NULL){
        fprintf(stderr, "[resizeNearestNeighborInit] Erreur d'allocation memoire avec tempsreel_malloc pour retval.j (pointeur nul)\n");
        exit(EXIT_FAILURE);
    }
    _createGrid(out_height, out_width, (float)in_height, (float)in_width, retval.i, retval.j);

    return retval;
}


ResizeGrid resizeBilinearInit(const unsigned int out_height, const unsigned int out_width, const unsigned int in_height, const unsigned int in_width)
{
    ResizeGrid retval;
    memset(&retval, 0, sizeof(retval));
    retval.i_f = (float*)tempsreel_malloc(out_height * out_width * sizeof(float));
    if(retval.i_f == NULL){
        fprintf(stderr, "[resizeBilinearInit] Erreur d'allocation memoire avec tempsreel_malloc pour retval.i_f (pointeur nul)\n");
        exit(EXIT_FAILURE);
    }
    retval.j_f = (float*)tempsreel_malloc(out_height * out_width * sizeof(float));
    if(retval.j_f == NULL){
        fprintf(stderr, "[resizeBilinearInit] Erreur d'allocation memoire avec tempsreel_malloc pour retval.j_f (pointeur nul)\n");
        exit(EXIT_FAILURE);
    }

    _createGridFloat(out_height, out_width, (float)in_height, (float)in_width, retval.i_f, retval.j_f);

    return retval;
}


void resizeDestroy(ResizeGrid rg)
{
    if (rg.i != NULL) { tempsreel_free(rg.i); }
    if (rg.j != NULL) { tempsreel_free(rg.j); }
    if (rg.i_f != NULL) { tempsreel_free(rg.i_f); }
    if (rg.j_f != NULL) { tempsreel_free(rg.j_f); }
}


void resizeNearestNeighbor(const unsigned char* input, const unsigned int in_height, const unsigned int in_width,
            unsigned char* output, const unsigned int out_height, const unsigned int out_width,
            const ResizeGrid rg, const unsigned int n_channels)
{
    if (n_channels > 1) {
        unsigned char *input_cont = (unsigned char*)tempsreel_malloc(in_height * in_width * n_channels * sizeof(unsigned char));
        if(input_cont == NULL){
            fprintf(stderr, "[resizeNearestNeighborInit] Erreur d'allocation memoire avec tempsreel_malloc pour input_cont (pointeur nul)\n");
            exit(EXIT_FAILURE);
        }
        unsigned char *output_cont = (unsigned char*)tempsreel_malloc(out_height * out_width * n_channels * sizeof(unsigned char));
        if(output_cont == NULL){
            fprintf(stderr, "[resizeNearestNeighborInit] Erreur d'allocation memoire avec tempsreel_malloc pour output_cont (pointeur nul)\n");
            exit(EXIT_FAILURE);
        }
        _permuteRGB_char(in_height, in_width, input_cont, n_channels, input);
        for (unsigned int i = 0; i < n_channels; ++i) {
            _ul_nearestneighbors_regulargrid(input_cont + (in_height*in_width)*i, in_width, rg.i, rg.j, out_height*out_width, output_cont + (out_height*out_width)*i);
        }

        for (unsigned int i = 0; i < out_height; ++i) {
            for (unsigned int j = 0; j < out_width; ++j) {
                for (unsigned int k = 0; k < n_channels; ++k) {
                    output[(i*out_width + j)*n_channels + k] = output_cont[(out_height*out_width)*k + i*out_width + j];
                }
            }
        }

        tempsreel_free(input_cont);
        tempsreel_free(output_cont);
    } else {
        _ul_nearestneighbors_regulargrid(input, in_width, rg.i, rg.j, out_height*out_width, output);
    }

}

void resizeBilinear(const unsigned char* input, const unsigned int in_height, const unsigned int in_width,
                    unsigned char* output, const unsigned int out_height, const unsigned int out_width,
                    const ResizeGrid rg, const unsigned int n_channels)
{
    unsigned char *input_cont = (unsigned char*)tempsreel_malloc(in_height * in_width * n_channels * sizeof(unsigned char));
    if(input_cont == NULL){
        fprintf(stderr, "[resizeBilinear] Erreur d'allocation memoire avec tempsreel_malloc pour input_cont (pointeur nul)\n");
        exit(EXIT_FAILURE);
    }
    unsigned char *output_cont = (unsigned char*)tempsreel_malloc(out_height * out_width * n_channels * sizeof(unsigned char));
    if(output_cont == NULL){
        fprintf(stderr, "[resizeBilinear] Erreur d'allocation memoire avec tempsreel_malloc pour output_cont (pointeur nul)\n");
        exit(EXIT_FAILURE);
    }
    if (n_channels > 1) {
        _permuteRGB_char(in_height, in_width, input_cont, n_channels, input);
        for (unsigned int i = 0; i < n_channels; ++i) {
            _ul_bilinear_regulargrid(input_cont + (in_height*in_width)*i, in_width, rg.i_f, rg.j_f, out_height*out_width, output_cont + (out_height*out_width)*i);
        }

        for (unsigned int i = 0; i < out_height; ++i) {
            for (unsigned int j = 0; j < out_width; ++j) {
                for (unsigned int k = 0; k < n_channels; ++k) {
                    output[(i*out_width + j)*n_channels + k] = output_cont[(out_height*out_width)*k + i*out_width + j];
                }
            }
        }
    } else {
        _ul_bilinear_regulargrid(input, in_width, rg.i_f, rg.j_f, out_height*out_width, output);
    }

    tempsreel_free(input_cont);
    tempsreel_free(output_cont);
}

void convertToGray(const unsigned char* input, const unsigned int in_height, const unsigned int in_width, const unsigned int n_channels,
                    unsigned char* output){
    // =========================================================================
    // OPTIMISATION POUR ARMv6 (Pi Zero W) : Arithmétique entière
    // =========================================================================
    // Le Pi Zero W n'a pas de FPU performante (VFPv2 très lent).
    // On remplace les calculs flottants par de l'arithmétique en point fixe.
    //
    // Formule originale (ITU-R BT.601) :
    //   Y = 0.114*B + 0.587*G + 0.299*R
    //
    // Conversion en point fixe (facteur 256 = 2^8) :
    //   0.114 * 256 ≈ 29
    //   0.587 * 256 ≈ 150  
    //   0.299 * 256 ≈ 77
    //   Total = 256, donc on divise par 256 avec un décalage >> 8
    //
    // Ceci évite toutes les conversions float et opérations FPU.
    // =========================================================================
    
    const unsigned int total_pixels = in_height * in_width;
    const unsigned char *src = input;
    unsigned char *dst = output;
    
    for (unsigned int idx = 0; idx < total_pixels; ++idx) {
        // Calcul en arithmétique entière : (29*B + 150*G + 77*R) >> 8
        // Les coefficients sont pour BGR (ordre utilisé dans le projet)
        *dst++ = (unsigned char)((29 * src[0] + 150 * src[1] + 77 * src[2]) >> 8);
        src += n_channels;
    }
}


void enregistreImage(const unsigned char* input, const unsigned int in_height, const unsigned int in_width, const unsigned int n_channels, const char* nomfichier){
    FILE *f = fopen(nomfichier, "w");
    fprintf(f, "P3\n%d %d\n%d\n", in_width, in_height, 255);
    if(n_channels == 1){
        for (unsigned int i=0; i<in_width*in_height; i++)
            fprintf(f,"%d %d %d ", input[i], input[i], input[i]);
    }
    else{
    for (unsigned int i=0; i<in_width*in_height*3; i+=3)
        fprintf(f,"%d %d %d ", input[i], input[i+1], input[i+2]);
    }
    fclose(f);
}

void initProfilage(InfosProfilage *dataprof, const char *chemin_enregistrement){
    if(PROFILAGE_ACTIF == 0){
        return;
    }
    // Ouverture du fichier
    dataprof->fd = fopen(chemin_enregistrement, "w+");
    dataprof->derniere_sauvegarde = 0;
    dataprof->dernier_etat = ETAT_INDEFINI;

    // Allocation du buffer pour eviter de devoir ecrire systematiquement a chaque appel
    dataprof->data = (char*)calloc(PROFILAGE_TAILLE_INIT, sizeof(char));
    // Force le noyau a allouer reellement la memoire
    memset(dataprof->data, 0, PROFILAGE_TAILLE_INIT * sizeof(char));
    
    dataprof->length = PROFILAGE_TAILLE_INIT;
    dataprof->pos = 0;
}

void evenementProfilage(InfosProfilage *dataprof, unsigned int type){
    if(PROFILAGE_ACTIF == 0){
        return;
    }

    // Obtention du temps courant
    struct timespec temps_courant;
    clock_gettime(CLOCK_MONOTONIC, &temps_courant);
    double multiplier = 1000000000.;
    int c;

    // Si l'etat du programme n'a pas change, on a rien a faire
    if(type == dataprof->dernier_etat){
        return;
    }

    dataprof->dernier_etat = type;

    // Si le buffer n'est pas assez grand (peu probable, mais on ne veut pas causer
    // une erreur de segmentation si ca arrive)
    if(dataprof->pos + 100 > dataprof->length){
        dataprof->data = (char*)realloc(dataprof->data, dataprof->length*2 * sizeof(char));
        dataprof->length *= 2;
    }

    // On cree la ligne indiquant l'evenement et le temps associe
    c = sprintf(dataprof->data + dataprof->pos, "%u,%f\n", type, multiplier * temps_courant.tv_sec + temps_courant.tv_nsec);
    dataprof->pos += c;

    // On ecrit dans le fichier a intervalles reguliers
    if(dataprof->derniere_sauvegarde == 0 || temps_courant.tv_sec - dataprof->derniere_sauvegarde > PROFILAGE_INTERVALLE_SAUVEGARDE_SEC){
        dataprof->derniere_sauvegarde = temps_courant.tv_sec;
        fwrite(dataprof->data, dataprof->pos, 1, dataprof->fd);
        fflush(dataprof->fd);
        dataprof->pos = 0;
    }
}
