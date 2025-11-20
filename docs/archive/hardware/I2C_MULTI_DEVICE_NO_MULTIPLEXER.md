> Note (archived): Consolidated. See `docs/guides/hardware/HARDWARE_INTEGRATION_GUIDE.md` for the canonical I2C guidance. See `docs/KEY_LEARNINGS.md` for rules of thumb.

# I2C Multi-Device Configuration - KEIN Multiplexer Nötig!

## 🎯 WICHTIG: I2C ist ein Shared Bus!

### Missverständnis aufgeklärt

**FALSCH:** "Ich brauche einen Multiplexer für 2 Geräte auf einem Bus"  
**RICHTIG:** I2C unterstützt **bis zu 127 Geräte** auf **einem einzigen Bus**!

### Wie I2C funktioniert

```
I2C Bus Topologie (Bus 7 Beispiel):

     3.3V
      │
    ┌─┴─┬─────┐ Pull-up Resistoren (2.2kΩ - 10kΩ)
    │   │     │
  ──┴───┴─────┴──── SDA (Data Line) - Pin 3
                    
  ─────────────────  SCL (Clock Line) - Pin 5
    │   │     │
   LCD INA  [More devices possible]
  0x27 0x40  0x??

Jedes Gerät hat eigene Adresse → Kein Konflikt!
```

### Adress-Zuordnung

| Gerät | Standard-Adresse | Alternative Adressen |
|-------|------------------|----------------------|
| **LCD 16x2 (PCF8574)** | 0x27 | 0x3F (via Jumper) |
| **INA219 Power Monitor** | 0x40 | 0x41, 0x44, 0x45 (via Jumper) |

**Keine Überlappung!** → Beide können parallel auf Bus 7 laufen.

## 🔌 Praktische Verkabelung - Parallel Connection

### Schema: Beide Geräte auf Bus 7

```
Jetson Orin Nano (40-Pin Header)
┌────────────────────┐
│ Pin 1:  3.3V       │───┬────→ LCD VCC
│ Pin 3:  I2C_SDA_1  │───┼────→ LCD SDA
│ Pin 5:  I2C_SCL_1  │───┼────→ LCD SCL
│ Pin 6:  GND        │───┼────→ LCD GND
│                    │   │
│ (same pins!)       │   │
│                    │   │
│ Pin 1:  3.3V       │───┴────→ INA219 VCC
│ Pin 3:  I2C_SDA_1  │────────→ INA219 SDA
│ Pin 5:  I2C_SCL_1  │────────→ INA219 SCL
│ Pin 6:  GND        │────────→ INA219 GND
└────────────────────┘

Beide teilen sich dieselben 4 Pins!
SDA/SCL sind parallele Verbindungen (Bus)
```

### Praktische Verkabelung mit Breadboard

```
Jetson Pin 3 (SDA) ─────┬───→ LCD SDA
                         └───→ INA219 SDA

Jetson Pin 5 (SCL) ─────┬───→ LCD SCL
                         └───→ INA219 SCL

Jetson Pin 1 (3.3V) ────┬───→ LCD VCC
                         └───→ INA219 VCC

Jetson Pin 6 (GND) ─────┬───→ LCD GND
                         └───→ INA219 GND
```

**ODER** mit JST-Splitter Kabel:
```
I2C_IDA_1 Connector ──→ Y-Splitter ──┬──→ LCD
                                       └──→ INA219
```

## 🛡️ Bus 0 EEPROM Schutz - Deine wichtige Frage!

### Frage: "Kann ich verhindern, dass externe Geräte Bus 0 nutzen?"

**Antwort:** ⚠️ **Software-Schutz JA, Hardware-Schutz SCHWIERIG**

### Option 1: Device Tree Deaktivierung (Empfohlen)

Bus 0 im Device Tree als "disabled" markieren:

```bash
# THEORIE (vereinfacht):
# In Device Tree Source (.dts):
i2c@3160000 {
    status = "disabled";  # Bus 0 wird nicht initialisiert
    # EEPROM ist intern, braucht keine User-Space-Zugriff
};
```

**Effekt:**
- ❌ Kein `/dev/i2c-0` Device Node erstellt
- ✅ EEPROM weiterhin funktionsfähig (Hardware-Ebene)
- ✅ User-Space kann nicht auf Bus 0 zugreifen
- ✅ Schutz vor versehentlichem Zugriff

**Problem:**
- ⚠️ Erfordert Custom Device Tree (komplex)
- ⚠️ Jetson-Updates können Änderung überschreiben

### Option 2: Permissions Lock (Einfacher)

```bash
# /dev/i2c-0 nur für root lesbar machen
sudo chmod 600 /dev/i2c-0
sudo chown root:root /dev/i2c-0

# Prüfen:
ls -la /dev/i2c-0
# Sollte zeigen: crw------- 1 root root
```

**Vorteil:**
- ✅ Sofort wirksam
- ✅ Kein Device Tree Ändern
- ✅ Schützt vor User-Space Zugriff

**Nachteil:**
- ⚠️ Wird bei Reboot zurückgesetzt
- ⚠️ Root kann immer noch zugreifen

