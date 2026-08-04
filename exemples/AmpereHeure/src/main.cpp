/**
 * @file main.cpp
 * @brief Exemple d'utilisation de la librairie AmpereHeure.h
 *
 * Monitoring de batterie :
 * - Ampères-heures cumulés (1 Ah = 3600 C)
 * - État de charge (State of Charge, SoC)
 * - Efficacités de charge / décharge
 * - Alerte batterie faible
 * - Estimation de l'autonomie restante
 *
 * Matériel :
 * - ESP8266 (WeMos D1 Mini / Lolin)
 *
 * Version : 1.0.0
 */

#include <Arduino.h>
#include "AmpereHeure.h"

// ==================== CONFIGURATION ====================
#define BATTERY_CAPACITY_AH  14.0   // Capacité nominale de la batterie en Ah
#define EEPROM_ADDR          0      // Adresse EEPROM pour la persistance (4 octets)
#define FILTER_ALPHA         0.99   // Coefficient de filtrage
#define PRINT_INTERVAL_MS    1000   // Affichage toutes les 1 s
#define SAVE_INTERVAL_MS     60000  // Sauvegarde EEPROM toutes les 60 s

// ==================== OBJETS GLOBAUX ====================
// Constructeur avec persistance EEPROM : charge la valeur sauvegardée au démarrage.
AmpereHeure ah(EEPROM_ADDR, BATTERY_CAPACITY_AH, FILTER_ALPHA);

// ==================== MESURE DU COURANT ====================

/**
 * @brief Mesure le courant en Ampères (positif = charge, négatif = décharge).
 *
 * Valeur simulée : cycle de 30 s alternant 15 s de charge (+1 A)
 * et 15 s de décharge (-1 A). Remplacer par une vraie mesure (ex: INA226).
 */
float readCurrent() {
  unsigned long seconds = millis() / 1000UL;
  return ((seconds / 15UL) % 2UL == 0) ? 1.0f : -1.0f;
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=== EXEMPLE AmpereHeure.h ===");

  // Démarrage à 90 % de charge (batterie presque pleine)
  ah.begin(0.0f, 0.90f);
  ah.setEfficiencies(0.96, 0.98);      // 96 % en charge, 98 % en décharge
  ah.setLowBatteryThreshold(0.15);     // Alerte batterie faible à 15 %

  Serial.print("Capacité batterie : ");
  Serial.print(ah.getBatteryCapacity(), 1);
  Serial.println(" Ah");
  Serial.print("État de charge    : ");
  Serial.print(ah.getStateOfChargePercent(), 1);
  Serial.println(" %");
}

// ==================== BOUCLE PRINCIPALE ====================
void loop() {
  // Intègre courant × Δt / 3600 en Ampères-heures avec le timestamp courant
  ah.addMeasurement(readCurrent(), micros());

  static unsigned long lastPrint = 0;
  static unsigned long lastSave = 0;
  unsigned long now = millis();

  // Affichage périodique
  if (now - lastPrint >= PRINT_INTERVAL_MS) {
    Serial.println("----------------------------");
    Serial.print("Courant         : "); Serial.print(ah.getCurrent(), 3); Serial.println(" A");
    Serial.print("État de charge  : "); Serial.print(ah.getStateOfChargePercent(), 1); Serial.println(" %");
    Serial.print("Restant         : "); Serial.print(ah.getRemainingCapacity(), 2); Serial.println(" Ah");
    Serial.print("Autonomie       : "); Serial.println(ah.getEstimatedRuntimeFormatted());
    Serial.print("Ah (total)      : "); Serial.print(ah.getTotalAh(), 3); Serial.println(" Ah");
    Serial.print("Charge totale   : "); Serial.print(ah.getTotalCharged(), 2); Serial.println(" Ah");
    Serial.print("Décharge totale : "); Serial.print(ah.getTotalDischarged(), 2); Serial.println(" Ah");
    Serial.print("Cycles estimés  : "); Serial.println(ah.getEstimatedCycles(), 1);

    if (ah.isCharging()) Serial.println("Mode : CHARGE");
    else if (ah.isDischarging()) Serial.println("Mode : DÉCHARGE");
    else Serial.println("Mode : REPOS");

    if (ah.isLowBattery()) Serial.println("ATTENTION : BATTERIE FAIBLE !");
    lastPrint = now;
  }

  // Sauvegarde périodique en EEPROM
  if (now - lastSave >= SAVE_INTERVAL_MS) {
    ah.save();
    Serial.println("\nDonnées sauvegardées en EEPROM.");
    lastSave = now;
  }
}
