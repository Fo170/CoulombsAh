# CoulombsAh

Librairies **header-only** pour un coulomètre sur ESP8266 (WeMos D1 Mini), typiquement associées à un capteur de courant INA226.

Version **2.0.0** · Licence **GPL-3.0** · Auteur : Olivier FOURNET (olivier.fournet@free.fr) · Dépôt : [github.com/Fo170/CoulombsAh](https://github.com/Fo170/CoulombsAh)

> ## ⚠️ Principe depuis la 2.0.0 : ces librairies ne font que COMPTER
>
> `Coulomb` et `AmpereHeure` sont des **intégrateurs purs**. Ils ne contiennent
> **aucune physique** : ni rendement faradique η, ni Peukert, ni état de charge,
> ni capacité batterie, ni autonomie, ni dégagement gazeux.
>
> Toute politique est une décision du **projet** qui utilise la librairie. Dans
> le dépôt du Coulométre, elle vit dans `src/Coulomb_fct.h` (η + gazage) et
> `src/Ah_fct.h` (Peukert + accumulateur à deux étages).
>
> ```cpp
> // La librairie fait ceci, et rien d'autre :
> ΔC  = I · Δt            // Coulomb::addMeasurementWithInterval()
> ΔAh = I · Δt / 3600     // AmpereHeure::addMeasurementWithInterval()
>
> // Le projet pondère s'il le souhaite :
> coulomb.addMeasurement(I * eta);
> ```
>
> Voir la section **[Migration 1.x → 2.0.0](#migration-1x--200)** : l'API a
> changé de façon incompatible.

## Contenu

| Classe | Fichier | Rôle |
|---|---|---|
| `Coulomb` | `src/Coulomb.h` | Intègre et cumule la charge électrique en **Coulombs** (1 C = 1 A·s) |
| `AmpereHeure` | `src/AmpereHeure.h` | Intègre et cumule la capacité en **Ampères-heures** (1 Ah = 3600 C) |

---

## `Coulomb`

### Intégration

```
ΔC = I · Δt
```

Aucun rendement n'est appliqué. Le courant est intégré tel que mesuré, dans les
deux sens (charge positive, décharge négative).

```cpp
#include "Coulomb.h"

Coulomb coulomb;

void setup() { coulomb.begin(); }

void loop() {
    float I = lireCourant();                 // A, > 0 = charge
    coulomb.addMeasurement(I);               // timestamp = micros()
    // ou, si l'intervalle est connu :
    coulomb.addMeasurementWithInterval(I, 0.1f);
}
```

### Injection d'une charge déjà intégrée

```cpp
float addCharge(float coulombsDelta);        // signé : < 0 = décharge
```

Utile quand le projet calcule lui-même son incrément (rendement, Peukert,
gazage…) sans passer par un courant. Ne touche ni `getCurrent()` ni l'horloge
interne — aucune notion de temps. Les NaN/Inf sont rejetés.

### API

| Méthode | Rôle |
|---|---|
| `begin(initialCoulomb = 0, resetTimestamp = true)` | Initialise |
| `addMeasurement(I)` | Intègre avec `micros()` |
| `addMeasurement(I, microsTimestamp)` | Intègre avec horodatage explicite |
| `addMeasurementWithInterval(I, dt_s)` | Intègre sur un intervalle donné (0 < Δt ≤ 3600 s) |
| `addCharge(dC)` | Ajoute une charge déjà intégrée |
| `reset()` / `reset(bool)` | Remet à zéro |
| `setTotalCoulombs(C)` | Fixe la **base** (persistance) ; l'intégration s'y ajoute |
| `getCoulombs()` | Coulombs depuis le dernier reset |
| `getTotalCoulombs()` | Base + intégration |
| `getAmpereHours()` | `getTotalCoulombs() / 3600` |
| `getAmpereHoursSinceReset()` | `getCoulombs() / 3600` |
| `getCoulombsPerSecond()` / `getAverageCoulombsPerSecond()` | Débit instantané / lissé |
| `getCurrent()` / `getAverageCurrent()` | Courant brut / lissé |
| `getLastInterval()` / `getAverageInterval()` | Intervalles |
| `getSampleCount()` | Nombre d'échantillons |
| `getMaxCoulombs()` / `getMinCoulombs()` | Bornes (si `enableMinMaxTracking(true)`) |
| `setFilterAlpha(α)` | Coefficient EMA du courant (0–1) |
| `enableMinMaxTracking(bool)` | Active le suivi min/max |
| `setAverageCurrent(I)` | Force le courant moyen lissé |
| `printDebug()` | État interne sur le port série |

### Garde-fous

- **Δt hors `]0 ; 3600]`** rejeté (pas d'intégration).
- **`I` = NaN ou ±Inf** rejeté : un seul échantillon corrompu (trame I²C)
  empoisonnerait définitivement le cumul.
- **Débordement de `micros()`** (~70 min) géré dans `calculateInterval()`.

---

## `AmpereHeure`

### Intégration

```
ΔAh = I · Δt / 3600
```

Cumuls séparés **chargé** (`getTotalCharged()`) et **déchargé**
(`getTotalDischarged()`), tous deux positifs.

```cpp
#include "AmpereHeure.h"

AmpereHeure ah;

void setup() { ah.begin(); }

void loop() {
    ah.addMeasurement(lireCourant());
}
```

### API

| Méthode | Rôle |
|---|---|
| `begin(initialAh = 0)` | Initialise |
| `addMeasurement(I)` / `addMeasurement(I, microsTimestamp)` | Intègre |
| `addMeasurementWithInterval(I, dt_s)` | Intègre sur un intervalle |
| `addCharge(ahDelta)` | Ajoute des Ah déjà intégrés (signés) |
| `reset()` / `reset(bool)` | Remet à zéro |
| `setTotalAh(ah)` | Fixe la **base** (persistance) |
| `getAh()` / `getTotalAh()` | Session / base + session |
| `getTotalCharged()` / `getTotalDischarged()` | Cumuls par sens (positifs) |
| `getCurrent()` / `getAverageCurrent()` | Courant |
| `getCurrentDirection()` | `1` charge, `−1` décharge, `0` repos |
| `isCharging()` / `isDischarging()` | Seuil ±100 mA |
| `getLastInterval()` / `getAverageInterval()` / `getSampleCount()` | Diagnostic |
| `getMaxAh()` / `getMinAh()` | Bornes |
| `setFilterAlpha(α)` | Coefficient EMA |
| `printDebug()` | État interne sur le port série |

---

## ⚠️ Précision : pourquoi `float` ne suffit pas pour un grand total

Un `float` 32 bits ne peut pas incrémenter par petits pas une grande valeur.
L'ULP (plus petit incrément représentable) croît avec la magnitude :

| `Ah_total` | ULP float32 | Incrément 0,1 A × 0,1 s | Incrément conservé ? |
|---|---|---|---|
| 1 Ah | 1,19e-07 Ah | 2,78e-06 Ah | oui |
| 26 Ah | 1,91e-06 Ah | 2,78e-06 Ah | oui (1,5 ULP, limite) |
| **100 Ah** | **7,63e-06 Ah** | 2,78e-06 Ah | **non** |
| **910 Ah** | **6,10e-05 Ah** | 2,78e-06 Ah | **non** (22× sous l'ULP) |
| 10 000 Ah | 9,77e-04 Ah | 2,78e-06 Ah | non (350× sous l'ULP) |

Simulation de 10 000 000 échantillons (0,1 A × 0,1 s), exact = 27,777778 Ah :

| Schéma | Résultat | Erreur |
|---|---|---|
| float32 non borné | 26,578040 Ah | **−1,20 Ah (−4,3 %)** |
| entier Ah + reste borné à 1 Ah | 27,777778 Ah | **0** |

**Solution recommandée** (implémentée dans `Ah_fct.h` du projet Coulométre) :
compteur à **deux étages**, principe de l'odomètre.

```
Étage BAS  : reste en micro-coulombs, int64, borné à ±3,6e9 (1 Ah)
             → petit, donc précis (1 µC = 2,8e-10 Ah)
Étage HAUT : Ah ENTIERS, int64
             → aucune perte : ajouter 1 à 910 donne 911 exactement
REPORT     : dès que |reste| ≥ 3,6e9 µC, on transfère 1 Ah vers l'étage haut
Lecture    : Ah_total = Ah_entier + reste / 3,6e9
```

Cette logique n'est **pas** dans la librairie : c'est une politique de
représentation, donc du ressort du projet.

---

## Migration 1.x → 2.0.0

### `Coulomb`

| Avant (1.3.x) | Après (2.0.0) |
|---|---|
| `setEfficiencies(c, d)` | supprimé → pondérer `I` avant l'appel, ou utiliser `CoulombFct` |
| `getChargeEfficiency(I)` / `getDischargeEfficiency(I)` | supprimés |
| `getChargeEfficiencyBase()` / `getDischargeEfficiencyBase()` | supprimés |
| `getAppliedEfficiency()` | supprimé |
| `setPeukertExponent()` / `setPeukertReferenceRate()` + getters | supprimés |
| `variableEfficiency(base, I)` | supprimé |
| `setGassingModel(...)`, `setBatteryVoltage(...)`, `getGassingThreshold()`, `getGassingCurrent()`, `isGassingModelEnabled()`, `getGassingIRef()` | supprimés |
| — | **`addCharge(dC)` ajouté** |

```cpp
// AVANT
Coulomb c;
c.setEfficiencies(0.90f, 1.00f);
c.setGassingModel(true, 2.35f, 11.7f, 0.45f, 6.0f, -0.005f);
c.setBatteryVoltage(V, T);
c.addMeasurement(I);

// APRÈS — soit on pondère soi-même :
Coulomb c;
float eta = monRendement(V, I, T);     // politique du projet
c.addMeasurement(I * eta);

// …soit on utilise la façade du projet (CoulombFct) :
f.addSample(I, V, T, dt_s);
```

### `AmpereHeure`

Le constructeur ne prend plus la capacité : `AmpereHeure(float filterAlpha = 0.99)`.

| Avant (1.2.x) | Après (2.0.0) |
|---|---|
| `AmpereHeure(float cap, float alpha)` | `AmpereHeure(float alpha = 0.99)` |
| `setBatteryCapacity()` / `getBatteryCapacity()` | supprimés |
| `setEfficiencies()` / `getChargeEfficiency()` / `getDischargeEfficiency()` | supprimés |
| `setPeukertExponent()` / `setPeukertReferenceRate()` + getters | supprimés |
| `getStateOfCharge()` / `getStateOfChargePercent()` / `calibrateStateOfCharge()` | supprimés |
| `getRemainingCapacity()` / `setLowBatteryThreshold()` / `isLowBattery()` | supprimés |
| `getEstimatedCycles()` / `getEstimatedRuntimeHours()` / `getEstimatedRuntimeFormatted()` | supprimés |
| `getPower(V)` | supprimé |
| `begin(initialAh, initialSoC)` | `begin(initialAh = 0)` |
| — | **`addCharge(ahDelta)` ajouté** |

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
| **2.0.0** | **RUPTURE** — les deux libs redeviennent des **intégrateurs purs**. Toute la physique (η, Peukert, SoC, capacité, autonomie, gazage) est supprimée et migre dans le projet. Ajout de `addCharge()`. |
| 1.3.1 | η découplé de Peukert (constant par direction) ; garde `isfinite` ; `reset()` complet ; `setPeukertExponent` borné `[1.0, 2.0]` |
| 1.3.0 | η appliqué à l'intégration par `Coulomb` (auparavant seul `AmpereHeure` en avait un) |
| 1.2.0 | Peukert dynamique + rendement η variable dans `AmpereHeure` |
| 1.1.0 | Persistance déléguée à `Fo170/Persistance` |
| 1.0.0 | Version initiale |
