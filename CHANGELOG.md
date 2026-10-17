# Changelog - CoulombsAh

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
