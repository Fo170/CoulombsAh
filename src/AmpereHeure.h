/**
 * @file AmpereHeure.h
 * @brief Librairie pour le comptage d'Ampères-heures (capacité de batterie)
 * 
 * Classe unique pour mesurer et cumuler la capacité en Ampères-heures
 * à partir de mesures de courant et d'intervalles de temps.
 * 1 Ah = 3600 Coulombs = 3600 Ampères × 1 seconde
 * 
 * Utilisation:
 * @code
 * #include "AmpereHeure.h"
 * 
 * AmpereHeure ahCounter(14.0); // Batterie de 14Ah
 * 
 * void loop() {
 *   float courant = lireCourant(); // votre fonction de mesure
 *   ahCounter.addMeasurement(courant, micros());
 *   
 *   if (millis() - lastDisplay > 1000) {
 *     Serial.print("État de charge: "); 
 *     Serial.print(ahCounter.getStateOfCharge() * 100);
 *     Serial.println("%");
 *   }
 * }
 * @endcode
 * 
 * Version: 1.0.0
 * Date: Mars 2026
 */

#ifndef AMPEREHEURE_H
#define AMPEREHEURE_H

#include <Arduino.h>
#include <EEPROM.h>
#include <climits>

/**
 * @class AmpereHeure
 * @brief Compteur d'Ampères-heures avec gestion de capacité batterie
 * 
 * Calcule et cumule les Ampères-heures (1 Ah = 3600 Coulombs)
 * avec des fonctionnalités spécifiques pour le monitoring de batterie:
 * - État de charge (State of Charge - SoC)
 * - Capacité totale/configurable
 * - Alertes de décharge complète
 * - Efficacité de charge/décharge
 * - Persistance optionnelle
 */
class AmpereHeure {
private:
    // Paramètres de configuration
    float _filterAlpha;           ///< Coefficient de filtrage exponentiel (0-1)
    bool _usePersistentStorage;    ///< Active la sauvegarde en EEPROM
    int _eepromAddress;            ///< Adresse de base en EEPROM (4 bytes)
    
    // Paramètres batterie
    float _batteryCapacityAh;      ///< Capacité nominale de la batterie en Ah
    float _chargeEfficiency;       ///< Efficacité en charge (0-1)
    float _dischargeEfficiency;    ///< Efficacité en décharge (0-1)
    
    // États internes
    float _ah;                      ///< Ah depuis la dernière remise à zéro
    float _ahCumul;                 ///< Ah totaux (avec valeur sauvegardée)
    float _ahPersisted;             ///< Valeur sauvegardée avant démarrage
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
    float _totalChargeAh;            ///< Total chargé
    float _totalDischargeAh;         ///< Total déchargé
    
    // État de charge
    float _stateOfCharge;            ///< 0.0 à 1.0 (0% à 100%)
    bool _lowBatteryAlert;           ///< Alerte batterie faible
    float _lowBatteryThreshold;      ///< Seuil d'alerte (ex: 0.2 = 20%)
    
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
    
    /**
     * @brief Charge la valeur persistante depuis l'EEPROM
     */
    void loadFromEEPROM() {
        if (!_usePersistentStorage) return;
        
        EEPROM.begin(4);
        EEPROM.get(_eepromAddress, _ahPersisted);
        EEPROM.end();
        
        if (isnan(_ahPersisted) || isinf(_ahPersisted)) {
            _ahPersisted = 0.0f;
        }
    }
    
    /**
     * @brief Sauvegarde la valeur courante en EEPROM
     */
    void saveToEEPROM() {
        if (!_usePersistentStorage) return;
        
        EEPROM.begin(4);
        EEPROM.put(_eepromAddress, _ahCumul);
        EEPROM.commit();
        EEPROM.end();
    }
    