### Option 3: udev Rule (Persistent)

```bash
# Erstelle /etc/udev/rules.d/99-i2c-lock.rules
sudo nano /etc/udev/rules.d/99-i2c-lock.rules

# Inhalt:
KERNEL=="i2c-0", MODE="0600", OWNER="root", GROUP="root"
KERNEL=="i2c-7", MODE="0666", OWNER="root", GROUP="i2c"

# Regel aktivieren:
sudo udevadm control --reload-rules
sudo udevadm trigger
```

**Effekt:**
- ✅ Bus 0: Nur root (0600)
- ✅ Bus 7: Alle i2c-Gruppe Mitglieder (0666)
- ✅ Persistent nach Reboot
- ✅ EEPROM geschützt

### Option 4: Physischer Schutz (Hardware)

**Problem:** I2C_IDA_0 JST-Connector ist **direkt mit Bus 0** verbunden!

**Hardware-Lösung:**
1. **Connector entfernen** (drastisch!)
2. **Epoxy versiegeln** (permanent)
3. **Isolierband + Warnung-Label** (einfach)

**Realität:** Nicht praktikabel für Development-Board.

### ⚠️ KRITISCHE ERKENNTNIS aus deinem Scan:

```
Bus 0: UU bei 0x50 und 0x57
```

**Das bedeutet:**
- EEPROM ist **bereits durch Kernel-Treiber geschützt** (`UU` = Kernel claimed)
- User-Space kann nicht auf 0x50/0x57 schreiben (Kernel blockiert)
- **Gefahr ist minimal!**

**Risiko-Bewertung:**
- ✅ EEPROM-Adressen sind kernel-protected
- ⚠️ Andere Adressen auf Bus 0 könnten Störsignale verursachen
- ⚠️ Falsche Verkabelung könnte Hardware beschädigen (Kurzschluss)

## 🔧 Software I2C über GPIO Pins - Komplexität

### Frage: "Wie schwierig ist Software I2C über GPIO?"

**Antwort:** ⚠️ **Machbar, aber mit Einschränkungen**

### Option A: Kernel i2c-gpio Modul (Empfohlen)

**Schwierigkeit:** 🟡 Mittel (Device Tree Overlay nötig)

```bash
# 1. Device Tree Overlay erstellen
# Datei: i2c-gpio-overlay.dts

/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target-path = "/";
        __overlay__ {
            i2c-gpio {
                compatible = "i2c-gpio";
                gpios = <&gpio TEGRA234_GPIO(X, Y) GPIO_ACTIVE_HIGH>, /* SDA */
                        <&gpio TEGRA234_GPIO(X, Z) GPIO_ACTIVE_HIGH>; /* SCL */
                i2c-gpio,delay-us = <5>;  /* 100 kHz */
                #address-cells = <1>;
                #size-cells = <0>;
            };
        };
    };
};

# 2. Kompilieren
dtc -@ -I dts -O dtb -o i2c-gpio-overlay.dtbo i2c-gpio-overlay.dts

# 3. Laden
sudo mkdir -p /boot/overlays
sudo cp i2c-gpio-overlay.dtbo /boot/overlays/
# Boot config anpassen

# 4. Ergebnis: Neuer Bus (z.B. /dev/i2c-10)
```

**Performance:**
- Speed: ~50-100 kHz (vs 400 kHz Hardware I2C)
- CPU: ~5-10% Overhead pro Transfer
- Latenz: 2-3x höher

### Option B: Python RPi.GPIO Bit-Banging

**Schwierigkeit:** 🟢 Einfach (für Prototyping)

