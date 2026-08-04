/**
 * @file main.cpp
 * @brief Exemple d'utilisation de la librairie AmpereHeure.h + Persistance
 *
 * Monitoring de batterie :
 * - Ampères-heures cumulés (1 Ah = 3600 C)
 * - État de charge (State of Charge, SoC)
 * - Efficacités de charge / décharge
 * - Alerte batterie faible
 * - Estimation de l'autonomie restante
 *
 * La persistance est déléguée à Fo170/Persistance (EEPROM, FRAM ou LittleFS).
 * Ici : PersistanceEEPROM (512 o). On sauvegarde getTotalAh() et on restaure
 * via setTotalAh().
 *
 * Matériel :
 * - ESP8266 (WeMos D1 Mini / Lolin)
 *
 * Version : 1.1.0
 */

#include <Arduino.h>
#include "AmpereHeure.h"
#include <persistance_EEPROM.h>

// ==================== CONFIGURATION ====================
#define BATTERY_CAPACITY_AH  14.0   // Capacité nominale de la batterie en Ah
#define EEPROM_SIZE          512    // Taille EEPROM utilisée
#define FILTER_ALPHA         0.99   // Coefficient de filtrage
#define PRINT_INTERVAL_MS    1000   // Affichage toutes les 1 s
#define SAVE_INTERVAL_MS     60000  // Sauvegarde toutes les 60 s

// ==================== OBJETS GLOBAUX ====================
AmpereHeure ah(BATTERY_CAPACITY_AH, FILTER_ALPHA);
PersistanceEEPROM persistance(EEPROM_SIZE);

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

  Serial.println("\n=== EXEMPLE AmpereHeure.h + Persistance ===");

  if (!persistance.begin()) {
    Serial.println("Échec EEPROM.begin");
    return;
  }
  persistance.addRegister("ah", sizeof(float));

  // Restauration depuis la persistance.
  float savedAh = 0.0f;
  if (persistance.loadFloat("ah", &savedAh)) {
    Serial.print("Ah restaurés : "); Serial.print(savedAh, 2); Serial.println(" Ah");
    ah.setTotalAh(savedAh);
  } else {
    Serial.println("Premier démarrage : compteur à 0 Ah");
  }

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

  // Sauvegarde périodique via Persistance
  if (now - lastSave >= SAVE_INTERVAL_MS) {
    persistance.saveFloat("ah", ah.getTotalAh());
    Serial.println("\nAh sauvegardés via Persistance.");
    lastSave = now;
  }
}
