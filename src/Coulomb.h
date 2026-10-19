/**
 * @file Coulomb.h
 * @brief Compteur de Coulombs — intégrateur PUR.
 *
 * Mesure et cumule la charge électrique en Coulombs (1 C = 1 A × 1 s) à partir
 * de mesures de courant et d'intervalles de temps.
 *
 * ⚠️ PRINCIPE (v2.0.0) — CETTE LIBRAIRIE NE FAIT QUE COMPTER.
 * Aucune physique n'y est implémentée : ni rendement faradique η, ni Peukert,
 * ni état de charge (SoC), ni dégagement gazeux. Ces politiques appartiennent
 * au PROJET qui utilise la librairie (cf. `src/Coulomb_fct.h` et `src/Ah_fct.h`
 * du dépôt Coulométre). Toute méthode de politique a été SUPPRIMÉE en 2.0.0 :
 *
 *   setEfficiencies, getChargeEfficiency, getDischargeEfficiency,
 *   getChargeEfficiencyBase, getDischargeEfficiencyBase, getAppliedEfficiency,
 *   setPeukertExponent, getPeukertExponent, setPeukertReferenceRate,
 *   getPeukertReferenceRate, variableEfficiency,
 *   setGassingModel, setBatteryVoltage, getGassingThreshold (×2),
 *   getGassingCurrent, isGassingModelEnabled, getGassingIRef
 *
 * Pour appliquer un rendement, le projet pondère le courant AVANT l'appel :
 * @code
 *   coulomb.addMeasurement(I * eta);        // ou
 *   coulomb.addCharge(I * eta * dt_s);      // charge déjà intégrée
 * @endcode
 *
 * Utilisation:
 * @code
 * #include "Coulomb.h"
 *
 * Coulomb coulombCounter;
 *
 * void loop() {
 *   float courant = lireCourant(); // votre fonction de mesure
 *   coulombCounter.addMeasurement(courant, micros());
 *
 *   if (millis() - lastDisplay > 1000) {
 *     Serial.print("Coulombs: "); Serial.println(coulombCounter.getTotalCoulombs());
 *   }
 * }
 * @endcode
 *
 * La persistance (EEPROM, FRAM, LittleFS) est déléguée à la librairie
 * Fo170/Persistance : on restaure via setTotalCoulombs() et on sauvegarde
 * via getTotalCoulombs() (ou getCoulombs()).
 *
 * Version: 2.0.1
 * Date: Octobre 2026
 */

#ifndef COULOMB_H
#define COULOMB_H

// Version de la librairie (source UNIQUE — agrégée par src/liste_lib.h)
#define __COULOMB_H_VERSION__ "2.0.1"

#include <Arduino.h>
#include <climits>

/**
 * @class Coulomb
 * @brief Compteur de Coulombs avec filtrage
 *
 * Calcule et cumule les Coulombs (1 Coulomb = 1 Ampère × 1 seconde)
 * à partir de mesures de courant et d'intervalles de temps.
 *
 * Caractéristiques:
 * - Calcul précis avec timestamps microsecondes
 * - Filtrage exponentiel du courant et des intervalles
 * - Statistiques (moyennes, min, max, nombre d'échantillons)
 * - Gestion des débordements de micros()
 */
class Coulomb {
private:
    // Paramètres de configuration
    float _filterAlpha;           ///< Coefficient de filtrage exponentiel (0-1)

    // États internes
    float _coulomb;                ///< Coulombs depuis la dernière remise à zéro
    float _coulombBase;            ///< Coulombs restaurés via setTotalCoulombs (base)
    float _coulombCumul;           ///< Coulombs totaux (base + intégration)
    float _coulombPerSecond;       ///< Coulombs par seconde (puissance instantanée)
    float _coulombPerSecondAvg;    ///< Moyenne filtrée des Coulombs/s

    // Variables de temps
    unsigned long _lastMicros;      ///< Dernier timestamp en microsecondes
    float _lastIntervalSec;         ///< Dernier intervalle calculé en secondes
    float _avgIntervalSec;          ///< Intervalle moyen filtré
    bool _firstMeasurement;         ///< Indique si c'est la première mesure

    // Variables de courant
    float _current;                 ///< Dernier courant mesuré (A)
    float _currentAvg;              ///< Courant moyen filtré

