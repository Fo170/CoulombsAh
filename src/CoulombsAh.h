/**
 * @file CoulombsAh.h
 * @brief Accumulateur de charge à GRANDE DYNAMIQUE — Coulombs ET Ampères-heures.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * POURQUOI CE FICHIER REMPLACE `Coulomb.h` ET `AmpereHeure.h`
 * ─────────────────────────────────────────────────────────────────────────────
 * La v2.x exposait deux compteurs séparés :
 *   - `Coulomb`     : cumul de charge en coulombs, en `float32`, remis à zéro
 *                     à chaque synchronisation ;
 *   - `AmpereHeure` : cumul en Ah, en `float32`, avec SoC/capacité/autonomie
 *                     (retirés en v2.0.0 car ce sont des politiques de projet).
 *
 * Les deux souffraient du même défaut de fond : **un `float32` ne peut pas
 * incrémenter une grande valeur par petits pas**. L'ULP croît avec la magnitude :
 *
 *     Ah_total     ULP float32     Incrément 0,1 A × 0,1 s     Conservé ?
 *        1 Ah       1,19e-07 Ah       2,78e-06 Ah             oui
 *       26 Ah       1,91e-06 Ah       2,78e-06 Ah             oui (1,5 ULP)
 *      100 Ah       7,63e-06 Ah       2,78e-06 Ah             NON
 *      910 Ah       6,10e-05 Ah       2,78e-06 Ah             NON (22× sous l'ULP)
 *    10000 Ah       9,77e-04 Ah       2,78e-06 Ah             NON (350× sous l'ULP)
 *
 * Mesuré sur 10 000 000 d'échantillons (exact 27,777778 Ah), un cumul `float32`
 * donne 26,578040 Ah, soit **−1,20 Ah (−4,3 %)**.
 *
 * D'où un compteur **unique et fusionnel** (coulombs et Ah sont la même
 * grandeur à un facteur 3600 près) construit sur deux étages.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * LA SOLUTION : DEUX ÉTAGES (principe de l'odomètre)
 * ─────────────────────────────────────────────────────────────────────────────
 *   Étage BAS  : reste en MICRO-COULOMBS, `int64_t`, borné à [0 ; 1 Ah[.
 *                Étant petit, il garde une précision absolue
 *                (1 µC = 2,78e-10 Ah, soit ~10 000× mieux qu'un échantillon).
 *   Étage HAUT : Ah ENTIERS, `int64_t`. Ne reçoit que des unités entières,
 *                donc aucune perte : ajouter 1 à 910 donne 911 exactement.
 *   REPORT     : dès que le reste atteint 1 Ah, on retire 1 Ah du reste et on
 *                l'ajoute à l'entier. Le reste ne dépasse donc jamais 1 Ah.
 *
 * Lecture :   Ah_total = Ah_entier + reste_µC / 3,6e9
 *
 * Vérification (10 000 000 d'échantillons, exact 27,777778 Ah) :
 *     float32 non borné            → 26,578040 Ah   (−1,20 Ah, −4,3 %)
 *     deux étages, reste float32   → 27,802482 Ah   (+0,025 Ah, +0,089 %)
 *     deux étages, reste int64 µC  → 27,777778 Ah   (erreur +1,9e-06 Ah)
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * DEUX COMPTEURS BRUTS, DEUX RESTES
 * ─────────────────────────────────────────────────────────────────────────────
 * Une première version n'avait qu'UN seul reste signé : les compteurs ne
 * s'incrémentaient donc que sur le SOLDE NET. Un cycle de (+0,5 / −0,4) Ah
 * répété 10 fois donnait 1,0 Ah chargé et 0,0 Ah déchargé au lieu de 5,0 / 4,0 :
 * tout aller-retour sous 1 Ah était invisible.
 *
 * Depuis, le reste est SCINDÉ par sens et l'aiguillage se fait sur le signe de
 * l'INCRÉMENT, pas sur le solde :
 *   - `ucRestChg` accumule les incréments POSITIFS, reporte vers `ahCharged`
 *   - `ucRestDis` accumule les incréments NÉGATIFS (en valeur absolue),
 *     reporte vers `ahDischarged`
 * Les deux compteurs sont donc des cumuls BRUTS, indépendants l'un de l'autre.
 * Le net se calcule par différence des deux.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * PORTÉE : INTÉGRATEUR PUR
 * ─────────────────────────────────────────────────────────────────────────────
 * Cette classe ne contient AUCUNE physique : ni rendement faradique, ni
 * Peukert, ni état de charge, ni capacité batterie, ni autonomie. Elle sait
 * seulement ADDITIONNER une charge et la représenter sans perte.
 *
 * Les politiques (η, gazage, Peukert, SoC) appartiennent au projet qui utilise
 * la librairie. Dépendances : `<Arduino.h>` uniquement, header-only, aucun état
 * global.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * UNITÉS ET CONVENTIONS
 * ─────────────────────────────────────────────────────────────────────────────
 *   1 Ah  = 3600 C = 3,6e9 µC
 *   1 C   = 1e6 µC
 *   Signe : une charge négative est une DÉCHARGE (compteur séparé).
 *
 * Version : 3.0.1
 * Date    : Octobre 2026
 */