    /**
     * @brief Met à jour l'état de charge
     */
    void updateStateOfCharge() {
        if (_batteryCapacityAh > 0) {
            // Calculer le SoC à partir des Ah cumulés
            // Note: _ahCumul peut être négatif (décharge) ou positif (charge)
            float netAh = _ahCumul;
            
            // Normaliser entre 0 et 1 par rapport à la capacité
            _stateOfCharge = netAh / _batteryCapacityAh;
            
            // Saturer entre 0 et 1
            if (_stateOfCharge > 1.0f) _stateOfCharge = 1.0f;
            if (_stateOfCharge < 0.0f) _stateOfCharge = 0.0f;
            
            // Vérifier l'alerte batterie faible
            _lowBatteryAlert = (_stateOfCharge <= _lowBatteryThreshold);
        }
    }
    
public:
    /**
     * @brief Constructeur sans persistance EEPROM
     * @param batteryCapacityAh Capacité nominale de la batterie en Ah
     * @param filterAlpha Coefficient de filtrage (défaut: 0.99)
     */
    AmpereHeure(float batteryCapacityAh, float filterAlpha = 0.99) : 
        _filterAlpha(filterAlpha),
        _usePersistentStorage(false),
        _eepromAddress(0),
        _batteryCapacityAh(batteryCapacityAh),
        _chargeEfficiency(1.0f),
        _dischargeEfficiency(1.0f),
        _ah(0.0f),
        _ahCumul(0.0f),
        _ahPersisted(0.0f),
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
        _totalDischargeAh(0.0f),
        _stateOfCharge(0.0f),
        _lowBatteryAlert(false),
        _lowBatteryThreshold(0.2f) {} // 20% par défaut
    
    /**
     * @brief Constructeur avec persistance EEPROM
     * @param eepromAddress Adresse en EEPROM
     * @param batteryCapacityAh Capacité nominale de la batterie
     * @param filterAlpha Coefficient de filtrage
     */
    AmpereHeure(int eepromAddress, float batteryCapacityAh, float filterAlpha = 0.99) : 
        _filterAlpha(filterAlpha),
        _usePersistentStorage(true),
        _eepromAddress(eepromAddress),
        _batteryCapacityAh(batteryCapacityAh),
        _chargeEfficiency(1.0f),
        _dischargeEfficiency(1.0f),
        _ah(0.0f),
        _ahCumul(0.0f),
        _ahPersisted(0.0f),
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
        _totalDischargeAh(0.0f),
        _stateOfCharge(0.0f),
        _lowBatteryAlert(false),
        _lowBatteryThreshold(0.2f) {
        
        loadFromEEPROM();
        _ahCumul = _ahPersisted;
        updateStateOfCharge();
    }
    
    /**
     * @brief Initialise le compteur
     * @param initialAh Valeur initiale d'Ah (défaut: 0)
     * @param initialSoC État de charge initial (0-1, si >0, prioritaire)
     */
    void begin(float initialAh = 0, float initialSoC = -1.0f) {
        if (initialSoC >= 0 && initialSoC <= 1.0f) {
            // Initialiser à partir du SoC
            _ah = initialSoC * _batteryCapacityAh;
        } else {
            _ah = initialAh;
        }
        
        _ahCumul = _ahPersisted + _ah;
        _sampleCount = 0;
        _lastMicros = micros();
        _firstMeasurement = true;
        
        updateStateOfCharge();
    }
    