    // Statistiques
    unsigned long _sampleCount;     ///< Nombre d'échantillons traités
    float _maxCoulomb;              ///< Valeur maximale atteinte
    float _minCoulomb;              ///< Valeur minimale atteinte
    bool _trackMinMax;              ///< Activer le suivi des min/max

    /**
     * @brief Calcule l'intervalle de temps en gérant le débordement de micros()
     * @param currentMicros Timestamp actuel en microsecondes
     * @return Intervalle en secondes
     */
    float calculateInterval(unsigned long currentMicros) {
        unsigned long diff;

        if (currentMicros >= _lastMicros) {
            // Cas normal
            diff = currentMicros - _lastMicros;
        } else {
            // Gestion du débordement (environ 70 minutes)
            diff = (ULONG_MAX - _lastMicros) + currentMicros + 1;
        }

        return diff * 0.000001f; // Conversion microsecondes -> secondes
    }

public:
    /**
     * @brief Constructeur
     * @param filterAlpha Coefficient de filtrage (0-1, défaut: 0.99)
     *        Plus proche de 1 = filtrage plus fort
     */
    Coulomb(float filterAlpha = 0.99) :
        _filterAlpha(filterAlpha),
        _coulomb(0.0f),
        _coulombBase(0.0f),
        _coulombCumul(0.0f),
        _coulombPerSecond(0.0f),
        _coulombPerSecondAvg(0.0f),
        _lastMicros(0),
        _lastIntervalSec(0.001f), // 1ms par défaut
        _avgIntervalSec(0.001f),
        _firstMeasurement(true),
        _current(0.0f),
        _currentAvg(0.0f),
        _sampleCount(0),
        _maxCoulomb(-1e38f),
        _minCoulomb(1e38f),
        _trackMinMax(false) {}

    /**
     * @brief Destructeur
     */
    ~Coulomb() {}

    /**
     * @brief Initialise le compteur
     * @param initialCoulomb Valeur initiale de Coulombs (défaut: 0)
     * @param resetTimestamp Si true, réinitialise le timestamp
     */
    void begin(float initialCoulomb = 0, bool resetTimestamp = true) {
        _coulomb = initialCoulomb;
        _coulombCumul = _coulombBase + initialCoulomb;
        _sampleCount = 0;

        if (resetTimestamp) {
            _lastMicros = micros();
            _firstMeasurement = true;
        }

        // Réinitialiser les stats si demandé
        if (_trackMinMax) {
            _maxCoulomb = -1e38f;
            _minCoulomb = 1e38f;
        }
    }

    /**
     * @brief Ajoute une mesure de courant avec timestamp automatique
     * @param currentAmps Courant mesuré en Ampères
     * @return Coulombs totaux après cette mesure
     *
     * Utilise micros() pour le timestamp automatiquement
     */
    float addMeasurement(float currentAmps) {
        return addMeasurement(currentAmps, micros());
    }

    /**
     * @brief Ajoute une mesure de courant avec timestamp explicite
     * @param currentAmps Courant mesuré en Ampères
     * @param microsTimestamp Timestamp en microsecondes
     * @return Coulombs totaux après cette mesure
     */
    float addMeasurement(float currentAmps, unsigned long microsTimestamp) {
        // Sauvegarder le courant
        _current = currentAmps;

        // Première mesure ? Juste stocker le timestamp
        if (_firstMeasurement || _lastMicros == 0) {
            _lastMicros = microsTimestamp;
            _currentAvg = currentAmps;
            _firstMeasurement = false;
            return _coulombCumul;
        }

        // Calculer l'intervalle
        _lastIntervalSec = calculateInterval(microsTimestamp);

        // Mettre à jour le timestamp pour la prochaine mesure
        _lastMicros = microsTimestamp;

        return addMeasurementWithInterval(currentAmps, _lastIntervalSec);
    }