```python
#!/usr/bin/env python3
"""
Software I2C Implementation using Jetson GPIO
WARNING: Slow and CPU-intensive!
"""

import Jetson.GPIO as GPIO
import time

SDA_PIN = 15  # Beispiel GPIO Pin
SCL_PIN = 16  # Beispiel GPIO Pin

class SoftwareI2C:
    def __init__(self, sda_pin, scl_pin):
        self.sda = sda_pin
        self.scl = scl_pin
        
        GPIO.setmode(GPIO.BOARD)
        GPIO.setup(self.sda, GPIO.OUT)
        GPIO.setup(self.scl, GPIO.OUT)
        GPIO.output(self.sda, GPIO.HIGH)
        GPIO.output(self.scl, GPIO.HIGH)
    
    def start_condition(self):
        """I2C Start: SDA falls while SCL high"""
        GPIO.output(self.sda, GPIO.HIGH)
        GPIO.output(self.scl, GPIO.HIGH)
        time.sleep(0.00001)  # 10µs delay
        GPIO.output(self.sda, GPIO.LOW)
        time.sleep(0.00001)
        GPIO.output(self.scl, GPIO.LOW)
    
    def stop_condition(self):
        """I2C Stop: SDA rises while SCL high"""
        GPIO.output(self.sda, GPIO.LOW)
        GPIO.output(self.scl, GPIO.HIGH)
        time.sleep(0.00001)
        GPIO.output(self.sda, GPIO.HIGH)
    
    def write_bit(self, bit):
        """Write single bit"""
        GPIO.output(self.sda, GPIO.HIGH if bit else GPIO.LOW)
        time.sleep(0.000005)
        GPIO.output(self.scl, GPIO.HIGH)
        time.sleep(0.00001)
        GPIO.output(self.scl, GPIO.LOW)
    
    def write_byte(self, byte):
        """Write 8 bits + check ACK"""
        for i in range(8):
            self.write_bit((byte >> (7 - i)) & 1)
        
        # Check ACK
        GPIO.setup(self.sda, GPIO.IN)
        GPIO.output(self.scl, GPIO.HIGH)
        ack = GPIO.input(self.sda)
        GPIO.output(self.scl, GPIO.LOW)
        GPIO.setup(self.sda, GPIO.OUT)
        
        return ack == 0  # ACK = LOW
    
    def write(self, address, data):
        """Write data to I2C device"""
        self.start_condition()
        
        # Write address (7-bit + write bit)
        if not self.write_byte((address << 1) | 0):
            print("NACK from device!")
            self.stop_condition()
            return False
        
        # Write data bytes
        for byte in data:
            if not self.write_byte(byte):
                print("NACK during data!")
                self.stop_condition()
                return False
        
        self.stop_condition()
        return True

# Verwendung:
i2c = SoftwareI2C(SDA_PIN, SCL_PIN)
i2c.write(0x27, [0xFF])  # LCD Backlight an
```

**Performance:**
- Speed: ~5-20 kHz (sehr langsam!)
- CPU: 20-40% während Transfer
- Timing-Probleme bei hoher Systemlast

### Option C: C++ Kernel-Level Bit-Banging

**Schwierigkeit:** 🔴 Hoch (Kernel-Entwicklung)

Erfordert:
- Kernel-Modul schreiben
- GPIO-Treiber-Kenntnisse
- Kernel-Debugging-Tools

**Nicht empfohlen** außer für spezielle Anforderungen.

## 📊 Vergleich: Hardware vs Software I2C

| Feature | Hardware I2C | i2c-gpio Kernel | Python Bit-Bang |
|---------|--------------|-----------------|-----------------|
| **Speed** | 100-400 kHz | 50-100 kHz | 5-20 kHz |
| **CPU Load** | <1% | 5-10% | 20-40% |
| **Reliability** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ |
| **Setup Difficulty** | ✅ Easy | 🟡 Medium | 🟢 Easy |
| **Clock Stretch** | ✅ Yes | ⚠️ Limited | ❌ No |
| **Multi-Master** | ✅ Yes | ❌ No | ❌ No |
| **Production Ready** | ✅ Yes | ⚠️ Maybe | ❌ No |

## 🎯 EMPFEHLUNG für dein Projekt

### Beste Lösung: Hardware I2C Bus 7 mit beiden Geräten

```cpp
// drone_web_controller.cpp
// Pin 3 (SDA) + Pin 5 (SCL) = Bus 7

I2C_LCD lcd(7, 0x27);           // LCD Display
INA219 power_monitor(7, 0x40);  // INA219 Power Monitor

// Beide parallel, keine Interferenz!
```

**Verkabelung:**
```
Jetson Pin 3 (SDA) ──┬── LCD SDA
                      └── INA219 SDA

Jetson Pin 5 (SCL) ──┬── LCD SCL
                      └── INA219 SCL

Jetson Pin 1 (3.3V) ─┬── LCD VCC
                      └── INA219 VCC

Jetson Pin 6 (GND) ──┬── LCD GND
                      └── INA219 GND
```

### Bus 0 Schutz:

```bash
# udev Regel erstellen (einmal ausführen):
sudo bash -c 'cat > /etc/udev/rules.d/99-i2c-protection.rules << EOF
# Protect Bus 0 (EEPROM)
KERNEL=="i2c-0", MODE="0600", OWNER="root", GROUP="root"
# Allow Bus 7 (External devices)
KERNEL=="i2c-7", MODE="0666", OWNER="root", GROUP="i2c"
EOF'

sudo udevadm control --reload-rules
sudo udevadm trigger

# Verifizieren:
ls -la /dev/i2c-*
```

### Software I2C: NICHT nötig!

**Warum kompliziert machen?**
- ❌ Langsamer
- ❌ Fehleranfälliger
- ❌ Mehr Code
- ❌ Höhere CPU-Last

**vs.**

- ✅ Hardware I2C Bus 7 mit 2 Geräten: Funktioniert perfekt!

---

**Zusammenfassung:**
1. ✅ **KEIN Multiplexer nötig** - I2C unterstützt Multi-Device nativ!
2. ✅ **LCD + INA219 parallel auf Bus 7** - Unterschiedliche Adressen
3. ✅ **Bus 0 Schutz via udev** - EEPROM ist bereits kernel-protected
4. ⚠️ **Software I2C** - Technisch möglich, aber unnötig und langsam

**Deine beste Option: Beide Geräte direkt an Pin 3/5 (Bus 7) anschließen!** 🎯
