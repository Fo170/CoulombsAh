# Changelog - CoulombsAh

## [1.3.1] - 2026-10-18

### 🐛 Fixed — η découplé de Peukert (régression de la 1.3.0)

La 1.3.0 appliquait `η(I) = η_base·(I_ref/|I|)^(n−1)` comme rendement. Deux défauts :

| # | Défaut | Effet |
|---|---|---|
| **F1** | **Sens inversé en décharge.** Peukert décrit la capacité *disponible* (décroît avec \|I\|) : les Ah prélevés doivent être **amplifiés**, pas réduits. | À 5 A la décharge n'était comptée qu'à **58 %** → le SoC descendait 42 % trop lentement (biais optimiste). |
| **F2** | **Écrêtage à 1.0 annulant le correctif.** `η_base·(I_ref/\|I\|)^(n−1)` écrêté vaut exactement 1.0 pour \|I\| ≤ ~1 A. | Le régime de faible courant (celui de la dérive de +7 %/cycle) n'était pas corrigé ; seul le bulk > 1 A l'était. |

**Correctif** : η est désormais **constant par direction**, sans lien avec Peukert.
`getChargeEfficiency()` renvoie `_chargeEfficiency` (0,90 plomb), `getDischargeEfficiency()`
renvoie `_dischargeEfficiency` (1,00). La correction de Peukert reste disponible là où elle
est physiquement juste : `BatteryModel::getEffectiveCapacity(I)` / `updateCoulombCount`.

Références : « Faraday efficiency » (Wikipedia) ; BU-808c et BU-403 (Battery University) —
le rendement coulombique du plomb vaut ~90 % et **diminue** à très faible courant
(auto-décharge), il ne remonte pas à 100 %.

### 🐛 Fixed — autres

- **F14** : garde `isfinite(currentAmps)` en tête de `addMeasurementWithInterval()`.
  Un seul échantillon NaN (trame I²C corrompue) empoisonnait `_coulomb` **définitivement**
  → `SoC_coulomb` → `SoC_fused`.
- **F12** : `reset()` remet aussi `_appliedEfficiency`, `_currentAvg`, `_avgIntervalSec`
  et `_coulombPerSecond*` (le diagnostic η renvoyait la valeur d'avant le reset).
- **F16** : `setPeukertExponent()` n'accepte plus que `[1.0, 2.0]` (aligné sur
  `BatteryModel`) au lieu de tout `n > 0` — un `n < 1` désactivait la loi silencieusement.

### ⚠️ Compatibilité

API inchangée (sauf validation de `setPeukertExponent`). Défauts `η = 1.0` ⇒ intégration
brute, identique à la 1.2.1. `variableEfficiency()` est conservée (dépréciée, renvoie `base`).

### 🏷️ Divers

- Version `1.3.0` → `1.3.1` (`library.json` + macro)

---

## [1.3.0] - 2026-10-18

### 🐛 Fixed — le rendement η est enfin APPLIQUÉ par `Coulomb`

`Coulomb::addMeasurementWithInterval()` intégrait le courant **brut** :
`ΔC = I · Δt`. Le rendement faradique n'existait que dans `AmpereHeure.h`,
**jamais instanciée** par le firmware Coulométre → la charge solaire était
comptée à 1:1 alors que le rendement réel d'une batterie plomb vaut 0,65-0,90
selon le courant. Conséquence mesurée sur le device (18/10/2026) : le SoC
coulomb dérivait de **+7 % par cycle** (84,3 % affiché vs 78,4 % réel).

Désormais : `ΔC = I · Δt · η(I)`, avec la même loi que
`AmpereHeure::variableEfficiency()` :

```
η(I) = η_base · (I_ref / |I|)^(n−1),  borné [0.5 ; 1]
```

### ✨ Added (API additive, rétrocompatible)

| API | Rôle |
|---|---|
| `setEfficiencies(charge, discharge)` | η de base en charge / décharge (à `I_ref`) |
| `setPeukertExponent(n)` / `getPeukertExponent()` | Exposant de Peukert (`1.0` = η constant) |
| `setPeukertReferenceRate(Iref_A)` / `getPeukertReferenceRate()` | Courant de référence |
| `getChargeEfficiency(I)` / `getDischargeEfficiency(I)` | η effectif à un courant |
| `variableEfficiency(base, I)` | Loi de Peukert (identique à `AmpereHeure`) |
| `getAppliedEfficiency()` | η appliqué à la dernière mesure (diagnostic) |

### ⚠️ Compatibilité

Défauts `η = 1.0` et `n = 1.0` ⇒ **comportement identique à la 1.2.1**.
Une application qui ne configure rien n'est pas affectée.

### 🏷️ Divers

- Version `1.2.1` → `1.3.0` (`library.json` + `library.properties` + macro)

---

## [1.2.1] - 2026-10-17

### ✨ Added — macro de version `__<LIB>_H_VERSION__`

Chaque header déclare désormais sa version via une macro préprocesseur,
source **unique** (un projet hôte peut l'agréger, ex. page `/version`).

### 🏷️ Divers

- Version `1.2.0` → `1.2.1` (`library.json` + `library.properties`)

---

## [1.2.0] - 2026-10-17

### ✨ Added — Peukert dynamique + rendement η variable

Ajouts **additifs** dans `AmpereHeure.h` (`Coulomb.h` inchangé).

| API | Rôle |
|---|---|
| `setPeukertExponent(n)` / `getPeukertExponent()` | Exposant de Peukert runtime (`1.0` = désactivé) |
| `setPeukertReferenceRate(Iref_A)` / `getPeukertReferenceRate()` | Courant de référence (A) — définit où les η de base s'appliquent |
| `getChargeEfficiency(I)` | Rendement de charge **variable** |
| `getDischargeEfficiency(I)` | Rendement de décharge **variable** |

### Comportement
`η(I) = η_base · (I_ref / |I|)^(n−1)`, borné `[0.5, 1]` : à fort courant, une part
de l'énergie est perdue (chaleur / gaz) → le rendement baisse.

`addMeasurement()` applique désormais l'efficacité **variable** (calculée à partir
du courant instantané) au lieu du rendement **statique** `_chargeEfficiency` /
`_dischargeEfficiency`.

### ⚠️ Convention (importante)
η ∈ [0.5, 1] **réduit** la magnitude de ΔAh ⇒ le SoC évolue de façon
**conservatrice** (baisse plus lente en décharge). C'est la convention
« rendement faradique » ; elle est **différente** du ratio de Peukert de
`BatteryModel::updateCoulombCount()` (qui, lui, **accélère** la baisse en
décharge : `peukert_ratio = C_nom / C_eff > 1`). Ne pas confondre les deux modèles.

## [1.1.0] - 2026-09-13

- `setTotalCoulombs()` / `setTotalAh()` utilisés comme **base persistante**
  (l'intégration s'y ajoute au lieu de l'écraser).
- Persistance déléguée à `Fo170/Persistance` (dépendance retirée du
  `library.json` : la lib est fournie par le projet hôte).
