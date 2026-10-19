/**
 * @file main.cpp
 * @brief Exemple d'utilisation de la librairie CoulombsAh.h + Persistance
 *
 * Compteur de charge FUSIONNEL : coulombs et ampères-heures sont la même
 * grandeur à un facteur 3600 près (1 Ah = 3600 C). Un seul accumulateur
 * `Ah_GD` les porte, avec une précision **exacte** quelle que soit la
 * magnitude du total.
 *
 * Ce que cet exemple démontre :
 *   1. l'intégration : `addCoulombs(I × dt)` à chaque échantillon ;
 *   2. les compteurs BRUTS chargé / déchargé (`getAhCharged` / `getAhDischarged`)
 *      et le net (`getAhNet`) ;
 *   3. les restes fractionnaires (`getAhFractionChg` / `getAhFractionDis`) ;
 *   4. ⚠️ la lecture EXACTE d'un delta via `getNetMicroCoulombs()` (int64) —
 *      comparée à la lecture en `float32` qui, elle, perd les petits incréments ;
 *   5. la persistance : `serialize()` produit 4 × uint32, que l'on confie à
 *      Fo170/Persistance (EEPROM, FRAM ou LittleFS).
 *
 * ⚠️ POURQUOI LE DELTA SE LIT EN int64
 * Soustraire deux `getAhNet()` en float32 perd le résultat : à 910 Ah l'ULP
 * d'un float32 vaut 6,1e-05 Ah, alors qu'un échantillon de 0,1 s à 0,5 A pèse
 * 1,4e-05 Ah. La différence est arrondie à zéro et le compteur se fige.
 * La bonne façon est de prendre la différence en micro-coulombs ENTIERS, puis
 * de diviser une seule fois.
 *
 * Matériel :
 * - ESP8266 (WeMos D1 Mini / Lolin)
 *
 * Version : 3.1.0
 */

#include <Arduino.h>
#include "CoulombsAh.h"
#include <persistance_EEPROM.h>

// ==================== CONFIGURATION ====================
#define EEPROM_SIZE          512    // Taille EEPROM utilisée
#define PRINT_INTERVAL_MS    1000   // Affichage toutes les 1 s
#define SAVE_INTERVAL_MS     60000  // Sauvegarde toutes les 60 s

// ==================== OBJETS GLOBAUX ====================
Ah_GD ah;
PersistanceEEPROM persistance(EEPROM_SIZE);

// Dernière valeur connue en micro-coulombs (int64) : c'est la référence pour
// calculer un delta EXACT, contrairement à une référence en float32.
int64_t lastUc = 0;
uint32_t lastMs = 0;

// ==================== MESURE DU COURANT ====================

