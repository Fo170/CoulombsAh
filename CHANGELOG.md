# Changelog - CoulombsAh

## [Séparation des dépôts] - 2026-10-19

Le dépôt historique `Fo170/CoulombsAh` contenait **trois** headers
(`Coulomb.h`, `AmpereHeure.h`, `CoulombsAh.h`) alors qu'ils sont parfaitement
indépendants : aucun include croisé, dépendance unique `<Arduino.h>`.

Chacun est désormais dans **son propre dépôt**, avec son historique propre :

| Dépôt | Header | Classe | Version |
|---|---|---|---|
| **Fo170/CoulombsAh** | **`CoulombsAh.h`** | `Ah_GD` | **3.1.0** |
| Fo170/Coulomb | `Coulomb.h` | `Coulomb` | 2.0.1 |
| Fo170/AmpereHeure | `AmpereHeure.h` | `AmpereHeure` | 2.0.1 |

Ce dépôt ne contient plus que **`CoulombsAh.h`**, le compteur fusionnel.
Les entrées de CHANGELOG concernant `Coulomb.h` et `AmpereHeure.h` ont été
transférées dans leurs dépôts respectifs.

L'historique de ce dépôt reste intact et consultable : le tag **`v3.0.1`**
contient encore les trois headers. Le tag **`v3.1.0`** marque la séparation.

## [3.1.0] - 2026-10-19

### 📦 Séparation — un header par dépôt

`Coulomb.h` et `AmpereHeure.h` quittent ce dépôt pour `Fo170/Coulomb` et
`Fo170/AmpereHeure`. `CoulombsAh.h` reste ici, seul.

**Motif** : les trois headers n'ont **aucune dépendance mutuelle** (vérifié :
`<Arduino.h>` + `<climits>` uniquement). Les regrouper n'apportait rien et
brouillait la portée de chacun — un utilisateur voulant seulement un compteur
de coulombs `float32` devait tout de même récupérer l'accumulateur `int64`.

### 🧹 Nettoyage

- `library.json` : `headers` réduit à `CoulombsAh.h`
- `library.json` : dépendance `Fo170/Persistance` **retirée** — le header ne
  l'inclut pas (`<Arduino.h>` seul). Elle n'était déclarée qu'à cause des
  exemples ; elle reste dans leurs `platformio.ini`.
