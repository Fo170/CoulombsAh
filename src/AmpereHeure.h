/**
 * @file AmpereHeure.h
 * @brief Compteur d'Ampères-heures — intégrateur PUR.
 *
 * Mesure et cumule la capacité en Ampères-heures (1 Ah = 3600 C) à partir de
 * mesures de courant et d'intervalles de temps.
 *
 * ⚠️ PRINCIPE (v2.0.0) — CETTE LIBRAIRIE NE FAIT QUE COMPTER.
 * Aucune physique n'y est implémentée : ni rendement faradique η, ni Peukert,
 * ni état de charge (SoC), ni capacité batterie, ni autonomie, ni cycles.
 * Ces politiques appartiennent au PROJET qui utilise la librairie (cf.
 * `src/Ah_fct.h` du dépôt Coulométre). Toute méthode de politique a été
 * SUPPRIMÉE en 2.0.0 :
 *
 *   setBatteryCapacity, getBatteryCapacity, setEfficiencies,
 *   getChargeEfficiency, getDischargeEfficiency, variableEfficiency,
 *   setPeukertExponent, getPeukertExponent, setPeukertReferenceRate,
 *   getPeukertReferenceRate, setLowBatteryThreshold, calibrateStateOfCharge,
 *   updateStateOfCharge, getStateOfCharge, getStateOfChargePercent,
 *   isLowBattery, getRemainingCapacity, getEstimatedCycles, getPower,
 *   getEstimatedRuntimeHours, getEstimatedRuntimeFormatted
 *
 * Utilisation:
 * @code
 * #include "AmpereHeure.h"
 *
 * AmpereHeure ahCounter;
 *
 * void loop() {
 *   float courant = lireCourant(); // votre fonction de mesure
 *   ahCounter.addMeasurement(courant, micros());
 * }
 * @endcode
 *
 * La persistance (EEPROM, FRAM, LittleFS) est déléguée à la librairie
 * Fo170/Persistance : on restaure via setTotalAh() et on sauvegarde
 * via getTotalAh() (ou getAh()).
 *
 * Version: 2.0.0
 * Date: Octobre 2026
 */

#ifndef AMPEREHEURE_H
#define AMPEREHEURE_H

// Version de la librairie (source UNIQUE — agrégée par src/liste_lib.h)
#define __AMPEREHEURE_H_VERSION__ "2.0.0"

#include <Arduino.h>
#include <climits>

/**
 * @class AmpereHeure
 * @brief Compteur d'Ampères-heures (float)
 *
 * Calcule et cumule les Ampères-heures (1 Ah = 3600 Coulombs) avec :
 * - filtrage exponentiel du courant et des intervalles
 * - cumuls séparés chargé / déchargé
 * - statistiques (min, max, nombre d'échantillons)
 * - gestion du débordement de micros()
 */
class AmpereHeure {
private:
    // Paramètres de configuration
    float _filterAlpha;           ///< Coefficient de filtrage exponentiel (0-1)

    // États internes
    float _ah;                      ///< Ah depuis la dernière remise à zéro
    float _ahBase;                  ///< Ah restaurés via setTotalAh (base)
    float _ahCumul;                 ///< Ah totaux (base + intégration)
    float _ahPerSecond;             ///< Ah par seconde (puissance)
    float _ahPerSecondAvg;          ///< Moyenne filtrée des Ah/s

    // Variables de temps
    unsigned long _lastMicros;       ///< Dernier timestamp en microsecondes
    float _lastIntervalSec;          ///< Dernier intervalle calculé en secondes
    float _avgIntervalSec;           ///< Intervalle moyen filtré
    bool _firstMeasurement;          ///< Indique si c'est la première mesure

    // Variables de courant
    float _current;                  ///< Dernier courant mesuré (A)
    float _currentAvg;               ///< Courant moyen filtré
    float _currentDirection;         ///< Direction du courant: 1=charge, -1=décharge

    // Statistiques
    unsigned long _sampleCount;      ///< Nombre d'échantillons traités
    float _maxAh;                    ///< Valeur maximale atteinte
    float _minAh;                    ///< Valeur minimale atteinte
    float _totalChargeAh;            ///< Total chargé (Ah, positif)
    float _totalDischargeAh;         ///< Total déchargé (Ah, positif)

