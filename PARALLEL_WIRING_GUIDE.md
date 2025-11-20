# Parallel I2C Wiring Guide - Basierend auf Jetson Orin Nano Expansion Header

## 📍 Expansion Header Pinout (von deinem Bild)

```
┌────────────────────────────────────────┐
│        Jetson Orin Nano 40-Pin         │
│         Expansion Header               │
├────────────┬─────┬─────┬──────────────┤
│  Function  │ Pin │ Pin │  Function    │
├────────────┼─────┼─────┼──────────────┤
│   3.3V     │  1  │  2  │  5.0V        │
│ I2C1_SDA ← │  3  │  4  │  5.0V        │  ← Bus 7 Data
│ I2C1_SCL ← │  5  │  6  │  GND         │  ← Bus 7 Clock
│ GPIO09     │  7  │  8  │ UART1_TXD    │
│   GND      │  9  │ 10  │ UART1_RXD    │
│ UART1_RTS* │ 11  │ 12  │ I2S0_SCLK    │
│ SPI1_SCK   │ 13  │ 14  │  GND         │
│ GPIO12     │ 15  │ 16  │ SPI1_CS1*    │
│   3.3V     │ 17  │ 18  │ SPI1_CS0*    │
│ SPI0_MOSI  │ 19  │ 20  │  GND         │
│ SPI0_MISO  │ 21  │ 22  │ SPI1_MISO    │
│ SPI0_SCK   │ 23  │ 24  │ SPI0_CS0*    │
│   GND      │ 25  │ 26  │ SPI0_CS1*    │
│ I2C0_SDA   │ 27  │ 28  │ I2C0_SCL     │
│ GPIO01     │ 29  │ 30  │  GND         │
│ GPIO11     │ 31  │ 32  │ GPIO07       │
│ GPIO13     │ 33  │ 34  │  GND         │
│ I2S0_FS    │ 35  │ 36  │ UART1_CTS*   │
│ SPI1_MOSI  │ 37  │ 38  │ I2S0_DIN     │
│   GND      │ 39  │ 40  │ I2S0_DOUT    │
└────────────┴─────┴─────┴──────────────┘
```

## 🔌 WIRING FÜR LCD + INA219 PARALLEL

### Methode 1: Breadboard (Empfohlen für Prototyping)

```
Jetson Expansion Header          Breadboard              Devices
═══════════════════════          ══════════              ═══════

Pin 1 (3.3V) ──────────→ + Rail ─┬─→ LCD VCC
                                   └─→ INA219 VCC

Pin 3 (I2C1_SDA) ──────→ Signal  ─┬─→ LCD SDA
                                   └─→ INA219 SDA

Pin 5 (I2C1_SCL) ──────→ Signal  ─┬─→ LCD SCL
                                   └─→ INA219 SCL

Pin 9 (GND) ───────────→ - Rail ─┬─→ LCD GND
                                   └─→ INA219 GND
```

### Methode 2: Direkt Löten/Verdrillen

```
Von Jetson:                    Zu Geräten:
═══════════                    ═══════════

Pin 1 (3.3V) ─────┬────────→ LCD VCC (Rot)
                  └────────→ INA219 VCC (Rot)
                  (Kabel zusammen löten/verdrillen)

Pin 3 (SDA) ──────┬────────→ LCD SDA (Gelb/Weiß)
                  └────────→ INA219 SDA (Gelb/Weiß)
                  (Kabel zusammen löten/Verdrillen)

Pin 5 (SCL) ──────┬────────→ LCD SCL (Grün/Blau)
                  └────────→ INA219 SCL (Grün/Blau)
                  (Kabel zusammen löten/Verdrillen)

Pin 9 (GND) ──────┬────────→ LCD GND (Schwarz)
                  └────────→ INA219 GND (Schwarz)
                  (Kabel zusammen löten/Verdrillen)
```

### Methode 3: Y-Kabel / Splitter

```
Jetson ──→ Y-Splitter ──┬──→ LCD (4-pin JST)
                         └──→ INA219 (4-pin Dupont)
```

## 📊 LCD Adresse finden

**Typische LCD I2C-Backpack Adressen:**

| Backpack Typ | Standard-Adresse | Alternative |
|--------------|------------------|-------------|
| PCF8574 (häufigste) | **0x27** | 0x3F |
| PCF8574A | 0x3F | 0x27 |

**So findest du deine LCD Adresse:**

```bash
# 1. Nur LCD an Pin 3/5 anschließen
# 2. Scannen:
sudo i2cdetect -y 7

# Erwartete Ausgabe (Beispiel):
#      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
# 20: -- -- -- -- -- -- -- 27 -- -- -- -- -- -- -- --
#                          ↑
#                     Deine LCD Adresse!

# Oder bei manchen LCDs:
# 30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 3F
#                                                   ↑
```

**Adresse vom LCD-Backpack ablesen:**

Auf dem I2C-Backpack des LCDs ist manchmal ein kleiner Chip mit Beschriftung:
- **PCF8574** → Meist 0x27
- **PCF8574A** → Meist 0x3F

Oder es gibt **Jumper** auf dem Backpack (A0, A1, A2):
```
Keine Jumper → 0x27
A0 geschlossen → 0x26
A1 geschlossen → 0x25
A0+A1+A2 geschlossen → 0x20
```

## ✅ Schritt-für-Schritt Anleitung

