/**
 * @file main.cpp
 * @brief Exemple d'utilisation de la librairie AmpereHeure.h + Persistance
 *
 * Depuis la v2.0.0, `AmpereHeure` est un INTÉGRATEUR PUR : il cumule
 * ΔAh = I · Δt / 3600 et ne connaît ni rendement, ni Peukert, ni état de
 * charge, ni capacité batterie. Ces notions appartiennent au projet.
 *
 * Cet exemple montre donc les deux niveaux :
 *   1. la librairie qui compte (getTotalAh, getTotalCharged, getTotalDischarged)
 *   2. une petite politique locale (rendement + SoC) écrite ICI, dans le sketch
 *
 * La persistance est déléguée à Fo170/Persistance (EEPROM, FRAM ou LittleFS).
 * Ici : PersistanceEEPROM (512 o). On sauvegarde getTotalAh() et on restaure
 * via setTotalAh().
 *
 * Matériel :
 * - ESP8266 (WeMos D1 Mini / Lolin)
 *
 * Version : 2.0.0
 */

#include <Arduino.h>
#include "AmpereHeure.h"
#include <persistance_EEPROM.h>

// ==================== CONFIGURATION ====================
// Politique du PROJET (n'appartient pas à la librairie)
#define BATTERY_CAPACITY_AH  14.0f  // Capacité nominale de la batterie en Ah
#define ETA_CHARGE           0.96f  // Rendement faradique en charge
#define ETA_DECHARGE         0.98f  // Rendement faradique en décharge
#define SEUIL_BATTERIE_BASSE 0.15f  // Alerte à 15 % de SoC
#define SOC_INITIAL          0.90f  // SoC au premier démarrage

#define EEPROM_SIZE          512    // Taille EEPROM utilisée
#define FILTER_ALPHA         0.99f  // Coefficient de filtrage
#define PRINT_INTERVAL_MS    1000   // Affichage toutes les 1 s
#define SAVE_INTERVAL_MS     60000  // Sauvegarde toutes les 60 s

// ==================== OBJETS GLOBAUX ====================
AmpereHeure ah(FILTER_ALPHA);       // v2.0.0 : plus de capacité au constructeur
PersistanceEEPROM persistance(EEPROM_SIZE);

// SoC tenu par le PROJET : c'est une politique, pas du comptage.
// (Dans un vrai projet, le SoC viendrait d'un filtre de Kalman ou d'un
//  modèle batterie ; ici on le dérive simplement des Ah nets.)
float soc = SOC_INITIAL;

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

/**
 * @brief Rendement faradique appliqué par le PROJET avant l'intégration.
 *
 * C'est ici que se décide la physique. La librairie, elle, intégrerait le
 * courant tel quel. Un projet réel utiliserait une loi plus fine (rendement
 * dépendant de la tension, du SoC, du dégagement gazeux…).
 */
float rendement(float I) {
  if (I > 0.0f) return ETA_CHARGE;
  if (I < 0.0f) return ETA_DECHARGE;
  return 1.0f;
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=== EXEMPLE AmpereHeure.h v2.0.0 + Persistance ===");

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

  ah.begin(0.0f);

  Serial.print("Capacité batterie : "); Serial.print(BATTERY_CAPACITY_AH, 1); Serial.println(" Ah");
  Serial.print("SoC initial       : "); Serial.print(soc * 100.0f, 1); Serial.println(" %");
}

// ==================== BOUCLE PRINCIPALE ====================
void loop() {
  float I = readCurrent();

  // 1) La librairie compte — le courant est pondéré par le PROJET.
  ah.addMeasurement(I * rendement(I), micros());

  // 2) Le PROJET met à jour son SoC à partir du comptage.
  //    Ah net = chargé − déchargé (exact, cumuls séparés dans la librairie).
  float ahNet = ah.getTotalCharged() - ah.getTotalDischarged();
  soc = SOC_INITIAL + ahNet / BATTERY_CAPACITY_AH;
  if (soc > 1.0f) soc = 1.0f;
  if (soc < 0.0f) soc = 0.0f;

  static unsigned long lastPrint = 0;
  static unsigned long lastSave = 0;
  unsigned long now = millis();

  // Affichage périodique
  if (now - lastPrint >= PRINT_INTERVAL_MS) {
    Serial.println("----------------------------");
    Serial.print("Courant         : "); Serial.print(ah.getCurrent(), 3); Serial.println(" A");
    Serial.print("SoC (projet)    : "); Serial.print(soc * 100.0f, 1); Serial.println(" %");
    Serial.print("Restant         : "); Serial.print(soc * BATTERY_CAPACITY_AH, 2); Serial.println(" Ah");
    Serial.print("Ah (total)      : "); Serial.print(ah.getTotalAh(), 3); Serial.println(" Ah");
    Serial.print("Charge totale   : "); Serial.print(ah.getTotalCharged(), 2); Serial.println(" Ah");
    Serial.print("Décharge totale : "); Serial.print(ah.getTotalDischarged(), 2); Serial.println(" Ah");
    Serial.print("Cycles équiv.   : "); Serial.println(ah.getTotalDischarged() / BATTERY_CAPACITY_AH, 2);

    if (ah.isCharging()) Serial.println("Mode : CHARGE");
    else if (ah.isDischarging()) Serial.println("Mode : DÉCHARGE");
    else Serial.println("Mode : REPOS");

    if (soc <= SEUIL_BATTERIE_BASSE) Serial.println("ATTENTION : BATTERIE FAIBLE !");
    lastPrint = now;
  }

  // Sauvegarde périodique via Persistance
  if (now - lastSave >= SAVE_INTERVAL_MS) {
    persistance.saveFloat("ah", ah.getTotalAh());
    Serial.println("\nAh sauvegardés via Persistance.");
    lastSave = now;
  }
}