    /**
     * @brief Calcule l'intervalle de temps en gérant le débordement de micros()
     */
    float calculateInterval(unsigned long currentMicros) {
        unsigned long diff;

        if (currentMicros >= _lastMicros) {
            diff = currentMicros - _lastMicros;
        } else {
            diff = (ULONG_MAX - _lastMicros) + currentMicros + 1;
        }

        return diff * 0.000001f; // Conversion microsecondes -> secondes
    }

public:
    /**
     * @brief Constructeur
     * @param filterAlpha Coefficient de filtrage (défaut: 0.99)
     */
    AmpereHeure(float filterAlpha = 0.99) :
        _filterAlpha(filterAlpha),
        _ah(0.0f),
        _ahBase(0.0f),
        _ahCumul(0.0f),
        _ahPerSecond(0.0f),
        _ahPerSecondAvg(0.0f),
        _lastMicros(0),
        _lastIntervalSec(0.001f),
        _avgIntervalSec(0.001f),
        _firstMeasurement(true),
        _current(0.0f),
        _currentAvg(0.0f),
        _currentDirection(0.0f),
        _sampleCount(0),
        _maxAh(-1e38f),
        _minAh(1e38f),
        _totalChargeAh(0.0f),
        _totalDischargeAh(0.0f) {}

    /**
     * @brief Initialise le compteur
     * @param initialAh Valeur initiale d'Ah (défaut: 0)
     */
    void begin(float initialAh = 0) {
        _ah = initialAh;
        _ahCumul = _ahBase + _ah;
        _sampleCount = 0;
        _lastMicros = micros();
        _firstMeasurement = true;
    }

    /**
     * @brief Ajoute une mesure de courant avec timestamp automatique
     * @param currentAmps Courant mesuré en Ampères (positif = charge)
     * @return Ampères-heures totaux après cette mesure
     */
    float addMeasurement(float currentAmps) {
        return addMeasurement(currentAmps, micros());
    }

    /**
     * @brief Ajoute une mesure de courant avec timestamp explicite
     * @param currentAmps Courant mesuré en Ampères
     * @param microsTimestamp Timestamp en microsecondes
     * @return Ampères-heures totaux après cette mesure
     */
    float addMeasurement(float currentAmps, unsigned long microsTimestamp) {
        _current = currentAmps;
        _currentDirection = (currentAmps > 0) ? 1.0f : (currentAmps < 0 ? -1.0f : 0.0f);

        if (_firstMeasurement || _lastMicros == 0) {
            _lastMicros = microsTimestamp;
            _currentAvg = currentAmps;
            _firstMeasurement = false;
            return _ahCumul;
        }

        _lastIntervalSec = calculateInterval(microsTimestamp);
        _lastMicros = microsTimestamp;

        return addMeasurementWithInterval(currentAmps, _lastIntervalSec);
    }

    /**
     * @brief Ajoute une mesure avec intervalle explicite
     * @param currentAmps Courant mesuré en Ampères
     * @param intervalSec Intervalle de temps en secondes
     * @return Ampères-heures totaux après cette mesure
     *
     * v2.0.0 : l'intégration est BRUTE (ΔAh = I · Δt / 3600). Le courant doit
     * déjà être pondéré par le rendement si le projet en applique un.
     */
    float addMeasurementWithInterval(float currentAmps, float intervalSec) {
        if (intervalSec <= 0 || intervalSec > 3600) {
            return _ahCumul;
        }

        // Garde : un échantillon NaN/Inf empoisonnerait définitivement le cumul.
        if (!isfinite(currentAmps)) return _ahCumul;

        _sampleCount++;

        // Filtrer le courant
        _currentAvg = _filterAlpha * _currentAvg + (1 - _filterAlpha) * currentAmps;
        _avgIntervalSec = _filterAlpha * _avgIntervalSec + (1 - _filterAlpha) * intervalSec;

        // Intégration pure : ΔAh = I × Δt / 3600
        float ahDelta = (currentAmps * intervalSec) / 3600.0f;

        // Cumuls séparés chargé / déchargé (comptage pur, sans rendement)
        if (currentAmps > 0) {
            _totalChargeAh += ahDelta;
        } else if (currentAmps < 0) {
            _totalDischargeAh += -ahDelta;   // stocké en positif
        }

        // Mettre à jour les cumuls
        _ah += ahDelta;
        _ahCumul = _ahBase + _ah;

        // Calculer les Ah par seconde
        _ahPerSecond = ahDelta / intervalSec;
        _ahPerSecondAvg = _filterAlpha * _ahPerSecondAvg +
                          (1 - _filterAlpha) * _ahPerSecond;

        // Mettre à jour les min/max
        if (_ahCumul > _maxAh) _maxAh = _ahCumul;
        if (_ahCumul < _minAh) _minAh = _ahCumul;

        return _ahCumul;
    }