    /**
     * @brief Ajoute une mesure de courant avec timestamp automatique
     * @param currentAmps Courant mesuré en Ampères (positif = charge, négatif = décharge)
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
        _currentDirection = (currentAmps > 0) ? 1.0f : -1.0f;
        
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
     */
    float addMeasurementWithInterval(float currentAmps, float intervalSec) {
        if (intervalSec <= 0 || intervalSec > 3600) {
            return _ahCumul;
        }
        
        _sampleCount++;
        
        // Filtrer le courant
        _currentAvg = _filterAlpha * _currentAvg + (1 - _filterAlpha) * currentAmps;
        _avgIntervalSec = _filterAlpha * _avgIntervalSec + (1 - _filterAlpha) * intervalSec;
        
        // Calculer les Ah pour cet intervalle (I × Δt / 3600)
        float ahDelta = (currentAmps * intervalSec) / 3600.0f;
        
        // Appliquer l'efficacité selon la direction
        if (currentAmps > 0) { // Charge
            ahDelta *= _chargeEfficiency;
            _totalChargeAh += ahDelta;
        } else if (currentAmps < 0) { // Décharge
            ahDelta *= _dischargeEfficiency;
            _totalDischargeAh += -ahDelta; // Stocker en positif
        }
        
        // Mettre à jour les cumuls
        _ah += ahDelta;
        _ahCumul = _ahPersisted + _ah;
        
        // Calculer les Ah par seconde
        _ahPerSecond = ahDelta / intervalSec;
        _ahPerSecondAvg = _filterAlpha * _ahPerSecondAvg + 
                          (1 - _filterAlpha) * _ahPerSecond;
        
        // Mettre à jour les min/max
        if (_ahCumul > _maxAh) _maxAh = _ahCumul;
        if (_ahCumul < _minAh) _minAh = _ahCumul;
        
        // Mettre à jour l'état de charge
        updateStateOfCharge();
        
        return _ahCumul;
    }
    
    /**
     * @brief Réinitialise le compteur
     * @param keepPersisted Si true, conserve la valeur persistée
     * @param resetToFull Si true, réinitialise à pleine charge (capacité)
     */
    void reset(bool keepPersisted = true, bool resetToFull = false) {
        if (resetToFull) {
            _ah = _batteryCapacityAh;
        } else {
            _ah = 0.0f;
        }
        
        _sampleCount = 0;
        
        if (keepPersisted) {
            _ahCumul = _ahPersisted + _ah;
        } else {
            _ahCumul = _ah;
            _ahPersisted = 0.0f;
        }
        
        _totalChargeAh = 0.0f;
        _totalDischargeAh = 0.0f;
        
        updateStateOfCharge();
    }
    
    /**
     * @brief Sauvegarde la valeur courante en EEPROM
     */
    void save() {
        if (!_usePersistentStorage) return;
        
        _ahPersisted = _ahCumul;
        saveToEEPROM();
    }
    
    // ========== Configuration batterie ==========
    
    /**
     * @brief Définit la capacité de la batterie
     * @param capacityAh Nouvelle capacité en Ah
     */
    void setBatteryCapacity(float capacityAh) {
        if (capacityAh > 0) {
            _batteryCapacityAh = capacityAh;
            updateStateOfCharge();
        }
    }
    
    /**
     * @brief Définit les efficacités de charge/décharge
     * @param charge Efficiency en charge (0-1, défaut: 0.95)
     * @param discharge Efficiency en décharge (0-1, défaut: 0.95)
     */
    void setEfficiencies(float charge, float discharge) {
        if (charge > 0 && charge <= 1) _chargeEfficiency = charge;
        if (discharge > 0 && discharge <= 1) _dischargeEfficiency = discharge;
    }
    
    /**
     * @brief Définit le seuil d'alerte batterie faible
     * @param threshold Seuil (0-1, ex: 0.2 pour 20%)
     */
    void setLowBatteryThreshold(float threshold) {
        if (threshold >= 0 && threshold <= 1) {
            _lowBatteryThreshold = threshold;
            updateStateOfCharge();
        }
    }
    