#ifndef COULOMBSAH_H
#define COULOMBSAH_H

#include <Arduino.h>

// Version de la librairie (source UNIQUE — agrégée par src/liste_lib.h)
#define __COULOMBSAH_H_VERSION__ "3.0.1"

// ============================================================
// Constantes de conversion
// ============================================================
#ifndef COULOMBSAH_UC_PER_AH
#define COULOMBSAH_UC_PER_AH   3600000000LL   // 1 Ah = 3600 C = 3,6e9 µC
#endif
#ifndef COULOMBSAH_UC_PER_C
#define COULOMBSAH_UC_PER_C    1000000LL      // 1 C = 1e6 µC
#endif
#ifndef COULOMBSAH_MAX_U32
#define COULOMBSAH_MAX_U32     4294967295UL   // saturation de la persistance
#endif

// Alias historiques (v1.x du fichier Ah_GrandeDynamique.h) — conservés pour ne
// pas casser les tests et le code existants.
#define AH_GD_UC_PER_AH        COULOMBSAH_UC_PER_AH
#define AH_GD_UC_PER_C         COULOMBSAH_UC_PER_C
#define AH_GD_MAX_U32          COULOMBSAH_MAX_U32

/**
 * @class Ah_GD
 * @brief Accumulateur de charge à grande dynamique (Ah entiers + reste µC).
 *
 * Intégrateur pur : additionne une charge et la représente sans perte, quelle
 * que soit la magnitude du total. Compteurs chargé / déchargé BRUTS et
 * indépendants. Unités d'entrée acceptées : µC (natif), C, ou Ah.
 */
class Ah_GD {
public:
    // ------------------------------------------------------------
    // État — public par conception (accès direct pour diagnostic/persistance)
    // ------------------------------------------------------------
    int64_t  ahCharged    = 0;   // Ah chargés,   toujours >= 0 (BRUT)
    int64_t  ahDischarged = 0;   // Ah déchargés, toujours >= 0 (BRUT)

    // Restes fractionnaires en µC, chacun dans [0 ; 1 Ah[.
    // SCINDÉS par sens : c'est ce qui rend les compteurs bruts exacts.
    int64_t  ucRestChg    = 0;   // reste des incréments POSITIFS
    int64_t  ucRestDis    = 0;   // reste des incréments NÉGATIFS (valeur absolue)

    uint32_t carryCount   = 0;   // nombre de reports effectués (diagnostic)

    // Résidu d'ARRONDI en µC : la fraction de micro-coulomb qui n'a pas encore
    // pu être convertie en entier. Sans lui, chaque appel à addCoulombs()
    // arrondirait dans le MÊME sens et le biais s'accumulerait (ex. 0,5 µC
    // répété 2000 fois donnerait 2000 µC au lieu de 1000).
    float    ucResidual   = 0.0f;