### Schritt 1: LCD anschließen (alleine)

```bash
# Verbindung:
# LCD GND → Pin 9 (GND)
# LCD VCC → Pin 1 (3.3V)  ⚠️ WICHTIG: 3.3V, nicht 5V!
# LCD SDA → Pin 3 (I2C1_SDA)
# LCD SCL → Pin 5 (I2C1_SCL)

# Testen:
sudo i2cdetect -y 7
```

**Erwartetes Ergebnis:**
```
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
20: -- -- -- -- -- -- -- 27 -- -- -- -- -- -- -- --
```
✅ LCD gefunden bei **0x27** (oder 0x3F)

### Schritt 2: INA219 hinzufügen (parallel)

```bash
# INA219 an DIESELBEN Pins anschließen:
# INA219 GND → Pin 9 (oder mit LCD GND zusammen)
# INA219 VCC → Pin 1 (oder mit LCD VCC zusammen)
# INA219 SDA → Pin 3 (oder mit LCD SDA zusammen)
# INA219 SCL → Pin 5 (oder mit LCD SCL zusammen)
#
# Plus für Spannungsmessung:
# INA219 VIN+ → Battery + (zu messende Spannung)
# INA219 VIN- → Battery - (zu messende Spannung)

# Testen:
sudo i2cdetect -y 7
```

**Erwartetes Ergebnis:**
```
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
20: -- -- -- -- -- -- -- 27 -- -- -- -- -- -- -- --
40: 40 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
```
✅ **BEIDE sichtbar!** LCD bei 0x27, INA219 bei 0x40

### Schritt 3: Im Code verwenden

```cpp
// drone_web_controller.cpp
#include "common/hardware/i2c_lcd/lcd.h"
#include "ina219_library.h"

// Beide auf Bus 7
I2C_LCD lcd(7, 0x27);           // Oder 0x3F je nach deinem LCD
INA219 power_monitor(7, 0x40);

// Initialisieren
lcd.init();
lcd.backlight();
power_monitor.begin();

// Gleichzeitig verwenden - kein Problem!
while (true) {
    float voltage = power_monitor.getBusVoltage();
    
    lcd.clear();
    lcd.print("Battery:");
    lcd.setCursor(0, 1);
    lcd.print(voltage);
    lcd.print("V");
    
    sleep(1);
}
```

## ⚠️ Wichtige Hinweise

### Spannung: 3.3V oder 5V?

**LCD:**
- Die meisten 16x2 LCD Module sind **5V Logik**
- ABER: Das **I2C Backpack** (PCF8574) hat oft **5V Toleranz**
- Viele funktionieren auch mit 3.3V (mit reduzierter Helligkeit)
- **ACHTUNG:** Jetson I2C ist **3.3V ONLY!**

**INA219:**
- **3.3V Logik** - perfekt für Jetson!
- VCC = 3.3V
- VIN+ kann bis 26V messen (z.B. 4S LiPo)

**KRITISCH:** 
- ❌ **NIEMALS 5V auf I2C1_SDA/SCL Pins** → Jetson Schaden!
- ✅ **Nur 3.3V auf Pin 1 verwenden**

Wenn dein LCD **zwingend 5V** braucht:
```
Option 1: 5V Backpack mit Level-Shifter (TXS0108)
Option 2: 3.3V-kompatibler LCD Backpack kaufen
```

### Pull-up Widerstände

**Bereits vorhanden auf:**
- LCD I2C Backpack (meist 4.7kΩ)
- INA219 Module (meist 10kΩ)

→ **Keine zusätzlichen Pull-ups nötig!**

Bei Problemen (Bus instabil):
- Externe 2.2kΩ Pull-ups von SDA/SCL zu 3.3V hinzufügen

## 🧪 Test-Script verwenden

```bash
# Interaktives Test-Script
sudo ./test_parallel_connection.sh
```

Dieses Script führt dich Schritt-für-Schritt durch:
1. LCD anschließen → Adresse finden
2. INA219 parallel hinzufügen → Beide gleichzeitig scannen
3. Bestätigung dass beide funktionieren

## 🎯 Zusammenfassung

**Deine Frage:** "Kann ich SDA/SCL zusammenlöten und auf Pin 3/5 hängen?"

**Antwort:** ✅ **JA, ABSOLUT!**

```
Das IST der Standard-Weg für I2C!

Pin 3 (SDA) ──┬── LCD SDA
              └── INA219 SDA    } Zusammen löten/verdrillen

Pin 5 (SCL) ──┬── LCD SCL
              └── INA219 SCL    } Zusammen löten/verdrillen

Pin 1 (3.3V) ─┬── LCD VCC
              └── INA219 VCC    } Zusammen löten/verdrillen

Pin 9 (GND) ──┬── LCD GND
              └── INA219 GND    } Zusammen löten/verdrillen
```

**Warum funktioniert das?**
- I2C ist ein **Shared Bus Protocol**
- Unterschiedliche Adressen (0x27 vs 0x40) = Kein Konflikt
- Bis zu 127 Geräte auf einem Bus möglich!
- Das ist **STANDARD** - Millionen von Geräten weltweit nutzen das so

**Deine LCD Adresse:**
- Wahrscheinlich **0x27** oder **0x3F**
- Finde es mit: `sudo i2cdetect -y 7` (nach Anschluss)
- Steht manchmal auf dem I2C-Backpack Chip

Viel Erfolg! 🚀