- `exemples/Coulomb/` et `exemples/AmpereHeure/` : déplacés vers leurs dépôts
- `exemples/CoulombsAh/` : **créé** (il n'existait pas)

## [3.0.1] - 2026-10-19

> Version publiée. Le correctif ci-dessous a été appliqué avant la première
> publication : le tag `v3.0.0` n'a jamais existé séparément (il a été retiré).

### 🐛 Fixed — `addAmpereHours()` était écrêtée par la borne de `addCoulombs()`

`addAmpereHours()` appelait `addCoulombs(dAh × 3600)`. Or `addCoulombs()` borne
son entrée à 1e12 C (calibrée pour un **échantillon** : dC × 1e6 doit rester
sous `INT64_MAX`). Une restauration légitime de **910 Ah = 3,276e6 C** passait
donc la borne et se retrouvait écrêtée.

Vérifié : `addAmpereHours(910)` donnait **277,78 Ah** au lieu de 910 Ah.

**Correctif** : `addAmpereHours()` a sa propre borne, exprimée en Ah
(1e9 Ah, soit dAh × 3,6e9 < INT64_MAX) et n'appelle plus `addCoulombs()`.

## [3.0.0] - 2026-10-19 (inclus dans 3.0.1)

### 💥 BREAKING — un seul compteur FUSIONNEL

Les deux compteurs séparés (`Coulomb` en coulombs, `AmpereHeure` en Ah)
souffraient du **même défaut de fond** : un `float32` ne peut pas incrémenter
une grande valeur par petits pas. L'ULP croît avec la magnitude :

| `Ah_total` | ULP float32 | Incrément 0,1 A × 0,1 s | Conservé ? |
|---|---|---|---|
| 26 Ah | 1,91e-06 Ah | 2,78e-06 Ah | oui (1,5 ULP) |
| **100 Ah** | **7,63e-06 Ah** | 2,78e-06 Ah | **non** |
| **910 Ah** | **6,10e-05 Ah** | 2,78e-06 Ah | **non (22× sous l'ULP)** |
| 10 000 Ah | 9,77e-04 Ah | 2,78e-06 Ah | non (350× sous l'ULP) |

Sur 10 000 000 d'échantillons (exact 27,777778 Ah), un cumul `float32` donne
**26,578040 Ah, soit −1,20 Ah (−4,3 %)**.

Coulombs et ampères-heures sont la **même grandeur** à un facteur 3600 près :
un seul accumulateur suffit, et il doit être exact.

### ✅ Le compteur : `Ah_GD` dans `CoulombsAh.h`

**Deux étages** (principe de l'odomètre) :

| Étage | Type | Rôle |
|---|---|---|
| BAS | `int64_t` µC, borné à [0 ; 1 Ah[ | reste fractionnaire, **précis** (1 µC = 2,78e-10 Ah) |
| HAUT | `int64_t` Ah | compteur entier, **aucune perte d'unité** |
| REPORT | automatique à 1 Ah | le reste ne dépasse jamais 1 Ah |

Lecture : `Ah_total = Ah_entier + reste_µC / 3,6e9`

**Deux compteurs BRUTS** (chargé / déchargé) avec **deux restes séparés** :
l'aiguillage se fait sur le signe de l'**incrément**, pas sur le solde. Sans
cela, un cycle de (+0,5 / −0,4) Ah répété 10 fois donnait 1,0 / 0,0 au lieu de
**5,0 / 4,0** — tout aller-retour sous 1 Ah était invisible.

### API

```cpp
// Entrées (l'unité native est le micro-coulomb)
void addMicroCoulombs(int64_t dUc);   // exact
void addCoulombs(float dC);
void addAmpereHours(float dAh);

// Lecture
float   getAhNet();  getAhCharged();  getAhDischarged();
float   getAhFractionChg();  getAhFractionDis();
int64_t getNetMicroCoulombs();        // EXACT — à utiliser pour les DIFFÉRENCES
float   getNetCoulombs();
bool    isEmpty();

// Persistance (4 × uint32, saturation explicite)
static constexpr uint8_t SER_LEN = 16;
void serialize(uint32_t& chg, uint32_t& dis, uint32_t& ucChg, uint32_t& ucDis) const;
void deserialize(uint32_t chg, uint32_t dis, uint32_t ucChg, uint32_t ucDis);

void reset();
```

### ⚠️ Pourquoi `getNetMicroCoulombs()` existe

Soustraire deux `getAhNet()` en `float32` perd le delta : à 910 Ah l'ULP vaut
6,1e-05 Ah, soit 22× l'incrément d'un échantillon de 0,1 s — la différence est
arrondie à zéro. Il faut prendre la différence en `int64` µC **puis** diviser
une seule fois :

```cpp
int64_t dUc = ah.getNetMicroCoulombs() - last_uc;
last_uc = ah.getNetMicroCoulombs();
float dAh = (float)dUc / 3.6e9f;      // petit delta → float32 précis
```

Mesuré : sur 3600 pas de 0,1 s à −0,5 A depuis 910 Ah, le cumul `float32` perd
**0,049988 Ah** ; le delta `int64` donne une erreur de SoC de **−0,0005 pt**.

### 🗑️ Supprimé

- `Coulomb.h` (classe `Coulomb`) — remplacé par `Ah_GD`
- `AmpereHeure.h` (classe `AmpereHeure`) — remplacé par `Ah_GD`
- `addMeasurement()`, `addMeasurementWithInterval()`, `addCharge()`,
  `getLastInterval()`, `getSampleCount()`, filtrage EMA, min/max

L'horodatage et le calcul du `dt` ne sont plus du ressort du compteur : le
projet les fournit (`addMicroCoulombs`/`addCoulombs` reçoivent une charge déjà
intégrée).

### 🏷️ Divers

- Version `2.0.1` → `3.0.0` (`library.json` + macro `__COULOMBSAH_H_VERSION__`)
- Nouvelle macro de version : `__COULOMBSAH_H_VERSION__` (remplace
  `__COULOMB_H_VERSION__` et `__AMPEREHEURE_H_VERSION__`)
- Alias de compatibilité : `AH_GD_UC_PER_AH` → `COULOMBSAH_UC_PER_AH`, etc.
