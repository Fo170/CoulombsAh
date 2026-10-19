# CoulombsAh

Compteur de charge **fusionnel Coulombs/Ah** pour ESP8266, à **grande dynamique**.

Version **3.1.0** · Licence **GPL-3.0** · Auteur : Olivier FOURNET (olivier.fournet@free.fr) · Dépôt : [github.com/Fo170/CoulombsAh](https://github.com/Fo170/CoulombsAh)

> ## Un seul compteur, exact quelle que soit la magnitude
>
> Coulombs et ampères-heures sont la **même grandeur** à un facteur 3600 près :
> un seul accumulateur les porte, et il doit être exact.
>
> **Le problème résolu.** Un `float32` ne peut pas incrémenter une grande valeur
> par petits pas — l'ULP croît avec la magnitude :
>
> | `Ah_total` | ULP float32 | Incrément 0,1 A × 0,1 s | Conservé ? |
> |---|---|---|---|
> | 26 Ah | 1,91e-06 Ah | 2,78e-06 Ah | oui (1,5 ULP) |
> | **100 Ah** | **7,63e-06 Ah** | 2,78e-06 Ah | **non** |
> | **910 Ah** | **6,10e-05 Ah** | 2,78e-06 Ah | **non (22× sous l'ULP)** |
> | 10 000 Ah | 9,77e-04 Ah | 2,78e-06 Ah | non (350× sous l'ULP) |
>
> Sur 10 000 000 d'échantillons (exact 27,777778 Ah), un cumul `float32` donne
> **26,578040 Ah, soit −1,20 Ah (−4,3 %)**.
>
> **La solution : deux étages** (principe de l'odomètre).
>
> | Étage | Type | Rôle |
> |---|---|---|
> | BAS | `int64_t` µC, borné à [0 ; 1 Ah[ | reste fractionnaire, **précis** (1 µC = 2,78e-10 Ah) |
> | HAUT | `int64_t` Ah | compteur entier, **aucune perte d'unité** |
> | REPORT | automatique à 1 Ah | le reste ne dépasse jamais 1 Ah |
>
> Lecture : `Ah_total = Ah_entier + reste_µC / 3,6e9`
>
> Résultat mesuré : **27,777778 Ah** (erreur 1,9e-06 Ah).
>
> `CoulombsAh` est un **intégrateur PUR** : aucune physique (ni rendement
> faradique, ni Peukert, ni état de charge, ni capacité batterie). Ces
> politiques appartiennent au projet qui utilise la librairie.

## Contenu

| Header | Classe | Rôle |
|---|---|---|
| **`CoulombsAh.h`** | `Ah_GD` | compteur **fusionnel** coulombs/Ah, deux étages `int64`, exact à toute magnitude |

Ce dépôt ne contient **qu'un seul header**, sans dépendance (hors `<Arduino.h>`).

> ### 📦 Dépôts séparés
>
> `Coulomb.h` et `AmpereHeure.h` ont vécu ici jusqu'à la **3.0.1**. Ils sont
> désormais dans leurs propres dépôts, avec leur historique :
>
> | Dépôt | Header | Classe | Version |
> |---|---|---|---|
> | **Fo170/CoulombsAh** | **`CoulombsAh.h`** | `Ah_GD` | **3.1.0** |
> | [Fo170/Coulomb](https://github.com/Fo170/Coulomb) | `Coulomb.h` | `Coulomb` | 2.0.1 |
> | [Fo170/AmpereHeure](https://github.com/Fo170/AmpereHeure) | `AmpereHeure.h` | `AmpereHeure` | 2.0.1 |
>
> Les trois headers étaient déjà **indépendants** (aucun include croisé) :
> les regrouper obligeait à récupérer l'accumulateur `int64` même pour un simple
> comptage de coulombs `float32`.

---

## Utilisation

```cpp
#include "CoulombsAh.h"

Ah_GD ah;

void setup() { }

void loop() {
    float I  = lireCourant();       // A, > 0 = charge
    float dt = 0.1f;                // s — le projet fournit le pas de temps

    ah.addCoulombs(I * dt);         // ou ah.addAmpereHours(I * dt / 3600.0f)
}
```

Le compteur **n'a pas d'horloge interne** : il reçoit une charge déjà intégrée.
C'est le projet qui décide du `dt`, du rendement, et de tout le reste.

---

## API

### Entrées

| Méthode | Unité | Remarque |
|---|---|---|
| `addMicroCoulombs(int64_t dUc)` | µC | **unité native**, aucune conversion, aucune perte |
| `addCoulombs(float dC)` | C | arrondi au µC le plus proche, résidu conservé |
| `addAmpereHours(float dAh)` | Ah | idem, avec **sa propre borne** (voir plus bas) |

Toutes acceptent des valeurs **signées** : `< 0` alimente le compteur de
décharge. NaN/Inf sont rejetés.

### Lecture

| Méthode | Retour | Remarque |
|---|---|---|
| `getAhNet()` | `float` Ah | net = (chargé + fChg) − (déchargé + fDis) |
| `getAhCharged()` / `getAhDischarged()` | `float` Ah | cumuls **bruts**, toujours ≥ 0 |
| `getAhFractionChg()` / `getAhFractionDis()` | `float` Ah | restes, chacun dans [0 ; 1[ |
| **`getNetMicroCoulombs()`** | `int64_t` µC | **EXACT — à utiliser pour les différences** |
| `getNetCoulombs()` | `float` C | affichage |
| `isEmpty()` | `bool` | aucun Ah accumulé |

### Persistance

| Méthode | Remarque |
|---|---|
| `serialize(chg, dis, ucChg, ucDis)` | 4 × `uint32`, saturation **explicite** |
| `deserialize(chg, dis, ucChg, ucDis)` | bornes vérifiées au chargement |
| `reset()` | tout à zéro |

`SER_LEN = 16` octets. Les compteurs entiers sont saturés à 4,29e9 Ah (au-delà,
la vie de l'installation est terminée) ; les restes tiennent intégralement sur
32 bits par construction, donc **aucun masque** n'est appliqué à la relecture.

---

## ⚠️ Pourquoi `getNetMicroCoulombs()` existe

Soustraire deux `getAhNet()` en `float32` **perd le delta**. À 910 Ah l'ULP vaut
6,1e-05 Ah, soit 22× l'incrément d'un échantillon de 0,1 s (2,78e-06 Ah) : la
différence est arrondie à zéro.

```cpp
// ❌ FAUX à grande magnitude
float dAh = ah.getAhNet() - last_Ah;

// ✅ EXACT
int64_t dUc = ah.getNetMicroCoulombs() - last_Uc;
last_Uc = ah.getNetMicroCoulombs();
float dAh = (float)dUc / 3.6e9f;      // petit delta → float32 précis
```

**Mesuré** : sur 3600 pas de 0,1 s à −0,5 A depuis 910 Ah,

| Méthode | Résultat | Erreur |
|---|---|---|
| cumul `float32` | 910,000000 Ah | **+0,049988 Ah perdus** |
| delta `int64` µC | 909,950012 Ah | **−0,0005 pt de SoC** |

---

## Deux compteurs BRUTS, deux restes

Une version antérieure n'avait qu'**un seul** reste signé : les compteurs ne
s'incrémentaient donc que sur le **solde net**. Un cycle de (+0,5 / −0,4) Ah
répété 10 fois donnait 1,0 / 0,0 au lieu de **5,0 / 4,0** — tout aller-retour
sous 1 Ah était invisible.

Depuis, l'aiguillage se fait sur le **signe de l'incrément** :

| Incrément | Reste alimenté | Compteur reporté |
|---|---|---|
| `> 0` | `ucRestChg` | `ahCharged` |
| `< 0` | `ucRestDis` (valeur absolue) | `ahDischarged` |

Les deux compteurs sont donc des cumuls **bruts indépendants** ; le net se
calcule par différence.

---

## ⚠️ Bornes de magnitude

Les bornes protègent contre le **comportement indéfini** du cast `float → int64`
au-delà d'`INT64_MAX` (9,22e18 µC) :

| Entrée | Borne | Justification |
|---|---|---|
| `addCoulombs(dC)` | **1e12 C** | dC × 1e6 < INT64_MAX ; calibrée pour un **échantillon** |
| `addAmpereHours(dAh)` | **1e9 Ah** | dAh × 3,6e9 < INT64_MAX |

⚠️ **`addAmpereHours()` n'appelle PAS `addCoulombs()`** : une restauration
légitime de 910 Ah vaut 3,276e6 C et serait écrêtée par la borne « échantillon ».
C'était un bug de la 3.0.0, corrigé en **3.0.1** (`addAmpereHours(910)` donnait
277,78 Ah au lieu de 910 Ah).

---

## Migration — des anciens compteurs vers `Ah_GD`

| Avant | Après |
|---|---|
| `#include "Coulomb.h"` + `Coulomb` | `#include "CoulombsAh.h"` + `Ah_GD` |
| `#include "AmpereHeure.h"` + `AmpereHeure` | idem |
| `addMeasurement(I)` / `addMeasurementWithInterval(I, dt)` | `addCoulombs(I * dt)` |
| `addCharge(dC)` | `addCoulombs(dC)` |
| `getTotalCoulombs()` / `getCoulombs()` | `getNetCoulombs()` / `getNetMicroCoulombs()` |
| `getAmpereHours()` | `getAhNet()` |
| `getLastInterval()` | supprimé — le projet fournit le `dt` |
| `getSampleCount()`, EMA, min/max | supprimés (inutilisés, et ce n'est pas du comptage) |
| `__COULOMB_H_VERSION__` / `__AMPEREHEURE_H_VERSION__` | `__COULOMBSAH_H_VERSION__` |

Si vous préférez conserver les compteurs `float32`, ils sont dans leurs propres
dépôts : [Fo170/Coulomb](https://github.com/Fo170/Coulomb) et
[Fo170/AmpereHeure](https://github.com/Fo170/AmpereHeure).

---

## Sources et références

Consultées le **2026-10-19**. Toutes répondent HTTP 200 à cette date.

| Sujet | Source | Ce qui en est tiré |
|---|---|---|
| Rendement faradique | [Wikipedia — Faraday efficiency](https://en.wikipedia.org/wiki/Faraday_efficiency) | η = rapport des charges échangées sur un cycle — grandeur physique, **pas** un facteur d'intégration |
| Rendement coulombique du plomb | [Battery University — BU-808c](https://batteryuniversity.com/article/bu-808c-coulombic-and-energy-efficiency-with-the-battery) | Plomb : CE ≈ **90 %** ; un courant très faible **abaisse** le rendement (auto-décharge) |
| Loi de Peukert | [Wikipedia — Peukert's law](https://en.wikipedia.org/wiki/Peukert%27s_law) | `C/C₀ = (I₀/I)^(k−1)` : la capacité **disponible décroît** avec le courant. Ne s'applique jamais comme rendement |
| Seuil de dégagement plomb | [Battery University — BU-403](https://batteryuniversity.com/article/bu-403-charging-lead-acid) | Dégagement au-delà de ~**2,35 V/cell** |
| Loi de dégagement (forme exponentielle) | [PVsyst — Lead-acid voltage model](https://www.pvsyst.com/help-pvsyst7/batteries_lead_acid_model.htm) | `Igass = Iogass · exp(Δ · dUgass)` avec **Δ = 11,7 V⁻¹** |
| Représentation IEEE 754 (ULP) | [Wikipedia — Unit in the last place](https://en.wikipedia.org/wiki/Unit_in_the_last_place) | L'ULP d'un float32 croît avec la magnitude ⇒ justifie l'accumulateur à deux étages |

---

## Historique synthétique des versions

| Version | Changement majeur |
|---|---|
| **3.1.0** | **Séparation des dépôts** — `Coulomb.h` et `AmpereHeure.h` partent dans `Fo170/Coulomb` et `Fo170/AmpereHeure`. Ce dépôt ne contient plus que `CoulombsAh.h`. Dépendance `Persistance` retirée de `library.json` (le header ne l'inclut pas). Exemple `exemples/CoulombsAh/` créé. |
| 3.0.1 | `addAmpereHours()` avait la borne « échantillon » de `addCoulombs()` → une restauration de 910 Ah était écrêtée à 277,78 Ah. Borne propre en Ah. |
| **3.0.0** | **RUPTURE** — `Coulomb.h` et `AmpereHeure.h` **remplacés** par un compteur unique et fusionnel `Ah_GD` (`CoulombsAh.h`) : deux étages (Ah entiers `int64` + reste µC `int64`), compteurs bruts, persistance 4 × `uint32`. Le `dt` n'est plus du ressort du compteur. |
| 2.0.1 | `getLastInterval()` n'était pas mis à jour par `addMeasurementWithInterval()` → pas de temps du modèle 100× trop petit. |
| 2.0.0 | **RUPTURE** — les deux libs redeviennent des **intégrateurs purs**. Toute la physique (η, Peukert, SoC, capacité, autonomie, gazage) est supprimée et migre dans le projet. |
| 1.3.1 | η découplé de Peukert (constant par direction) ; garde `isfinite` ; `reset()` complet ; `setPeukertExponent` borné `[1.0, 2.0]` |
| 1.3.0 | η appliqué à l'intégration par `Coulomb` (auparavant seul `AmpereHeure` en avait un) |
| 1.2.0 | Peukert dynamique + rendement η variable dans `AmpereHeure` |
| 1.1.0 | Persistance déléguée à `Fo170/Persistance` |
| 1.0.0 | Version initiale |