    Ah_GD() {}
    ~Ah_GD() {}

    // ------------------------------------------------------------
    // Intégration
    // ------------------------------------------------------------
    /**
     * @brief Ajoute une charge en micro-coulombs et effectue les reports.
     * @param dUc Charge à ajouter, en µC, signée (< 0 = décharge).
     *
     * L'aiguillage se fait sur le SIGNE DE L'INCRÉMENT : un incrément positif
     * alimente `ahCharged`, un incrément négatif alimente `ahDischarged`. Les
     * deux compteurs sont donc des cumuls BRUTS indépendants — un aller-retour
     * de faible amplitude reste visible.
     */
    void addMicroCoulombs(int64_t dUc) {
        if (dUc == 0) return;

        // Reports par DIVISION (au lieu d'une boucle) : coût constant quel que
        // soit |dUc|, et aucun risque de boucle longue sur une valeur aberrante.
        if (dUc > 0) {
            ucRestChg += dUc;
            if (ucRestChg < 0) ucRestChg = COULOMBSAH_UC_PER_AH - 1;  // débordement : on clampe
            int64_t n = ucRestChg / COULOMBSAH_UC_PER_AH;
            if (n > 0) {
                ucRestChg -= n * COULOMBSAH_UC_PER_AH;
                ahCharged += n;
                carryCount += (uint32_t)((n > 0xFFFFFFFFLL) ? 0xFFFFFFFFLL : n);
            }
        } else {
            // -INT64_MIN déborderait : on borne avant la négation.
            ucRestDis += (dUc == INT64_MIN) ? INT64_MAX : -dUc;
            if (ucRestDis < 0) ucRestDis = COULOMBSAH_UC_PER_AH - 1;  // débordement : on clampe
            int64_t n = ucRestDis / COULOMBSAH_UC_PER_AH;
            if (n > 0) {
                ucRestDis -= n * COULOMBSAH_UC_PER_AH;
                ahDischarged += n;
                carryCount += (uint32_t)((n > 0xFFFFFFFFLL) ? 0xFFFFFFFFLL : n);
            }
        }
    }

    /**
     * @brief Ajoute une charge en coulombs (float).
     * @param dC Charge en Coulombs, signée (< 0 = décharge).
     *
     * Arrondi au µC le plus proche (±0,5) : une troncature biaiserait toujours
     * dans le même sens et dériverait sur des millions d'échantillons. Le
     * résidu (`ucResidual`) est conservé pour l'appel suivant.
     */
    void addCoulombs(float dC) {
        if (!isfinite(dC)) return;
        // Borne de magnitude : le cast float→int64 est un comportement indéfini
        // au-delà d'INT64_MAX. dC × 1e6 doit rester < 9,22e18 ⇒ dC < 9,2e12.
        // On borne à 1e12 C = 2,8e8 Ah : très au-delà de tout usage réel (une
        // restauration passe par deserialize(), pas par ici), et sans risque.
        const float DC_MAX = 1e12f;                // C
        if (dC >  DC_MAX) dC =  DC_MAX;
        if (dC < -DC_MAX) dC = -DC_MAX;
        float uc = dC * (float)COULOMBSAH_UC_PER_C + ucResidual;
        int64_t dUc = (int64_t)(uc >= 0.0f ? floorf(uc + 0.5f) : ceilf(uc - 0.5f));
        ucResidual = uc - (float)dUc;      // conserve la fraction pour l'appel suivant
        addMicroCoulombs(dUc);
    }