/**
 * @brief Mesure le courant traversant la batterie en Ampères.
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

// ==================== PERSISTANCE ====================
// L'accumulateur se sérialise en 4 × uint32 : ahCharged, ahDischarged, puis
// les deux restes en micro-coulombs. On persiste le COUPLE (entiers + restes) :
// sans les restes, un redémarrage perdrait jusqu'à 1 Ah de comptage.

void loadState() {
  uint32_t chg = 0, dis = 0, ucChg = 0, ucDis = 0;
  bool ok = persistance.loadUint32("ah_chg", &chg);
  ok &= persistance.loadUint32("ah_dis", &dis);
  ok &= persistance.loadUint32("ah_ucc", &ucChg);
  ok &= persistance.loadUint32("ah_ucd", &ucDis);

  if (ok) {
    ah.deserialize(chg, dis, ucChg, ucDis);
    Serial.print("État restauré : ");
    Serial.print(ah.getAhCharged(), 3); Serial.print(" Ah chargés, ");
    Serial.print(ah.getAhDischarged(), 3); Serial.println(" Ah déchargés");
  } else {
    Serial.println("Premier démarrage : compteur à 0");
  }
  lastUc = ah.getNetMicroCoulombs();
}

void saveState() {
  uint32_t chg, dis, ucChg, ucDis;
  ah.serialize(chg, dis, ucChg, ucDis);
  persistance.saveUint32("ah_chg", chg);
  persistance.saveUint32("ah_dis", dis);
  persistance.saveUint32("ah_ucc", ucChg);
  persistance.saveUint32("ah_ucd", ucDis);
  Serial.println("État sauvegardé via Persistance.");
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=== EXEMPLE CoulombsAh.h + Persistance ===");

  if (!persistance.begin()) {
    Serial.println("Échec EEPROM.begin");
    return;
  }
  persistance.addRegister("ah_chg", sizeof(uint32_t));
  persistance.addRegister("ah_dis", sizeof(uint32_t));
  persistance.addRegister("ah_ucc", sizeof(uint32_t));
  persistance.addRegister("ah_ucd", sizeof(uint32_t));

  loadState();
  lastMs = millis();
}

// ==================== BOUCLE PRINCIPALE ====================
void loop() {
  // ── 1. INTÉGRATION ───────────────────────────────────────
  // Le compteur n'a PAS d'horloge interne : c'est ici que l'on mesure le pas
  // de temps réel. Un `dt` fixe sous-compterait dès que la boucle est retardée.
  uint32_t now = millis();
  float dt = (now - lastMs) * 0.001f;
  lastMs = now;
  if (dt <= 0.0f || dt > 0.5f) dt = 0.0f;   // garde anti-stall

  float I = readCurrent();
  if (dt > 0.0f) ah.addCoulombs(I * dt);    // ΔC = I × Δt

  // ── 2. AFFICHAGE PÉRIODIQUE ──────────────────────────────
  static uint32_t lastPrint = 0;
  static uint32_t lastSave = 0;

  if (now - lastPrint >= PRINT_INTERVAL_MS) {
    lastPrint = now;

    // Delta EXACT : différence en int64 µC, puis division unique.
    int64_t nowUc = ah.getNetMicroCoulombs();
    int64_t dUc = nowUc - lastUc;
    lastUc = nowUc;

    // Le MÊME delta lu en float32, pour comparaison (à ne pas faire !).
    static float lastAhF = 0.0f;
    float dAhFloat = ah.getAhNet() - lastAhF;
    lastAhF = ah.getAhNet();

    Serial.println("----------------------------");
    Serial.print("Courant                : "); Serial.print(I, 3); Serial.println(" A");
    Serial.print("Ah chargés (BRUT)      : "); Serial.print(ah.getAhCharged(), 6); Serial.println(" Ah");
    Serial.print("Ah déchargés (BRUT)    : "); Serial.print(ah.getAhDischarged(), 6); Serial.println(" Ah");
    Serial.print("Ah net                 : "); Serial.print(ah.getAhNet(), 6); Serial.println(" Ah");
    Serial.print("Net (exact)            : "); Serial.print((double)nowUc / 3.6e9, 6); Serial.println(" Ah");
    Serial.print("Restes                 : ");
    Serial.print(ah.getAhFractionChg(), 6); Serial.print(" / ");
    Serial.print(ah.getAhFractionDis(), 6); Serial.println(" Ah");

    // Les deux façons de lire le delta : la première est exacte.
    Serial.print("Delta (int64 uC)       : ");
    Serial.print((double)dUc / 3.6e9, 9); Serial.println(" Ah  <- EXACT");
    Serial.print("Delta (float32)        : ");
    Serial.print(dAhFloat, 9); Serial.println(" Ah  <- perd les petits pas");

    if (!ah.isEmpty()) Serial.println("(compteur non vide)");
    Serial.println();
  }

  // ── 3. SAUVEGARDE PÉRIODIQUE ─────────────────────────────
  if (now - lastSave >= SAVE_INTERVAL_MS) {
    lastSave = now;
    saveState();
  }
}
