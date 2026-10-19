# Changelog - CoulombsAh

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
