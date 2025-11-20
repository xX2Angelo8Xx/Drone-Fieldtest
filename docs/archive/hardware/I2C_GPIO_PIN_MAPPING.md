> Note (archived): Superseded by `docs/guides/hardware/HARDWARE_INTEGRATION_GUIDE.md`. See also `docs/KEY_LEARNINGS.md` for critical rules. This file is preserved for history.

# I2C GPIO Pin Mapping - Jetson Orin Nano

**Date:** November 19, 2025  
**Platform:** Jetson Orin Nano Developer Kit

## 📍 Physical Pin Mapping

### I2C_IDA_0 (Bus 0 - `/dev/i2c-0`)
**40-Pin Header:** NOT EXPOSED (Internal only)  
**Hardware Controller:** `0x3160000.i2c`  
**JST Connector:** I2C_IDA_0 (4-pin, separate connector)  
**Used by:** System EEPROM (0x50, 0x57)

```
I2C_IDA_0 JST Connector:
┌─────────────────────┐
│  1   2   3   4      │
│ VCC SDA SCL GND     │
└─────────────────────┘
Internal I2C bus for system components
```

### I2C_IDA_1 (Bus 7 - `/dev/i2c-7`)
**40-Pin Header:** Pin 3 (SDA), Pin 5 (SCL)  
**Hardware Controller:** `0xc250000.i2c`  
**JST Connector:** I2C_IDA_1 (4-pin, separate connector)  
**Used by:** External devices (LCD, sensors, etc.)

```
I2C_IDA_1 on 40-Pin Header:
┌─────────────────────┐
│ Pin 1:  3.3V Power  │
│ Pin 2:  5V Power    │
│ Pin 3:  SDA_1 ←───  │ ← I2C Data Line
│ Pin 4:  5V Power    │
│ Pin 5:  SCL_1 ←───  │ ← I2C Clock Line
│ Pin 6:  GND         │
└─────────────────────┘

I2C_IDA_1 JST Connector (alternative):
┌─────────────────────┐
│  1   2   3   4      │
│ VCC SDA SCL GND     │
└─────────────────────┘
```

## 🔧 Pin Multiplexing (Pinmux) System

### Was ist Pinmux?

Auf dem Jetson können die meisten GPIO-Pins **mehrere Funktionen** haben:
- GPIO (General Purpose Input/Output)
- I2C (SDA/SCL)
- UART (TX/RX)
- SPI (MOSI/MISO/CLK)
- PWM
- etc.

Die **Device Tree** Konfiguration legt fest, welche Funktion aktiv ist.

### Aktuelle Konfiguration (Standard)

```
Pin 27 → I2C0_SDA (I2C Funktion)
Pin 28 → I2C0_SCL (I2C Funktion)
Pin 29 → I2C1_SDA (I2C Funktion)
Pin 30 → I2C1_SCL (I2C Funktion)
```

### Automatische Bus-Zuordnung

**Ja, die Zuordnung ist automatisch:**

Wenn du ein I2C-Gerät an den I2C_IDA_0 JST-Connector anschließt:
1. Hardware-Verbindung geht zu GPIO Pin 27/28
2. Pinmux leitet diese zu I2C Controller 0
3. Software greift über `/dev/i2c-0` (Bus 0) darauf zu

**Du musst nichts konfigurieren** - die Zuordnung ist fest im Device Tree.

## ⚙️ Kann man die Pin-Zuordnung ändern?

### Option 1: Device Tree Modification (KOMPLIZIERT, NICHT EMPFOHLEN)

**Theoretisch JA, aber:**

```bash
# 1. Device Tree Source bearbeiten
sudo nano /boot/tegra234-p3767-0000-p3509-0000.dts  # Oder ähnlich

# 2. Pinmux-Konfiguration ändern
# Beispiel (vereinfacht):
pinmux@2430000 {
    i2c0_pins {
        nvidia,pins = "gpio27", "gpio28";
        nvidia,function = "i2c0";  # <- Könnte zu "gpio" geändert werden
        nvidia,pull = <TEGRA_PIN_PULL_NONE>;
    };
};

# 3. Device Tree kompilieren
dtc -I dts -O dtb -o custom.dtb tegra234-xxx.dts

# 4. Bootloader-Konfiguration ändern
# 5. System neu flashen oder DTB ersetzen
# 6. Risiko: System bootet nicht mehr!
```

**WARUM NICHT EMPFOHLEN:**
- ❌ Komplexe Änderungen am Boot-System
- ❌ Risiko: System startet nicht mehr
- ❌ Verlust der Herstellergarantie möglicherweise
- ❌ Schwer zu debuggen bei Problemen
- ❌ Updates überschreiben Änderungen

### Option 2: Jetson-IO Tool (EINFACHER, aber begrenzt)

NVIDIA bietet ein Tool zur Pin-Konfiguration:

```bash
# Jetson-IO starten (grafisches Tool)
sudo /opt/nvidia/jetson-io/jetson-io.py
```

**Was es kann:**
- ✅ Aktivieren/Deaktivieren von I2C-Controllern
- ✅ Pin-Funktionen umschalten (wenn Alternativen verfügbar)
- ✅ Änderungen werden in DTB gespeichert

**Limitierungen:**
- ⚠️ Nur vordefinierte Konfigurationen
- ⚠️ Nicht alle Pins sind konfigurierbar
- ⚠️ I2C-Pins sind meist fest zugeordnet

### Option 3: Software I2C (Bit-Banging) - WORKAROUND

Wenn Hardware-I2C auf Bus 0 defekt ist, kannst du **Software-I2C** nutzen:

```bash
# 1. i2c-gpio Kernel-Modul laden
sudo modprobe i2c-gpio

# 2. Device Tree Overlay erstellen (vereinfacht)
# Definiert neue I2C-Bus auf beliebigen GPIO-Pins
# Beispiel: GPIO 17 (SDA), GPIO 18 (SCL) → neuer Bus 10
```

**Nachteile:**
- 🐌 Sehr langsam (~10kHz vs 100kHz Hardware-I2C)
- 💻 Höhere CPU-Last
- ⚠️ Nicht für zeitkritische Anwendungen

## 🎯 Praktische Lösung für dein Problem

### Situation:
- Bus 0 (GPIO 27/28) funktioniert möglicherweise nicht
- Bus 7 (GPIO 29/30) funktioniert einwandfrei

### ✅ EMPFOHLENE LÖSUNG: Beide Geräte auf Bus 7

**Keine Pin-Änderungen nötig!**

```
Bus 7 (I2C_IDA_1 - GPIO 29/30):
  ├─ LCD Display @ 0x27
  └─ INA219 @ 0x40
```

**Warum das die beste Lösung ist:**
- ✅ Keine Device Tree Änderungen
- ✅ Keine System-Risiken
- ✅ Funktioniert sofort
- ✅ Unterschiedliche Adressen = kein Konflikt
- ✅ Einfache Verkabelung
- ✅ Bewährt stabil

**Code-Beispiel:**
```cpp
// Beide Geräte auf Bus 7 (I2C_IDA_1)
I2C_LCD lcd(7, 0x27);           // LCD auf Bus 7, Adresse 0x27
INA219 power_monitor(7, 0x40);  // INA219 auf Bus 7, Adresse 0x40

// Beide funktionieren parallel - kein Problem!
lcd.print("Voltage:");
float voltage = power_monitor.getBusVoltage();
lcd.print(voltage);
```

## 🔍 Verifizierung der Pin-Konfiguration

### Aktuellen Pinmux-Status prüfen:

```bash
# 1. Device Tree Blob dekompilieren
sudo dtc -I fs -O dts /proc/device-tree > current_dt.dts

# 2. I2C-Konfiguration suchen
grep -A 10 "i2c@3160000" current_dt.dts  # Bus 0
grep -A 10 "i2c@c250000" current_dt.dts  # Bus 7

# 3. Pin-Status in sysfs prüfen
cat /sys/kernel/debug/pinctrl/2430000.pinmux/pinmux-pins | grep -i i2c
```

### GPIO-Funktion überprüfen:

```bash
# Zeigt alle GPIO-Pins und ihre Funktionen
sudo cat /sys/kernel/debug/gpio

# Suche nach I2C-Pins
sudo cat /sys/kernel/debug/gpio | grep -i "i2c\|27\|28\|29\|30"
```

### I2C-Controller Status:

```bash
# Liste aller I2C-Adapter
ls -la /sys/class/i2c-adapter/

# Detaillierte Info zu Bus 0
cat /sys/class/i2c-adapter/i2c-0/name
cat /sys/class/i2c-adapter/i2c-0/device/modalias

# Device Tree Status für I2C0
cat /proc/device-tree/i2c@3160000/status
# Sollte "okay" ausgeben, wenn aktiviert
```

## 📋 Zusammenfassung: Deine Fragen beantwortet

### Frage 1: "Pins 27/28 sind automatisch Bus 0?"
**Antwort:** ✅ **JA**, die Zuordnung ist fest im Device Tree:
- GPIO Pin 27/28 → I2C Controller 0 → `/dev/i2c-0` (Bus 0)
- GPIO Pin 29/30 → I2C Controller 1 → `/dev/i2c-7` (Bus 7)

Wenn du ein Gerät an I2C_IDA_0 JST-Connector anschließt, kommuniziert es automatisch über Bus 0.

### Frage 2: "Kann ich das ändern?"
**Antwort:** ⚠️ **THEORETISCH JA, PRAKTISCH NICHT EMPFOHLEN**

**Änderung möglich durch:**
1. Device Tree Modifikation (komplex, riskant)
2. Jetson-IO Tool (begrenzte Optionen)
3. Software-I2C auf anderen Pins (langsam)

**ABER für dein Projekt:**
- ❌ Unnötig kompliziert
- ❌ Risiko von System-Instabilität
- ✅ **BESSER:** Beide Geräte auf Bus 7 nutzen (funktioniert perfekt!)

## 🔗 Weiterführende Ressourcen

- [Jetson Orin Nano GPIO Header Pinout](https://developer.nvidia.com/embedded/downloads)
- [Device Tree Documentation](https://www.kernel.org/doc/Documentation/devicetree/)
- [Jetson Linux Driver Package Release Notes](https://developer.nvidia.com/embedded/jetson-linux)

## ⚡ Quick Reference Commands

```bash
# Welche I2C-Buses sind verfügbar?
ls /dev/i2c-*

# Welche Hardware-Adressen haben die Buses?
ls -la /sys/class/i2c-adapter/

# Scan Bus 0 (GPIO 27/28)
sudo i2cdetect -y 0

# Scan Bus 7 (GPIO 29/30)
sudo i2cdetect -y 7

# Prüfe Device Tree I2C Status
cat /proc/device-tree/i2c@3160000/status  # Bus 0
cat /proc/device-tree/i2c@c250000/status  # Bus 7
```

---

**Bottom Line für dein Projekt:**
Die Pin-Zuordnung ist fest und funktioniert automatisch. Du musst **nichts konfigurieren**. Wenn Bus 0 (Pins 27/28) nicht funktioniert, nutze einfach beide Geräte auf Bus 7 (Pins 29/30) - das ist die sicherste und einfachste Lösung! 🎯
