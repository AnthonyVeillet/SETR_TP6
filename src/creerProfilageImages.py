# Lit les fichiers de profilage et cree un graphique correspondant
# Marc-Andre Gardner, Hiver 2025

import argparse
from pathlib import Path

import numpy as np
import matplotlib

matplotlib.use("Agg")
from matplotlib import pyplot as plt
from matplotlib.patches import Rectangle, Patch

# Usage
# python creerProfilageImages.py dossier_vers_les_fichiers_prof-* --sortie=fichier_de_sortie_optionnel --duree=duree_en_secondes

COULEUR_MAPPING = {
    0: "#404040",
    10: "#f5d742",
    20: "#f52121",
    30: "#00B500",
    40: "#f542ef",
    50: "#00CCFF",
}
COULEUR_MAPPING = {     # Voir https://jfly.uni-koeln.de/color/
    0: "#000000",
    10: "#404040",
    20: "#F0E442",
    30: "#009E73",
    40: "#D55E00",
    50: "#56B4E9",
}
NS_TO_MS = 1e6


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="Afficheur des etats des programmes, Labo 3 SETR"
    )
    parser.add_argument(
        "dossier_input",
        help="Chemin vers le dossier contenant les fichiers profilage-*",
        type=Path,
    )
    parser.add_argument(
        "--sortie",
        type=str,
        default="graph.png",
        help="Fichier de sortie (PNG), optionnel",
    )
    parser.add_argument(
        "--duree",
        type=int,
        default=-1,
        help="Afficher uniquement les N premieres secondes (par defaut, tout afficher)",
    )
    args = parser.parse_args()
    files = sorted(
        args.dossier_input.glob("profilage-*"),
        key=lambda f: int(f.stem.rpartition("-")[2]),
        reverse=True,
    )
    data = {}

    print(
        "Liste des derniers evenements par processus (voir le header utils.h pour la definition de chaque valeur numerique)"
    )
    for fpath in files:
        # Validation
        with open(fpath) as f:
            d = f.readlines()
            if len(d) < 3:
                print(f"Attention, le fichier {fpath.stem} possède moins de 2 lignes et sera ignoré!")
                continue
            if any(len(ligne) < 10 for ligne in d[:-1]):
                print(f"Attention, le fichier {fpath.stem} possède des lignes invalides (moins de 10 caractères) et sera ignoré!")
                continue
            if any(ligne.count(",") != 1 for ligne in d[:-1]):
                print(f"Attention, le fichier {fpath.stem} possède des lignes invalides (sans caractère de séparation) et sera ignoré!")
                continue
        data[fpath.stem] = np.loadtxt(fpath, delimiter=",")
        print(
            f"Dernier evenement pour {fpath.stem.partition('-')[2]} : {int(data[fpath.stem][-1,0])} au temps t={data[fpath.stem][-1,1]/1e9:.6f}"
        )
        data[fpath.stem] = np.concatenate(
            (data[fpath.stem][:-1, :], data[fpath.stem][1:, 1:]), axis=1
        )

    ref_temps = min(d[0, 1] for d in data.values())
    temps_max = (
        max(d[-1, 2] for d in data.values()) - ref_temps
        if args.duree == -1
        else args.duree * 1e9
    )

    fig, ax = plt.subplots(figsize=((temps_max / 1e9 / 2) + 10, 4), dpi=300)
    for i, (nom, d) in enumerate(data.items()):
        for etat, temps_debut, temps_fin in d:
            temps_norm_debut = (temps_debut - ref_temps) / NS_TO_MS
            temps_norm_fin = (temps_fin - ref_temps) / NS_TO_MS
            r = Rectangle(
                (temps_norm_debut, i + 0.1),
                temps_norm_fin,
                0.8,
                color=COULEUR_MAPPING[etat],
            )
            ax.add_patch(r)

    ax.set_yticks(
        [(i + 0.5) for i in range(len(data))],
        labels=[k.partition("-")[2] for k in data.keys()],
    )
    graphpos = ax.get_position()
    ax.set_position(
        [
            graphpos.x0,
            graphpos.y0 + graphpos.height * 0.11,
            graphpos.width,
            graphpos.height * 0.89,
        ]
    )
    ax.legend(
        loc="upper left",
        bbox_to_anchor=(0.03, -0.15),
        ncol=6,
        handles=[
            Patch(color=COULEUR_MAPPING[0], label="Indefini"),
            Patch(color=COULEUR_MAPPING[10], label="Initialisation"),
            Patch(color=COULEUR_MAPPING[20], label="Attente lecture"),
            Patch(color=COULEUR_MAPPING[30], label="Traitement"),
            Patch(color=COULEUR_MAPPING[40], label="Attente ecriture"),
            Patch(color=COULEUR_MAPPING[50], label="En pause"),
        ],
    )

    plt.xlim(0, temps_max / NS_TO_MS)
    plt.ylim(0, len(data))
    plt.xlabel("Temps (ms)")
    plt.ylabel("Processus")
    plt.savefig(args.sortie, bbox_inches="tight", pad_inches=0.1)
