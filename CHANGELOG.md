# Changelog - CoulombsAh

## [3.0.1] - 2026-10-19

> **`Coulomb.h` et `AmpereHeure.h` sont RESTAURÉS** dans ce dépôt (ils avaient été
> supprimés par erreur lors de la 3.0.0). Ils restent disponibles et fonctionnels
> — les trois headers coexistent sans conflit — mais `CoulombsAh.h` est le
> compteur recommandé. Voir la section « Coexistence des trois headers ».

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

### 💥 BREAKING — un seul compteur FUSIONNEL, `Coulomb.h` et `AmpereHeure.h` supprimés

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

### 📦 Coexistence des trois headers

`src/` contient désormais **trois** headers indépendants :

| Header | Classe | État | Rôle |
|---|---|---|---|
| **`CoulombsAh.h`** | `Ah_GD` | **recommandé** | compteur fusionnel coulombs/Ah, deux étages `int64`, exact à toute magnitude |
| `Coulomb.h` | `Coulomb` | disponible | compteur de coulombs `float32` (v2.0.1) |
| `AmpereHeure.h` | `AmpereHeure` | disponible | compteur d'Ah `float32` (v2.0.1) |

Ils sont **indépendants** : aucun conflit de symbole, chacun peut être inclus
seul. Vérifié par compilation conjointe :

```
Ah_GD       : 1.000000 Ah
Coulomb     : 3600.0000 C = 1.0000 Ah
AmpereHeure : 1.0000 Ah
```

⚠️ `Coulomb` et `AmpereHeure` restent en `float32` : au-delà de ~50 Ah, leurs
incréments sont arrondis à zéro (voir le tableau de précision en tête de
`CoulombsAh.h`). Les utiliser pour un cumul de longue durée reproduira le défaut
que `Ah_GD` corrige.

## [2.0.1] - 2026-10-19

### 🐛 Fixed — `getLastInterval()` n'était jamais mis à jour par `addMeasurementWithInterval()`

`_lastIntervalSec` n'était assigné QUE dans `addMeasurement(current, micros)`.
Un appelant qui utilise `addMeasurementWithInterval()` — c'est le cas de
`CoulombFct` dans le firmware Coulométre depuis la v2.0.0 — laissait donc
`getLastInterval()` à sa valeur d'initialisation (**0,001 s**) pour toujours.

**Impact en aval.** `BatteryKalman::update()` lit
`coulombMeter->getLastInterval()` comme pas de temps du modèle. Il valait
0,001 s au lieu de 0,1 s, soit **100× trop petit**. Toutes les constantes de
temps étaient dilatées d'autant :

| Constante | Valeur voulue | Valeur effective |
|---|---|---|
| τ_pol | 12 s | **1200 s** |
| τ2 (diffusion) | 300 s | **8,3 h** |
| τ_hyst | 2000 s | **55 h** |

La polarisation et l'hystérésis ne s'établissaient donc jamais : `V_pol_state`
restait ≈ 0 et l'OCV calculé était biaisé.

**Correctif** : `addMeasurementWithInterval()` mémorise l'intervalle qu'il a
réellement utilisé.

### 🏷️ Divers

- Version `2.0.0` → `2.0.1` (`library.json` + macro)

## [2.0.0] - 2026-10-19

### 💥 BREAKING — les deux compteurs redeviennent des INTÉGRATEURS PURS

**Principe.** Une librairie de comptage doit savoir faire `Σ = Σ + I·Δt` et rien
d'autre. Toute **politique physique** — rendement faradique, Peukert, état de
charge, capacité batterie, autonomie, dégagement gazeux — est une décision du
**projet** qui utilise la librairie. Elle a donc été **supprimée** de
`CoulombsAh` et déplacée dans le dépôt du Coulométre :
`src/Coulomb_fct.h` (η + gazage) et `src/Ah_fct.h` (Peukert + accumulateur).

### ❌ API SUPPRIMÉE — `Coulomb.h`

| Méthode retirée | Remplacée par |
|---|---|
| `setEfficiencies(charge, discharge)` | `CoulombFct::setEfficiencyBase()` |
| `getChargeEfficiency(I)` | `CoulombFct::efficiencyFor(I)` |
| `getDischargeEfficiency(I)` | `CoulombFct::etaDis` |
| `getChargeEfficiencyBase()` / `getDischargeEfficiencyBase()` | `CoulombFct::etaChg` / `.etaDis` |
| `getAppliedEfficiency()` | `CoulombFct::etaApplied` |
| `setPeukertExponent()` / `getPeukertExponent()` | `AhFct::setPeukertExponent()` (code mort, supprimé) |
| `setPeukertReferenceRate()` / `getPeukertReferenceRate()` | `AhFct::setPeukertReferenceRate()` |
| `variableEfficiency(base, I)` | supprimée (renvoyait `base` depuis la 1.3.1) |
| `setGassingModel(...)` | `CoulombFct::setGassingModel(...)` |
| `setBatteryVoltage(V, T)` | `CoulombFct::setBatteryVoltage(V, T)` |
| `getGassingThreshold()` ×2, `getGassingCurrent()`, `isGassingModelEnabled()`, `getGassingIRef()` | `CoulombFct::gassingThreshold()`, `.gasI` |

