/**
 * @file Coulomb.h
 * @brief Librairie pour le comptage de Coulombs (charge électrique)
 * 
 * Classe unique pour mesurer et cumuler la charge électrique en Coulombs
 * à partir de mesures de courant et d'intervalles de temps.
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
 * Version: 1.1.0
 * Date: Août 2026
 */

#ifndef COULOMB_H
#define COULOMB_H

// Version de la librairie (source UNIQUE — agrégée par src/liste_lib.h)
#define __COULOMB_H_VERSION__ "1.2.0"

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
     */
    float addMeasurementWithInterval(float currentAmps, float intervalSec) {
        // Validation
        if (intervalSec <= 0 || intervalSec > 3600) { // Max 1 heure
            return _coulombCumul;
        }
        
        // Mise à jour des statistiques
        _sampleCount++;
        
        // Filtrer le courant
        _currentAvg = _filterAlpha * _currentAvg + (1 - _filterAlpha) * currentAmps;
        
        // Filtrer l'intervalle
        _avgIntervalSec = _filterAlpha * _avgIntervalSec + (1 - _filterAlpha) * intervalSec;
        
        // Calculer les Coulombs pour cet intervalle (I × Δt)
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
     * @brief Réinitialise le compteur à zéro
     */
    void reset() {
        _coulomb = 0.0f;
        _coulombBase = 0.0f;
        _coulombCumul = 0.0f;
        _sampleCount = 0;
        
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
     * @brief Active/désactive le suivi des min/max
     * @param enable true pour activer
     */
    void enableMinMaxTracking(bool enable) {
        _trackMinMax = enable;
        if (enable) {
            _maxCoulomb = _coulombCumul;
            _minCoulomb = _coulombCumul;
        }
    }
    
    /**
     * @brief Force une valeur de courant moyen (utile pour l'initialisation)
     * @param current Valeur de courant à utiliser
     */
    void setAverageCurrent(float current) {
        _currentAvg = current;
    }
    
    /**
     * @brief Affiche toutes les statistiques sur la console série
     */
    void printDebug() {
        Serial.println("=== COULOMB COUNTER DEBUG ===");
        Serial.print("Coulombs (current): "); Serial.println(_coulomb, 6);
        Serial.print("Coulombs (total): "); Serial.println(_coulombCumul, 6);
        Serial.print("C/s: "); Serial.println(_coulombPerSecond, 6);
        Serial.print("C/s avg: "); Serial.println(_coulombPerSecondAvg, 6);
        Serial.print("Current: "); Serial.print(_current, 3); Serial.println(" A");
        Serial.print("Current avg: "); Serial.print(_currentAvg, 3); Serial.println(" A");
        Serial.print("Interval: "); Serial.print(_lastIntervalSec * 1000, 3); Serial.println(" ms");
        Serial.print("Interval avg: "); Serial.print(_avgIntervalSec * 1000, 3); Serial.println(" ms");
        Serial.print("Samples: "); Serial.println(_sampleCount);
        Serial.print("Ampere-hours: "); Serial.print(getAmpereHours(), 6); Serial.println(" Ah");
        if (_trackMinMax) {
            Serial.print("Max: "); Serial.println(_maxCoulomb, 6);
            Serial.print("Min: "); Serial.println(_minCoulomb, 6);
        }
        Serial.println("===============================");
    }
};

#endif // COULOMB_H