    /**
     * @brief Calibre l'état de charge
     * @param actualSoC État de charge réel mesuré (0-1)
     * 
     * Permet de recaler le compteur avec une mesure réelle (ex: tension batterie)
     */
    void calibrateStateOfCharge(float actualSoC) {
        if (actualSoC < 0 || actualSoC > 1) return;
        
        float targetAh = actualSoC * _batteryCapacityAh;
        float correction = targetAh - _ahCumul;
        
        _ah += correction;
        _ahCumul = _ahPersisted + _ah;
        
        updateStateOfCharge();
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
     * @return État de charge (0.0 à 1.0)
     */
    float getStateOfCharge() const { return _stateOfCharge; }
    
    /**
     * @return État de charge en pourcentage (0-100)
     */
    float getStateOfChargePercent() const { return _stateOfCharge * 100.0f; }
    
    /**
     * @return Vrai si batterie faible (sous le seuil)
     */
    bool isLowBattery() const { return _lowBatteryAlert; }
    
    /**
     * @return Capacité restante estimée en Ah
     */
    float getRemainingCapacity() const { return _stateOfCharge * _batteryCapacityAh; }
    
    /**
     * @return Capacité nominale de la batterie
     */
    float getBatteryCapacity() const { return _batteryCapacityAh; }
    
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
     * @return Vrai si en charge
     */
    bool isCharging() const { return _current > 0.1f; } // Seuil de 100mA
    
    /**
     * @return Vrai si en décharge
     */
    bool isDischarging() const { return _current < -0.1f; }
    
    /**
     * @return Total chargé (Ah)
     */
    float getTotalCharged() const { return _totalChargeAh; }
    
    /**
     * @return Total déchargé (Ah)
     */
    float getTotalDischarged() const { return _totalDischargeAh; }
    
    /**
     * @return Nombre de cycles estimé (décharge totale / capacité)
     */
    float getEstimatedCycles() const { 
        if (_batteryCapacityAh > 0) {
            return _totalDischargeAh / _batteryCapacityAh;
        }
        return 0;
    }
    
    /**
     * @return Puissance en Watts (si tension connue)
     * @param batteryVoltage Tension batterie en Volts
     */
    float getPower(float batteryVoltage) const {
        return _current * batteryVoltage;
    }
    
    /**
     * @return Autonomie restante en heures (au courant actuel)
     */
    float getEstimatedRuntimeHours() const {
        float currentAbs = abs(_current);
        if (currentAbs > 0.01f) {
            return getRemainingCapacity() / currentAbs;
        }
        return INFINITY; // Courant nul = autonomie infinie
    }
    
    /**
     * @return Temps restant formaté en heures:minutes
     */
    String getEstimatedRuntimeFormatted() const {
        float hours = getEstimatedRuntimeHours();
        if (isinf(hours)) return "Infinite";
        
        int h = (int)hours;
        int m = (int)((hours - h) * 60);
        return String(h) + "h " + (m < 10 ? "0" : "") + String(m) + "m";
    }
    
    // ========== Debug ==========
    
    /**
     * @brief Affiche toutes les statistiques
     */
    void printDebug() {
        Serial.println("=== AMPERE-HEURE COUNTER DEBUG ===");
        Serial.print("Ah (current): "); Serial.println(_ah, 6);
        Serial.print("Ah (total): "); Serial.println(_ahCumul, 6);
        Serial.print("Persisted: "); Serial.println(_ahPersisted, 6);
        Serial.print("State of Charge: "); Serial.print(_stateOfCharge * 100, 1); Serial.println("%");
        Serial.print("Remaining: "); Serial.print(getRemainingCapacity(), 3); Serial.println(" Ah");
        Serial.print("Battery capacity: "); Serial.print(_batteryCapacityAh, 1); Serial.println(" Ah");
        Serial.print("Current: "); Serial.print(_current, 3); Serial.println(" A");
        Serial.print("Current avg: "); Serial.print(_currentAvg, 3); Serial.println(" A");
        Serial.print("Direction: "); 
        if (isCharging()) Serial.println("CHARGING");
        else if (isDischarging()) Serial.println("DISCHARGING");
        else Serial.println("IDLE");
        Serial.print("Total charged: "); Serial.print(_totalChargeAh, 3); Serial.println(" Ah");
        Serial.print("Total discharged: "); Serial.print(_totalDischargeAh, 3); Serial.println(" Ah");
        Serial.print("Estimated cycles: "); Serial.println(getEstimatedCycles(), 1);
        Serial.print("Runtime: "); Serial.println(getEstimatedRuntimeFormatted());
        Serial.print("Low battery: "); Serial.println(_lowBatteryAlert ? "YES" : "NO");
        Serial.print("Efficiencies (C/D): "); 
        Serial.print(_chargeEfficiency * 100, 0); Serial.print("% / ");
        Serial.print(_dischargeEfficiency * 100, 0); Serial.println("%");
        Serial.println("===============================");
    }
};

#endif // AMPEREHEURE_H