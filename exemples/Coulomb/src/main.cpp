/**
 * @file main.cpp
 * @brief Exemple d'utilisation de la librairie Coulomb.h
 *
 * Comptage de la charge électrique en Coulombs (1 C = 1 A·s)
 * à partir de mesures de courant et d'intervalles de temps.
 *
 * La librairie gère :
 * - l'intégration I × Δt sur un timestamp microsecondes (micros())
 * - le débordement de micros() (~70 min)
 * - le filtrage exponentiel du courant
 * - la persistance optionnelle en EEPROM
 *
 * Matériel :
 * - ESP8266 (WeMos D1 Mini / Lolin)
 *
 * Version : 1.0.0
 */

#include <Arduino.h>
#include "Coulomb.h"

// ==================== CONFIGURATION ====================
#define EEPROM_ADDR          0      // Adresse EEPROM pour la persistance (4 octets)
#define FILTER_ALPHA         0.99   // Coefficient de filtrage (proche de 1 = lissage fort)
#define PRINT_INTERVAL_MS    1000   // Affichage toutes les 1 s
#define SAVE_INTERVAL_MS     60000  // Sauvegarde EEPROM toutes les 60 s

// ==================== OBJETS GLOBAUX ====================
// Constructeur avec persistance EEPROM : charge la valeur sauvegardée au démarrage.
Coulomb coulomb(EEPROM_ADDR, FILTER_ALPHA);

// ==================== MESURE DU COURANT ====================

/**
 * @brief Mesure le courant traversant la charge en Ampères.
 *
 * Valeur simulée pour une démonstration autonome sans capteur.
 * Remplacer par une vraie mesure, par exemple un capteur INA226 sur I2C :
 *   float readCurrent() {
 *     // retourner la lecture du registre de courant de l'INA226
 *   }
 */
float readCurrent() {
  // Courant sinusoïdal : tantôt positif (charge), tantôt négatif (décharge)
  float seconds = (float)micros() / 1000000.0f;
  return 0.5f * sinf(2.0f * PI * seconds / 30.0f);
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=== EXEMPLE Coulomb.h ===");

  // Démarrage : valeur initiale 0 C, timestamp réinitialisé
  coulomb.begin(0.0f, true);
  coulomb.enableMinMaxTracking(true);

  Serial.print("Valeur persistée : ");
  Serial.print(coulomb.getPersistedValue(), 3);
  Serial.println(" C");
}

// ==================== BOUCLE PRINCIPALE ====================
void loop() {
  // Intègre courant × Δt en Coulombs avec le timestamp courant
  coulomb.addMeasurement(readCurrent(), micros());

  static unsigned long lastPrint = 0;
  static unsigned long lastSave = 0;
  unsigned long now = millis();

  // Affichage périodique
  if (now - lastPrint >= PRINT_INTERVAL_MS) {
    Serial.println("----------------------------");
    Serial.print("Courant                : "); Serial.print(coulomb.getCurrent(), 3); Serial.println(" A");
    Serial.print("Courant moyen (filtré) : "); Serial.print(coulomb.getAverageCurrent(), 3); Serial.println(" A");
    Serial.print("Coulombs (reset)       : "); Serial.print(coulomb.getCoulombs(), 3); Serial.println(" C");
    Serial.print("Coulombs (total)       : "); Serial.print(coulomb.getTotalCoulombs(), 3); Serial.println(" C");
    Serial.print("Ampères-heures (total) : "); Serial.print(coulomb.getAmpereHours(), 3); Serial.println(" Ah");
    Serial.print("C/s (instantané)       : "); Serial.print(coulomb.getCoulombsPerSecond(), 3); Serial.println(" C/s");
    Serial.print("Intervalle             : "); Serial.print(coulomb.getLastInterval() * 1000.0f, 2); Serial.println(" ms");
    Serial.print("Échantillons           : "); Serial.println(coulomb.getSampleCount());
    lastPrint = now;
  }

  // Sauvegarde périodique en EEPROM
  if (now - lastSave >= SAVE_INTERVAL_MS) {
    coulomb.save();
    Serial.println("\nDonnées sauvegardées en EEPROM.");
    lastSave = now;
  }
}