    /**
     * @brief Ajoute des ampères-heures (float).
     * @param dAh Ah à ajouter, signés (< 0 = décharge).
     *
     * ⚠️ Ne PAS passer par addCoulombs() : la borne de celui-ci (1e12 C) est
     * calibrée pour un ÉCHANTILLON. Une restauration légitime de 910 Ah vaut
     * 3,276e6 C et serait écrêtée — bug corrigé en v3.0.1. On borne ici en Ah.
     */
    void addAmpereHours(float dAh) {
        if (!isfinite(dAh)) return;
        // dAh × 3,6e9 doit rester < 9,22e18 ⇒ dAh < 2,56e9. Borne à 1e9 Ah.
        const float AH_MAX = 1e9f;                 // Ah
        if (dAh >  AH_MAX) dAh =  AH_MAX;
        if (dAh < -AH_MAX) dAh = -AH_MAX;
        float uc = dAh * (float)COULOMBSAH_UC_PER_AH + ucResidual;
        int64_t dUc = (int64_t)(uc >= 0.0f ? floorf(uc + 0.5f) : ceilf(uc - 0.5f));
        ucResidual = uc - (float)dUc;
        addMicroCoulombs(dUc);
    }

    // ------------------------------------------------------------
    // Lecture
    // ------------------------------------------------------------
    /** @return Fraction d'Ah des incréments positifs ([0 ; 1[ Ah) */
    float getAhFractionChg() const {
        return (float)ucRestChg / (float)COULOMBSAH_UC_PER_AH;
    }
    /** @return Fraction d'Ah des incréments négatifs ([0 ; 1[ Ah, positive) */
    float getAhFractionDis() const {
        return (float)ucRestDis / (float)COULOMBSAH_UC_PER_AH;
    }
    /**
     * @return Fraction NETTE d'Ah = fraction chargée − fraction déchargée.
     *         Utilisée par getAhNet() ; peut être négative.
     */
    float getAhFraction() const {
        return getAhFractionChg() - getAhFractionDis();
    }

    /** @return Total NET en Ah = (chargé + fChg) − (déchargé + fDis) */
    float getAhNet() const {
        return (float)(ahCharged - ahDischarged) + getAhFraction();
    }

    /** @return Total CHARGÉ brut en Ah (entier + sa fraction) */
    float getAhCharged() const {
        return (float)ahCharged + getAhFractionChg();
    }

    /** @return Total DÉCHARGÉ brut en Ah (entier + sa fraction) */
    float getAhDischarged() const {
        return (float)ahDischarged + getAhFractionDis();
    }

    /**
     * @brief Total NET en micro-coulombs (int64, EXACT).
     *
     * ⚠️ À UTILISER POUR LES DIFFÉRENCES. Calculer `getAhNet()` puis soustraire
     * deux valeurs donne un `float32` : à 910 Ah l'ULP vaut 6,1e-05 Ah, soit
     * 22× l'incrément d'un échantillon de 0,1 s — le delta serait arrondi à 0.
     * En prenant la différence en `int64` µC puis en divisant UNE SEULE FOIS,
     * le résultat est exact.
     *
     * @code
     *   int64_t dUc = ah.getNetMicroCoulombs() - last_uc;
     *   last_uc = ah.getNetMicroCoulombs();
     *   float dAh = (float)dUc / 3.6e9f;      // petit delta → float32 précis
     * @endcode
     *
     * Note : le produit (Ah entiers × 3,6e9) reste dans int64 jusqu'à
     * 2,5e9 Ah — très au-delà de tout usage réel.
     */
    int64_t getNetMicroCoulombs() const {
        return (ahCharged - ahDischarged) * COULOMBSAH_UC_PER_AH
               + ucRestChg - ucRestDis;
    }

    /** @return Total NET en coulombs (float, pour affichage) */
    float getNetCoulombs() const {
        return (float)getNetMicroCoulombs() / (float)COULOMBSAH_UC_PER_C;
    }

    /** @return true si aucun Ah n'a encore été accumulé */
    bool isEmpty() const {
        return ahCharged == 0 && ahDischarged == 0 &&
               ucRestChg == 0 && ucRestDis == 0;
    }

    // ------------------------------------------------------------
    // Remise à zéro
    // ------------------------------------------------------------
    void reset() {
        ahCharged = 0;
        ahDischarged = 0;
        ucRestChg = 0;
        ucRestDis = 0;
        carryCount = 0;
        ucResidual = 0.0f;
    }