    /**
     * @brief Ajoute une mesure avec intervalle explicite
     * @param currentAmps Courant mesuré en Ampères
     * @param intervalSec Intervalle de temps en secondes
     * @return Coulombs totaux après cette mesure
     *
     * v2.0.0 : l'intégration est BRUTE (ΔC = I · Δt). Le courant doit déjà
     * être pondéré par le rendement si le projet en applique un.
     */
    float addMeasurementWithInterval(float currentAmps, float intervalSec) {
        // Validation
        if (intervalSec <= 0 || intervalSec > 3600) { // Max 1 heure
            return _coulombCumul;
        }

        // Garde : un seul échantillon NaN/Inf empoisonnait DÉFINITIVEMENT
        // _coulomb (aucune réparation automatique). L'INA226 peut renvoyer NaN
        // si la trame I²C est corrompue.
        if (!isfinite(currentAmps)) return _coulombCumul;

        // Mise à jour des statistiques
        _sampleCount++;

        // Filtrer le courant
        _currentAvg = _filterAlpha * _currentAvg + (1 - _filterAlpha) * currentAmps;

        // Filtrer l'intervalle
        _avgIntervalSec = _filterAlpha * _avgIntervalSec + (1 - _filterAlpha) * intervalSec;

        // ⚠️ v2.0.1 (CORRECTIF) — mémoriser l'intervalle utilisé.
        // `getLastInterval()` n'était renseigné QUE par `addMeasurement()`
        // (horodatage micros). Un appelant qui passe par
        // `addMeasurementWithInterval()` — c'est le cas de `CoulombFct` dans le
        // firmware Coulométre — laissait `_lastIntervalSec` à sa valeur
        // d'initialisation (0,001 s) POUR TOUJOURS.
        // Conséquence en aval : `BatteryKalman::update()` lit
        // `coulombMeter->getLastInterval()` comme pas de temps → il valait
        // 0,001 s au lieu de 0,1 s, soit 100× trop petit. Toutes les constantes
        // de temps du modèle étaient dilatées d'autant :
        //     τ_pol  12 s   → 1200 s
        //     τ2     300 s  → 8,3 h
        //     τ_hyst 2000 s → 55 h
        // La polarisation et l'hystérésis ne s'établissaient donc jamais,
        // V_pol_state restait ≈ 0 et l'OCV était biaisé.
        _lastIntervalSec = intervalSec;

        // Intégration pure : ΔC = I × Δt
        float coulombDelta = currentAmps * intervalSec;

        // Mettre à jour les cumuls
        _coulomb += coulombDelta;
        _coulombCumul = _coulombBase + _coulomb;

        // Calculer les Coulombs par seconde (puissance instantanée)
        if (intervalSec > 1e-6) {
            _coulombPerSecond = coulombDelta / intervalSec;
        } else {
            _coulombPerSecond = currentAmps; // Équivalent car 1A = 1C/s
        }

        // Filtrer la puissance instantanée
        _coulombPerSecondAvg = _filterAlpha * _coulombPerSecondAvg +
                               (1 - _filterAlpha) * _coulombPerSecond;

        // Mettre à jour les min/max si activé
        if (_trackMinMax) {
            if (_coulombCumul > _maxCoulomb) _maxCoulomb = _coulombCumul;
            if (_coulombCumul < _minCoulomb) _minCoulomb = _coulombCumul;
        }

        return _coulombCumul;
    }

    /**
     * @brief Ajoute directement une quantité de charge déjà intégrée
     * @param coulombsDelta Charge à ajouter en Coulombs (signée : < 0 = décharge)
     * @return Coulombs totaux après cet ajout
     *
     * v2.0.0 — point d'entrée pour un projet qui calcule lui-même son
     * incrément (rendement, Peukert, gazage…) sans passer par un courant :
     * @code
     *   float dC = I * dt_s * eta;   // politique du projet
     *   coulomb.addCharge(dC);
     * @endcode
     * Ne touche ni `_current` ni `_lastMicros` (aucune notion de temps ici).
     */
    float addCharge(float coulombsDelta) {
        if (!isfinite(coulombsDelta)) return _coulombCumul;

        _sampleCount++;
        _coulomb += coulombsDelta;
        _coulombCumul = _coulombBase + _coulomb;

        if (_trackMinMax) {
            if (_coulombCumul > _maxCoulomb) _maxCoulomb = _coulombCumul;
            if (_coulombCumul < _minCoulomb) _minCoulomb = _coulombCumul;
        }

        return _coulombCumul;
    }

    /**
     * @brief Réinitialise le compteur à zéro
     */
    void reset() {
        _coulomb = 0.0f;
        _coulombBase = 0.0f;
        _coulombCumul = 0.0f;
        _sampleCount = 0;
        _coulombPerSecond = 0.0f;
        _coulombPerSecondAvg = 0.0f;
        _currentAvg = 0.0f;

        // Réinitialiser les stats
        if (_trackMinMax) {
            _maxCoulomb = -1e38f;
            _minCoulomb = 1e38f;
        }
    }

