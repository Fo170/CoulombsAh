# CoulombsAh

Librairies **header-only** pour un coulomètre sur ESP8266 (WeMos D1 Mini), typiquement associées à un capteur de courant INA226.

Version **1.3.1** · Licence **GPL-3.0** · Auteur : Olivier FOURNET (olivier.fournet@free.fr) · Dépôt : [github.com/Fo170/CoulombsAh](https://github.com/Fo170/CoulombsAh)

Deux classes indépendantes :

- `Coulomb` (`src/Coulomb.h`) : comptage de la charge électrique en **Coulombs** (1 C = 1 A·s),
  avec **rendement faradique η** configurable (v1.3.0+).
- `AmpereHeure` (`src/AmpereHeure.h`) : comptage en **Ampères-heures** (1 Ah = 3600 C) avec état de charge (SoC), efficacités charge/décharge, alerte batterie faible et estimation d'autonomie.

## Contenu

| Élément | Description |
|---|---|
| `src/Coulomb.h` | Classe header-only `Coulomb` |
| `src/AmpereHeure.h` | Classe header-only `AmpereHeure` |
| `exemples/Coulomb/` | Projet PlatformIO complet : utilisation de `Coulomb.h` |
| `exemples/AmpereHeure/` | Projet PlatformIO complet : utilisation de `AmpereHeure.h` |
| `library.json` | Manifeste PlatformIO pour utilisation en dépendance |

> Note : `src/` contient uniquement les en-têtes, il n'y a pas de `main.cpp` à la racine. Les programmes d'exemple sont dans `exemples/`.

## Utilisation dans un projet PlatformIO

### Option 1 : dépôt local

Ajouter la racine de ce dépôt dans `lib_extra_dirs` du `platformio.ini` (dans la section `[env:...]`) :

```ini
[env:d1_mini]
platform = espressif8266
board = d1_mini
framework = arduino
lib_extra_dirs = ../CoulombAh
```

### Option 2 : dépendance git

```ini
lib_deps =
    https://github.com/Fo170/CoulombsAh.git
```

Puis inclure dans le code :

```cpp
#include "Coulomb.h"        // et/ou
#include "AmpereHeure.h"
```

### Compiler les exemples

```bash
pio run -d exemples/Coulomb
pio run -d exemples/AmpereHeure
pio device monitor -d exemples/Coulomb   # baud 115200
```

## Exemple rapide : Coulomb

```cpp
#include <Arduino.h>
#include "Coulomb.h"

Coulomb coulomb(0.99);   // filtrage 0.99 (plus de persistance interne)

void loop() {
  float courant = lireCourant();           // en Ampères
  coulomb.addMeasurement(courant, micros());
  Serial.println(coulomb.getTotalCoulombs());  // Coulombs cumulés
}
```

## Rendement faradique η (v1.3.0+)

`Coulomb` applique un **rendement faradique** à l'intégration :

```
ΔC = I · Δt · η          (η = 1.0 par défaut ⇒ intégration brute)
```

`η` est **constant par direction** : un rendement de charge (0,90 pour le plomb) et un
rendement de décharge (1,00). Il ne dépend **pas** du courant.

```cpp
Coulomb coulomb(0.99);
coulomb.setEfficiencies(0.90f, 1.00f);   // η_charge, η_décharge
```

| API | Rôle |
|---|---|
| `setEfficiencies(charge, discharge)` | η de base par direction (0–1) |
| `getChargeEfficiency(I)` / `getDischargeEfficiency(I)` | η appliqué (constant ; `I` ignoré, signature conservée) |
| `getAppliedEfficiency()` | η appliqué à la dernière mesure (diagnostic) |
| `getChargeEfficiencyBase()` / `getDischargeEfficiencyBase()` | valeurs configurées |
| `setPeukertExponent(n)` / `setPeukertReferenceRate(Iref)` | **dépréciés** — sans effet sur l'intégration (cf. ci-dessous) |

### ⚠️ Pourquoi η n'utilise PAS la loi de Peukert

Les versions **1.3.0** appliquaient `η(I) = η_base·(I_ref/|I|)^(n−1)`. C'était faux, et
corrigé en **1.3.1** :

1. **Sens inversé en décharge.** Peukert décrit la capacité *disponible*, qui **décroît**
   avec |I| : les Ah prélevés doivent être **amplifiés**, pas réduits. Appliquée comme
   rendement, la loi **réduisait** la décharge (58 % à 5 A au lieu de 100 %) → le SoC
   descendait 42 % trop lentement (biais optimiste, dangereux).
2. **Écrêtage annulant le correctif.** `η_base·(I_ref/|I|)^(n−1)` écrêté à 1,0 vaut
   **exactement 1,0 pour |I| ≤ ~1 A** — soit précisément le régime de faible courant qu'il
   fallait corriger. Seul le bulk > 1 A était affecté.

La loi de Peukert reste disponible là où elle est physiquement juste :
`BatteryModel::getEffectiveCapacity(I)` / `updateCoulombCount()` (lib **BatteryModels**).

Références : [Faraday efficiency](https://en.wikipedia.org/wiki/Faraday_efficiency) (η =
rapport des charges échangées sur un cycle, pas un facteur de Peukert) ;
[BU-808c](https://batteryuniversity.com/article/bu-808c-coulombic-and-energy-efficiency-with-the-battery)
et [BU-403](https://batteryuniversity.com/article/bu-403-charging-lead-acid) — le rendement
coulombique du plomb vaut ~90 % et **diminue** à très faible courant (auto-décharge), il ne
remonte pas à 100 %.

## Exemple rapide : AmpereHeure

```cpp
#include <Arduino.h>
#include "AmpereHeure.h"

AmpereHeure ah(14.0, 0.99);  // batterie 14 Ah, filtrage 0.99

void setup() {
  ah.begin(0.0f, 0.90f);        // démarre à 90 % de charge
  ah.setEfficiencies(0.96, 0.98);
  ah.setLowBatteryThreshold(0.15);
}

void loop() {
  ah.addMeasurement(lireCourant(), micros());  // positif = charge, négatif = décharge
  Serial.println(ah.getStateOfChargePercent());
}
```

## Persistance (via Fo170/Persistance)

Depuis la **v1.1.0**, la persistance est déléguée à la librairie
[**Fo170/Persistance**](https://github.com/Fo170/Persistance) (EEPROM, FRAM ou LittleFS).
`Coulomb.h` / `AmpereHeure.h` sont désormais sans persistance interne (plus de
constructeur à adresse, plus de `save()` / `load()`).

- **Sauvegarde** : lire `getTotalCoulombs()` (ou `getTotalAh()`) et l'écrire via Persistance.
- **Restauration** : charger la valeur via Persistance puis `setTotalCoulombs(v)` (ou `setTotalAh(v)`).

Exemple avec PersistanceEEPROM :

```cpp
#include "Coulomb.h"
#include <persistance_EEPROM.h>

Coulomb coulomb(0.99);
PersistanceEEPROM persistance(512);

void setup() {
  persistance.begin();
  persistance.addRegister("coulomb", sizeof(float));

  float saved = 0.0f;
  if (persistance.loadFloat("coulomb", &saved)) coulomb.setTotalCoulombs(saved);
}

void loop() {
  coulomb.addMeasurement(lireCourant(), micros());
  persistance.saveFloat("coulomb", coulomb.getTotalCoulombs());  // périodiquement
}
```

Voir les exemples complets `exemples/Coulomb/` et `exemples/AmpereHeure/` (dépendance
`Fo170/Persistance >= 1.0` déclarée dans leur `platformio.ini`).

## Notes

- Timing basé sur `micros()` : débordement (~70 min) géré, intervalles validés (`0 < Δt ≤ 3600 s`).
- **Garde `isfinite()`** (v1.3.1) : un échantillon `NaN`/`Inf` (trame I²C corrompue) est rejeté —
  avant, un seul NaN empoisonnait définitivement le cumul.
- Filtrage exponentiel : `filterAlpha` (défaut `0.99`), plus proche de 1 = plus lissé ; la première mesure initialise l'état.
- `reset()` remet à zéro cumuls **et** moyennes/statistiques (v1.3.1).
- Configuration INA226 utilisée en matériel : adresse `0x40`, shunt `0.01 Ω`, courant max `4.0 A`.
- Commentaires et sortie série rédigés en **français**.

## Évolutions et corrections — détail (quoi, pourquoi, formule)

> Chaque entrée indique **ce qui était faux**, **pourquoi c'était faux** et **la formule exacte**
> avant/après. Les sources utilisées pour trancher sont listées en fin de section (toutes
> vérifiables, cf. [Sources](#sources-et-références)).

### v1.3.1 — 2026-10-18 · η découplé de Peukert (correction de la 1.3.0)

**Ce qui était faux.** La 1.3.0 appliquait la loi de Peukert comme rendement faradique :

```
η(I) = η_base · (I_ref / |I|)^(n−1)          borné à [0,5 ; 1]        ← FAUX
```

**Pourquoi c'était faux — deux défauts indépendants :**

1. **Sens inversé en décharge.** La loi de Peukert décrit la **capacité disponible**, qui
   **décroît** quand le courant augmente : `C(I) = C_nom · (I_ref/I)^(n−1)`. Il en découle que
   les Ah **prélevés** à fort courant doivent être **amplifiés** (à 5 A on consomme plus de
   capacité qu'à 0,5 A pour la même énergie). Utilisée comme rendement, la même loi **réduisait**
   la décharge intégrée : avec `η_base = 1,00`, `I_ref = 1,30 A`, `n = 1,40`, à 5 A on obtenait
   `η = 0,583` → la décharge n'était comptée qu'à **58 %**. Le SoC descendait donc **42 % trop
   lentement** (biais optimiste : la batterie est plus vide qu'affiché — dangereux).
2. **Écrêtage annulant le correctif en charge.** Avec `η_base = 0,90` (plomb), la loi vaut
   `η = 0,90·(1,30/|I|)^0,40`. Elle dépasse 1,0 dès que
   `|I| < I_ref · η_base^(1/(n−1)) = 1,30 · 0,90^2,5 ≈ 0,999 A`, donc l'écrêtage à 1,0 renvoyait
   **exactement 1,000** pour tout `|I| ≲ 1 A` — c'est-à-dire précisément le régime de faible courant
   que le correctif devait traiter. Seul le bulk (> 1 A) était affecté.

**Ce qui a changé.** η est désormais **constant par direction**, sans lien avec Peukert :

```
η(I) = η_charge      si I > 0      (charge)
η(I) = η_décharge    si I < 0      (décharge)
η(I) = 1             si I = 0
```

Défauts : `η_charge = 1,0`, `η_décharge = 1,0` ⇒ intégration brute (rétrocompatible v1.2.1).
Pour le plomb : `η_charge = 0,90`, `η_décharge = 1,00`.

La loi de Peukert reste disponible là où elle est **physiquement juste** :
`BatteryModel::getEffectiveCapacity(I)` et `updateCoulombCount()` (lib **BatteryModels**).

**Autres correctifs de la même version :**

| # | Défaut | Correctif |
|---|---|---|
| F14 | Aucune garde `isfinite()` sur le courant : un seul échantillon `NaN`/`Inf` (trame I²C corrompue) empoisonnait `_coulomb` **définitivement** (aucune réparation automatique) → `SoC_coulomb` → `SoC_fused` | `if (!isfinite(currentAmps)) return _coulombCumul;` en tête de `addMeasurementWithInterval()` |
| F12 | `reset()` ne remettait ni `_appliedEfficiency` ni les moyennes → `getAppliedEfficiency()` renvoyait le η d'**avant** le reset (diagnostic faux) | `reset()` remet `_coulombPerSecond`, `_coulombPerSecondAvg`, `_currentAvg`, `_appliedEfficiency` |
| F16 | `setPeukertExponent()` acceptait tout `n > 0` ; un `n < 1` **désactivait** la loi silencieusement | Borné à `[1.0, 2.0]`, aligné sur `BatteryModel::setPeukertExponent()` |

### v1.3.0 — 2026-10-18 · η appliqué à l'intégration

**Ce qui était faux.** `Coulomb::addMeasurementWithInterval()` intégrait le courant **brut** :

```
ΔC = I · Δt                                   ← avant la 1.3.0 (η = 1 implicite)
```

Le rendement faradique n'existait que dans `AmpereHeure.h` — **classe jamais instanciée** par le
firmware Coulométre (qui utilise `Coulomb.h`). Conséquence mesurée sur l'appareil le 18/10/2026 :
la charge solaire était comptée à 1:1 alors que le rendement réel du plomb vaut ~0,90, d'où une
dérive du SoC de **+7 % par cycle** (84,3 % affiché contre 78,4 % réel).

**Ce qui a changé.** `ΔC = I · Δt · η`, avec `setEfficiencies()` pour configurer η.

### v1.2.1 — 2026-10-17 · macro de version

Ajout de `__COULOMB_H_VERSION__` / `__AMPEREHEURE_H_VERSION__` : source **unique** de la version,
agrégeable par un projet hôte (page `/version`).

### v1.2.0 — 2026-10-17 · Peukert + η dans `AmpereHeure` (additif)

Ajout de `setPeukertExponent()`, `setPeukertReferenceRate()`, `getChargeEfficiency(I)`,
`getDischargeEfficiency(I)` dans `AmpereHeure.h`. `Coulomb.h` était **inchangé** — d'où le bug
corrigé en 1.3.0.

### v1.1.0 — 2026-10-17 · persistance externalisée (breaking)

Persistance déléguée à [Fo170/Persistance](https://github.com/Fo170/Persistance) : suppression de
`#include <EEPROM.h>`, du constructeur à adresse, de `save()`/`load()`/`resetComplete()`. Restauration
via `setTotalCoulombs()` / `setTotalAh()`.

### v1.0.0 — version initiale

---

## Formules exactes (référence)

Convention de signe : **`I > 0` = charge**, `I < 0` = décharge.

### 1. Intervalle de temps (`calculateInterval`)

```
Δt_k = (t_k − t_{k−1}) · 10⁻⁶   [s]                    si t_k ≥ t_{k−1}
Δt_k = (ULONG_MAX − t_{k−1} + t_k + 1) · 10⁻⁶   [s]    si t_k < t_{k−1}  (wrap de micros(), ~71,6 min)
```

Garde : `0 < Δt ≤ 3600 s` sinon l'échantillon est ignoré.

### 2. Filtrage exponentiel (moyennes, non utilisé pour l'intégration)

```
Ī_k      = α · Ī_{k−1}      + (1 − α) · I_k
Δt̄_k     = α · Δt̄_{k−1}     + (1 − α) · Δt_k
```
`α` = `_filterAlpha` (défaut 0,99 ; plus proche de 1 = plus lissé). `α` est appliqué aux
**moyennes** ; l'intégration utilise toujours le courant et l'intervalle **instantanés**.

### 3. Rendement faradique

```
η = η_charge    si I > 0
η = η_décharge  si I < 0
η = 1           si I = 0
```
`η_charge`, `η_décharge` ∈ ]0 ; 1], configurés par `setEfficiencies()`. Valeurs constantes :
η ne dépend **pas** du courant (cf. v1.3.1).

### 4. Intégration (coulombs)

```
ΔC_k     = I_k · Δt_k · η(I_k)          [C]
C_k      = C_base + Σ ΔC_k              [C]
Ah_k     = C_k / 3600                   [Ah]
```

### 5. Loi de Peukert — **dans `BatteryModel`, pas ici**

```
C_eff(I) = C_nom · (I_ref / |I|)^(n−1)      si |I| ≥ 0,01 A et n > 1
C_eff(I) = C_nom                            sinon
```
Ce facteur est **> 1** : la capacité disponible **diminue** à fort courant. Il ne doit jamais être
appliqué comme rendement (cf. v1.3.1).

---

## Sources et références

Consultées le **2026-10-18**. Toutes répondent HTTP 200 à cette date.

| Sujet | Source | Ce qui en est tiré |
|---|---|---|
| Loi de Peukert | [Wikipedia — Peukert's law](https://en.wikipedia.org/wiki/Peukert%27s_law) | `C/C₀ = (I₀/I)^(k−1)` ; la capacité **disponible décroît** avec le courant ⇒ justifie de ne PAS utiliser Peukert comme rendement (v1.3.1, défaut n°1) |
| Rendement faradique | [Wikipedia — Faraday efficiency](https://en.wikipedia.org/wiki/Faraday_efficiency) | η = rapport des charges échangées sur un cycle (grandeur **constante par direction**, pas un facteur dépendant de `I`) |
| Rendement coulombique du plomb | [Battery University — BU-808c](https://batteryuniversity.com/article/bu-808c-coulombic-and-energy-efficiency-with-the-battery) | Plomb : CE ≈ **90 %** ; un courant très faible **abaisse** le rendement (auto-décharge) — il ne remonte pas à 100 % ⇒ justifie le défaut n°2 de la v1.3.1 |
| Charge du plomb | [Battery University — BU-403](https://batteryuniversity.com/article/bu-403-charging-lead-acid) | Rendement élevé « with sufficient charge current », dégradé à faible courant ⇒ même conclusion |
| Batterie plomb-acide | [Wikipedia — Lead–acid battery](https://en.wikipedia.org/wiki/Lead%E2%80%93acid_battery) | Ordres de grandeur (rendement, tensions par cellule) |

---

## Historique synthétique des versions

| Version | Changement majeur |
|---|---|
| **1.3.1** | η **découplé de Peukert** (constant par direction) — corrige une régression de la 1.3.0 ; garde `isfinite` ; `reset()` complet ; `setPeukertExponent` borné `[1.0, 2.0]` |
| 1.3.0 | η **appliqué à l'intégration** par `Coulomb` (auparavant seul `AmpereHeure` en avait un) |
| 1.2.1 | Macro de version `__<LIB>_H_VERSION__` (source unique) |
| 1.2.0 | Peukert dynamique + η variable dans `AmpereHeure` |
| 1.1.0 | Persistance déléguée à `Fo170/Persistance` (breaking) |
| 1.0.0 | Version initiale |

## Licence

GPL-3.0 — voir `library.json`.