    // ------------------------------------------------------------
    // Persistance — 4 × uint32 (16 octets), int64 en RAM
    // ------------------------------------------------------------
    // POURQUOI uint32 : 4,29e9 Ah couvrent très largement la vie d'une
    // installation, et 16 octets tiennent dans une FRAM/EEPROM contrainte.
    // La conversion int64 ⇄ uint32 est faite ICI avec saturation EXPLICITE :
    // un dépassement silencieux produirait un compteur faux sans symptôme.
    //
    // ⚠️ Une version antérieure écrêtait les restes à 0x7FFFFFFF (0,5965 Ah) et
    // masquait les 4 champs avec 0x7FFFFFFF à la relecture. Deux conséquences :
    //   1. PERTE : un reste dans [0,5965 ; 1[ Ah était écrêté à chaque
    //      sauvegarde (~40 % des coupures, jusqu'à 0,403 Ah).
    //   2. BOUCLAGE : les Ah entiers étaient saturés à 4,29e9 en écriture mais
    //      relus sur 31 bits ⇒ à 2 147 483 648 Ah le compteur retombait à 0.
    // Les restes sont TOUJOURS dans [0 ; 3,6e9[ après report, et 3,6e9 tient
    // intégralement dans un uint32 (max 4,29e9) : AUCUN masque n'est nécessaire.
    static constexpr uint8_t SER_LEN = 16;

    void serialize(uint32_t& chg, uint32_t& dis,
                   uint32_t& ucChg, uint32_t& ucDis) const {
        chg = (ahCharged    > (int64_t)COULOMBSAH_MAX_U32) ? COULOMBSAH_MAX_U32 : (uint32_t)ahCharged;
        dis = (ahDischarged > (int64_t)COULOMBSAH_MAX_U32) ? COULOMBSAH_MAX_U32 : (uint32_t)ahDischarged;
        // Pas d'écrêtage : les restes tiennent sur 32 bits par construction.
        ucChg = (uint32_t)ucRestChg;
        ucDis = (uint32_t)ucRestDis;
    }

    void deserialize(uint32_t chg, uint32_t dis,
                     uint32_t ucChg, uint32_t ucDis) {
        // Lecture BRUTE (pas de masque) : la saturation a été faite en écriture.
        ahCharged    = (int64_t)chg;
        ahDischarged = (int64_t)dis;
        ucRestChg    = (int64_t)ucChg;
        ucRestDis    = (int64_t)ucDis;
        // Garde : chaque reste doit rester < 1 Ah. Une valeur corrompue (ou
        // écrite par une version antérieure) ferait boucler le report.
        if (ucRestChg >= COULOMBSAH_UC_PER_AH) ucRestChg = COULOMBSAH_UC_PER_AH - 1;
        if (ucRestDis >= COULOMBSAH_UC_PER_AH) ucRestDis = COULOMBSAH_UC_PER_AH - 1;
        carryCount = 0;
        ucResidual = 0.0f;
    }

    // ------------------------------------------------------------
    // Diagnostic
    // ------------------------------------------------------------
    void printDebug() const {
        Serial.println(F("--- CoulombsAh (deux etages, brut) ---"));
        Serial.printf("Ah charges    : %lld + %.9f = %.9f\n",
                      (long long)ahCharged, getAhFractionChg(), getAhCharged());
        Serial.printf("Ah decharges  : %lld + %.9f = %.9f\n",
                      (long long)ahDischarged, getAhFractionDis(), getAhDischarged());
        Serial.printf("Net           : %.9f Ah\n", getAhNet());
        Serial.printf("Net exact     : %lld uC\n", (long long)getNetMicroCoulombs());
        Serial.printf("Restes        : chg=%lld uC  dis=%lld uC\n",
                      (long long)ucRestChg, (long long)ucRestDis);
        Serial.printf("Reports       : %u\n", (unsigned)carryCount);
    }
};

#endif // COULOMBSAH_H