    /**
     * @brief Rétro-compatibilité : réinitialise le compteur à zéro.
     * @param keepPersisted Ignoré (plus de persistance interne).
     */
    void reset(bool keepPersisted) {
        (void)keepPersisted;
        reset();
    }

    /**
     * @brief Restaure une valeur cumulée (appelé après un load de persistance)
     * @param coulombs Valeur cumulée à restaurer (Coulombs totaux)
     *
     * Définit la base sur laquelle l'intégration s'ajoutera.
     * À utiliser avec la librairie Fo170/Persistance :
     *   persistance.loadFloat("coulomb", &v);
     *   coulomb.setTotalCoulombs(v);
     */
    void setTotalCoulombs(float coulombs) {
        _coulombBase = coulombs;
        _coulombCumul = _coulombBase + _coulomb;
    }

    // ========== Getters ==========

    /**
     * @return Coulombs depuis la dernière réinitialisation
     */
    float getCoulombs() const { return _coulomb; }

    /**
     * @return Coulombs totaux (depuis le dernier reset ou setTotalCoulombs)
     */
    float getTotalCoulombs() const { return _coulombCumul; }

    /**
     * @return Coulombs par seconde actuels
     */
    float getCoulombsPerSecond() const { return _coulombPerSecond; }

    /**
     * @return Moyenne filtrée des Coulombs par seconde
     */
    float getAverageCoulombsPerSecond() const { return _coulombPerSecondAvg; }

    /**
     * @return Courant actuel
     */
    float getCurrent() const { return _current; }

    /**
     * @return Courant moyen filtré
     */
    float getAverageCurrent() const { return _currentAvg; }

    /**
     * @return Dernier intervalle calculé en secondes
     */
    float getLastInterval() const { return _lastIntervalSec; }

    /**
     * @return Intervalle moyen filtré
     */
    float getAverageInterval() const { return _avgIntervalSec; }

    /**
     * @return Nombre d'échantillons traités
     */
    unsigned long getSampleCount() const { return _sampleCount; }

    /**
     * @return Valeur maximale atteinte (si tracking activé)
     */
    float getMaxCoulombs() const { return _maxCoulomb; }

    /**
     * @return Valeur minimale atteinte (si tracking activé)
     */
    float getMinCoulombs() const { return _minCoulomb; }

    /**
     * @return Les Coulombs convertis en Ampères-heures (1Ah = 3600C)
     */
    float getAmpereHours() const { return _coulombCumul / 3600.0f; }

    /**
     * @return Les Coulombs depuis reset convertis en Ampères-heures
     */
    float getAmpereHoursSinceReset() const { return _coulomb / 3600.0f; }

    // ========== Setters et configuration ==========

    /**
     * @brief Configure le coefficient de filtrage
     * @param alpha Nouveau coefficient (0-1)
     */
    void setFilterAlpha(float alpha) {
        if (alpha >= 0 && alpha <= 1) {
            _filterAlpha = alpha;
        }
    }

    /**
     * @brief Active/désactive le suivi des valeurs min/max
     * @param enable true pour activer
     */
    void enableMinMaxTracking(bool enable) {
        _trackMinMax = enable;
        if (enable) {
            _maxCoulomb = -1e38f;
            _minCoulomb = 1e38f;
        }
    }

    /**
     * @brief Force la valeur du courant moyen filtré
     * @param current Valeur du courant moyen (A)
     */
    void setAverageCurrent(float current) {
        _currentAvg = current;
    }

    /**
     * @brief Affiche l'état interne sur le port série (debug)
     */
    void printDebug() {
        Serial.println(F("--- Coulomb ---"));
        Serial.print(F("Coulombs     : ")); Serial.println(_coulomb, 4);
        Serial.print(F("Total        : ")); Serial.println(_coulombCumul, 4);
        Serial.print(F("Ah           : ")); Serial.println(getAmpereHours(), 6);
        Serial.print(F("Courant      : ")); Serial.println(_current, 4);
        Serial.print(F("Courant moy  : ")); Serial.println(_currentAvg, 4);
        Serial.print(F("Intervalle   : ")); Serial.println(_lastIntervalSec, 4);
        Serial.print(F("Échantillons : ")); Serial.println(_sampleCount);
    }
};

#endif // COULOMB_H