**Conséquence sur l'intégration** : `addMeasurementWithInterval()` n'applique
plus aucun rendement. `ΔC = I · Δt`, point. Le projet pondère le courant avant
l'appel (`addMeasurement(I * eta)`) ou injecte une charge déjà intégrée.

### ✅ API AJOUTÉE — `Coulomb.h`

```cpp
float addCharge(float coulombsDelta);   // charge déjà intégrée, signée
```
Ne touche ni `_current` ni `_lastMicros` : aucune notion de temps. Rejette
NaN/Inf. Sert aux projets qui calculent eux-mêmes leur incrément.

### ❌ API SUPPRIMÉE — `AmpereHeure.h`

| Méthode retirée | Motif |
|---|---|
| `setBatteryCapacity()` / `getBatteryCapacity()` | notion de capacité = politique du projet |
| `setEfficiencies()` / `getChargeEfficiency()` / `getDischargeEfficiency()` / `variableEfficiency()` | rendement = politique du projet |
| `setPeukertExponent()` / `getPeukertExponent()` / `setPeukertReferenceRate()` / `getPeukertReferenceRate()` | Peukert = politique du projet |
| `updateStateOfCharge()` (privé) | SoC = politique du projet |
| `getStateOfCharge()` / `getStateOfChargePercent()` | idem |
| `calibrateStateOfCharge()` | idem |
| `getRemainingCapacity()` | idem |
| `setLowBatteryThreshold()` / `isLowBattery()` | idem |
| `getEstimatedCycles()` | dérivé de la capacité |
| `getEstimatedRuntimeHours()` / `getEstimatedRuntimeFormatted()` | dérivé de la capacité |
| `getPower(V)` | ne relève pas du comptage |

**Signature du constructeur** : `AmpereHeure(float filterAlpha = 0.99)`.
La capacité n'est plus un paramètre (elle n'a plus de sens ici).

### ✅ API AJOUTÉE — `AmpereHeure.h`

```cpp
float addCharge(float ahDelta);   // Ah déjà intégrés, signés
```

### 🧪 Ce qui est CONSERVÉ

**`Coulomb`** : `begin`, `addMeasurement` (×2), `addMeasurementWithInterval`,
`addCharge`, `reset` (×2), `setTotalCoulombs`, `getCoulombs`,
`getTotalCoulombs`, `getCoulombsPerSecond`, `getAverageCoulombsPerSecond`,
`getCurrent`, `getAverageCurrent`, `getLastInterval`, `getAverageInterval`,
`getSampleCount`, `getMaxCoulombs`, `getMinCoulombs`, `getAmpereHours`,
`getAmpereHoursSinceReset`, `setFilterAlpha`, `enableMinMaxTracking`,
`setAverageCurrent`, `printDebug`.

**`AmpereHeure`** : `begin`, `addMeasurement` (×2), `addMeasurementWithInterval`,
`addCharge`, `reset` (×2), `setTotalAh`, `getAh`, `getTotalAh`, `getCurrent`,
`getAverageCurrent`, `getCurrentDirection`, `isCharging`, `isDischarging`,
`getTotalCharged`, `getTotalDischarged`, `getLastInterval`,
`getAverageInterval`, `getSampleCount`, `getMaxAh`, `getMinAh`,
`setFilterAlpha`, `printDebug`.

### ⚠️ Migration

```cpp
// AVANT (1.3.x)
Coulomb c;
c.setEfficiencies(0.90f, 1.00f);
c.setGassingModel(true, 2.35f, 11.7f, 0.45f, 6.0f, -0.005f);
c.setBatteryVoltage(V, T);
c.addMeasurement(I);

// APRÈS (2.0.0) — la physique est dans le projet
CoulombFct f;                       // src/Coulomb_fct.h
f.setEfficiencyBase(0.90f, 1.00f);
f.setGassingModel(true, 2.35f, 11.7f, 0.45f, 6.0f, -0.005f);
f.addSample(I, V, T, dt_s);         // → applique η puis intègre

// Ou, sans le projet, en pondérant soi-même :
Coulomb c;
c.addMeasurement(I * eta);
```

### 🏷️ Divers

- `library.json` : `1.3.1` → `2.0.0`
- `__COULOMB_H_VERSION__` : `1.3.1` → `2.0.0`
- `__AMPEREHEURE_H_VERSION__` : `1.2.1` → `2.0.0`
- La v1.3.2 (rendement de charge variable) n'a **jamais été publiée** : son
  contenu a directement migré dans `CoulombFct` côté projet.

## [1.3.1] - 2026-10-18