    /**
     * @brief Ajoute directement une quantité d'Ah déjà intégrée
     * @param ahDelta Ampères-heures à ajouter (signé : < 0 = décharge)
     * @return Ampères-heures totaux après cet ajout
     *
     * v2.0.0 — point d'entrée pour un projet qui calcule lui-même son
     * incrément (rendement, Peukert, gazage…). Ne touche ni `_current` ni
     * `_lastMicros` (aucune notion de temps ici).
     */
    float addCharge(float ahDelta) {
        if (!isfinite(ahDelta)) return _ahCumul;

        _sampleCount++;
        if (ahDelta > 0.0f)      _totalChargeAh    += ahDelta;
        else if (ahDelta < 0.0f) _totalDischargeAh += -ahDelta;

        _ah += ahDelta;
        _ahCumul = _ahBase + _ah;

        if (_ahCumul > _maxAh) _maxAh = _ahCumul;
        if (_ahCumul < _minAh) _minAh = _ahCumul;

        return _ahCumul;
    }

    /**
     * @brief Réinitialise le compteur
     */
    void reset() {
        _ah = 0.0f;
        _ahCumul = _ahBase + _ah;
        _sampleCount = 0;
        _totalChargeAh = 0.0f;
        _totalDischargeAh = 0.0f;
        _ahPerSecond = 0.0f;
        _ahPerSecondAvg = 0.0f;
        _currentAvg = 0.0f;
        _maxAh = -1e38f;
        _minAh = 1e38f;
    }

    /**
     * @brief Rétro-compatibilité : reset(bool) — l'argument est ignoré.
     * @param resetToFull Ignoré (plus de notion de capacité dans la librairie).
     */
    void reset(bool resetToFull) {
        (void)resetToFull;
        reset();
    }

    /**
     * @brief Restaure une valeur cumulée (appelé après un load de persistance)
     * @param ah Valeur cumulée à restaurer (Ah totaux)
     *
     * Définit la base sur laquelle l'intégration s'ajoutera.
     * À utiliser avec la librairie Fo170/Persistance :
     *   persistance.loadFloat("ah", &v);
     *   ahCounter.setTotalAh(v);
     */
    void setTotalAh(float ah) {
        _ahBase = ah;
        _ahCumul = _ahBase + _ah;
    }

    // ========== Getters ==========

    /**
     * @return Ampères-heures depuis la dernière réinitialisation
     */
    float getAh() const { return _ah; }

    /**
     * @return Ampères-heures totaux (persistés + depuis démarrage)
     */
    float getTotalAh() const { return _ahCumul; }

    /**
     * @return Courant actuel
     */
    float getCurrent() const { return _current; }

    /**
     * @return Courant moyen filtré
     */
    float getAverageCurrent() const { return _currentAvg; }

    /**
     * @return Direction du courant: 1=charge, -1=décharge, 0=repos
     */
    float getCurrentDirection() const { return _currentDirection; }

    /**
     * @return Vrai si en charge (> 100 mA)
     */
    bool isCharging() const { return _current > 0.1f; }

    /**
     * @return Vrai si en décharge (< −100 mA)
     */
    bool isDischarging() const { return _current < -0.1f; }

    /**
     * @return Total chargé (Ah, positif)
     */
    float getTotalCharged() const { return _totalChargeAh; }

    /**
     * @return Total déchargé (Ah, positif)
     */
    float getTotalDischarged() const { return _totalDischargeAh; }

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
     * @return Valeur maximale atteinte
     */
    float getMaxAh() const { return _maxAh; }

    /**
     * @return Valeur minimale atteinte
     */
    float getMinAh() const { return _minAh; }

    // ========== Configuration ==========

    /**
     * @brief Configure le coefficient de filtrage
     * @param alpha Nouveau coefficient (0-1)
     */
    void setFilterAlpha(float alpha) {
        if (alpha >= 0 && alpha <= 1) {
            _filterAlpha = alpha;
        }
    }

    // ========== Debug ==========

    /**
     * @brief Affiche toutes les statistiques
     */
    void printDebug() {
        Serial.println(F("=== AMPERE-HEURE COUNTER ==="));
        Serial.print(F("Ah (session)   : ")); Serial.println(_ah, 6);
        Serial.print(F("Ah (total)     : ")); Serial.println(_ahCumul, 6);
        Serial.print(F("Total chargé   : ")); Serial.println(_totalChargeAh, 6);
        Serial.print(F("Total déchargé : ")); Serial.println(_totalDischargeAh, 6);
        Serial.print(F("Courant        : ")); Serial.println(_current, 3);
        Serial.print(F("Courant moyen  : ")); Serial.println(_currentAvg, 3);
        Serial.print(F("Intervalle     : ")); Serial.println(_lastIntervalSec, 4);
        Serial.print(F("Échantillons   : ")); Serial.println(_sampleCount);
        Serial.println(F("==========================="));
    }
};

#endif // AMPEREHEURE_H
