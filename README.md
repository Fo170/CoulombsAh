# CoulombsAh

Librairies **header-only** pour un coulomètre sur ESP8266 (WeMos D1 Mini), typiquement associées à un capteur de courant INA226.

Version **1.1.0** · Licence **GPL-3.0** · Auteur : Olivier FOURNET (olivier.fournet@free.fr) · Dépôt : [github.com/Fo170/CoulombsAh](https://github.com/Fo170/CoulombsAh)

Deux classes indépendantes :

- `Coulomb` (`src/Coulomb.h`) : comptage de la charge électrique en **Coulombs** (1 C = 1 A·s).
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
- Filtrage exponentiel : `filterAlpha` (défaut `0.99`), plus proche de 1 = plus lissé ; la première mesure initialise l'état.
- Configuration INA226 utilisée en matériel : adresse `0x40`, shunt `0.01 Ω`, courant max `4.0 A`.
- Commentaires et sortie série rédigés en **français**.

## Licence

GPL-3.0 — voir `library.json`.
