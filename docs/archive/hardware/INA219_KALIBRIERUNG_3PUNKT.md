> Hinweis (archiviert): Konsolidierter Stand in `docs/guides/hardware/HARDWARE_INTEGRATION_GUIDE.md` (Akkusektion). Kurzüberblick in `docs/KEY_LEARNINGS.md`.

# INA219 3-Punkt Kalibrierung - Anleitung
**Datum:** 19. November 2025  
**System:** Jetson Orin Nano + INA219 (I2C Bus 7, Adresse 0x40)  
**Kalibrierungsmethode:** 2-Segment Piecewise Linear (v1.5.4+)

## 🎯 Übersicht

Das INA219 wird mit einer **2-Segment piecewise linearen Kalibrierung** kalibriert, um maximale Präzision über den gesamten Spannungsbereich (14.6V - 16.8V) zu erreichen. Die Kalibrierung wird **permanent gespeichert** und automatisch beim Systemstart geladen.

### Was ist 2-Segment Kalibrierung?

Statt einer einzigen linearen Formel verwendet das System **zwei verschiedene Kalibrierungsformeln**:

```
Segment 1 (14.6V - 15.7V):  Calibrated_V = slope1 × Raw_V + offset1
Segment 2 (15.7V - 16.8V):  Calibrated_V = slope2 × Raw_V + offset2
```

**Vorteile gegenüber 1-Segment:**
- ✅ **Perfekte Genauigkeit** an allen 3 Messpunkten (0.000V Fehler!)
- ✅ **100% Fehlerreduktion** am kritischen Mittelpunkt (war 0.275V → jetzt 0.000V)
- ✅ **Kompensiert INA219-Nichtlinearität** über gesamten Bereich
- ✅ **Essentiell für präzise "Remaining Time" Berechnung**

**Vergleich:**
| Kalibrierung | Max. Fehler | Fehler am Mittelpunkt (15.7V) |
|--------------|-------------|-------------------------------|
| 1-Segment | 0.275V (1.75%) | +0.274V |
| 2-Segment | 0.000V (0.00%) | ±0.000V |

## 📋 Voraussetzungen

### Hardware
- ✅ Einstellbares Netzteil (14.6V - 16.8V, min. 2A)
- ✅ INA219 Power Monitor angeschlossen (I2C Bus 7, 0x40)
- ✅ Jetson Orin Nano

### Software
- ✅ Python Virtual Environment mit INA219-Bibliothek
- ✅ Drone Controller Build-System

## 🔧 Kalibrierungspunkte

Das System kalibriert an **drei kritischen Punkten** für 4S LiPo-Batterien:

| Punkt | Spannung | Beschreibung | Zweck |
|-------|----------|--------------|-------|
| **1. Minimum** | **14.6V** | 3.65V/Zelle | Kritische Unterspannung (Auto-Stop) |
| **2. Mitte** | **15.7V** | 3.925V/Zelle | Mathematischer Mittelpunkt |
| **3. Maximum** | **16.8V** | 4.20V/Zelle | Vollladung |

**Warum 3 Punkte?**
- Erfasst Nichtlinearitäten des INA219-ADC
- Kompensiert Shunt-Widerstand-Toleranzen
- Präzise über gesamten Betriebsbereich
- Least-Squares-Fit für optimale Genauigkeit

## 🚀 Kalibrierung durchführen

### Schritt 1: Netzteil vorbereiten
```bash
# Netzteil einstellen:
# - Spannung: 14.6V (erster Messpunkt)
# - Strombegrenzung: 2-5A
# - INA219 anschließen (Vin+ / Vin-)
```

### Schritt 2: Kalibrierungsprogramm starten
```bash
cd /home/angelo/Projects/Drone-Fieldtest
.venv/bin/python calibrate_ina219_3point.py
```

### Schritt 3: Anweisungen folgen

Das Programm fordert dich auf, das Netzteil auf folgende Spannungen einzustellen:

#### **Messpunkt 1: 14.6V (Minimum)**
```
CALIBRATION POINT 1/3: Minimum (Critical)
─────────────────────────────────────────────────────────────────────────────

Please adjust your power supply to: 14.6V
(3.65V per cell)

Press ENTER when power supply is set to 14.6V...
```

**Aktion:**
1. Netzteil auf **genau 14.6V** einstellen
2. 10 Sekunden warten (Stabilisierung)
3. ENTER drücken
4. Programm nimmt **20 Samples über 2 Sekunden**

#### **Messpunkt 2: 15.7V (Mitte)**
```
CALIBRATION POINT 2/3: Middle (Midpoint)
─────────────────────────────────────────────────────────────────────────────

Please adjust your power supply to: 15.7V
(3.925V per cell)

Press ENTER when power supply is set to 15.7V...
```

**Aktion:**
1. Netzteil auf **genau 15.7V** einstellen
2. 10 Sekunden warten
3. ENTER drücken
4. Warten auf Messung

#### **Messpunkt 3: 16.8V (Maximum)**
```
CALIBRATION POINT 3/3: Maximum (Full)
─────────────────────────────────────────────────────────────────────────────

Please adjust your power supply to: 16.8V
(4.2V per cell)

Press ENTER when power supply is set to 16.8V...
```

**Aktion:**
1. Netzteil auf **genau 16.8V** einstellen
2. 10 Sekunden warten
3. ENTER drücken
4. Warten auf finale Messung

### Schritt 4: Ergebnisse überprüfen

Das Programm zeigt die Kalibrierungsergebnisse:

```
================================================================================
  CALCULATING CALIBRATION COEFFICIENTS
================================================================================

Calibration formula: Calibrated_V = 1.012345 × Raw_V + 0.045678

Verification:
Point                Reference     Raw Reading    Calibrated     Error    
────────────────────────────────────────────────────────────────────────────────
Minimum (Critical)      14.600V      14.420V      14.601V     +0.001V
Middle (Midpoint)       15.700V      15.515V      15.699V     -0.001V
Maximum (Full)          16.800V      16.585V      16.801V     +0.001V

Maximum error after calibration: 0.0012V (0.007%)
```

**Gute Kalibrierung:**
- ✅ Maximaler Fehler < 0.01V (< 0.1%)
- ✅ Fehler gleichmäßig verteilt
- ✅ Slope nahe 1.0 (typisch: 0.98 - 1.02)

**Schlechte Kalibrierung (Wiederholung nötig):**
- ❌ Maximaler Fehler > 0.05V
- ❌ Systematische Abweichung an einem Punkt
- ❌ Slope außerhalb 0.95 - 1.05

### Schritt 5: Kalibrierung speichern

```
================================================================================
  SAVING CALIBRATION
================================================================================

Calibration file: /home/angelo/Projects/Drone-Fieldtest/ina219_calibration.json
✓ Calibration saved successfully
✓ File permissions set (readable by all)
```

Die Kalibrierung wird automatisch gespeichert in:
```
/home/angelo/Projects/Drone-Fieldtest/ina219_calibration.json
```

### Schritt 6: System neu starten

```bash
# Build und Deploy
cd /home/angelo/Projects/Drone-Fieldtest
./scripts/build.sh
sudo cp build/apps/drone_web_controller/drone_web_controller /usr/local/bin/
sudo systemctl restart drone-recorder

# Logs überprüfen
sudo journalctl -u drone-recorder -f | grep -i calibration
```

**Erwartete Ausgabe:**
```
[BatteryMonitor] Loaded calibration from: /home/angelo/Projects/Drone-Fieldtest/ina219_calibration.json
[BatteryMonitor]   Formula: Calibrated_V = 1.012345 × Raw_V + +0.045678
[WEB_CONTROLLER] ✓ Battery monitor initialized
```

## 📊 Kalibrierungsdatei Format

Die Kalibrierung wird als JSON gespeichert:

```json
{
  "slope": 1.012345,
  "offset": 0.045678,
  "calibration_date": "2025-11-19 14:30:00",
  "raw_readings": [14.420, 14.615, 16.585],
  "reference_voltages": [14.6, 14.8, 16.8],
  "max_error": 0.0012,
  "calibration_points": [
    {"name": "Minimum (Critical)", "voltage": 14.6, "description": "3.65V per cell"},
    {"name": "Middle (Midpoint)", "voltage": 15.7, "description": "3.925V per cell"},
    {"name": "Maximum (Full)", "voltage": 16.8, "description": "4.2V per cell"}
  ]
}
```

## 🔍 Kalibrierung verifizieren

### Web UI Überprüfung
1. Browser öffnen: http://192.168.4.1:8080
2. Power-Tab öffnen
3. Netzteil auf bekannte Spannung einstellen (z.B. 15.0V)
4. Angezeigte Spannung überprüfen (sollte ±0.01V stimmen)

### Terminal-Test
```bash
# Test-Skript mit Kalibrierung
cd /home/angelo/Projects/Drone-Fieldtest
.venv/bin/python battery_monitor.py

# Erwartete Ausgabe:
# Voltage: 15.998V  (bei 16.0V Netzteil)
# Cell V:  3.999V
```

## 🛠️ Troubleshooting

### Problem: INA219 nicht gefunden
```
❌ Error initializing INA219: [Errno 121] Remote I/O error
```

**Lösung:**
```bash
# I2C-Bus scannen
sudo i2cdetect -y -r 7

# Erwartetes Ergebnis: Gerät bei 0x40
```

### Problem: Unstabile Messwerte
```
Sample 1: 15.768V
Sample 2: 15.923V  ← Große Schwankung!
Sample 3: 15.645V
```

**Ursachen:**
- Netzteil nicht stabil → besseres Netzteil verwenden
- Lose Verbindungen → Kabel überprüfen
- Störungen auf I2C-Bus → kürzere Kabel

**Lösung:**
- 30 Sekunden nach Spannungseinstellung warten
- Kalibrierung wiederholen

### Problem: Maximaler Fehler zu groß
```
Maximum error after calibration: 0.0850V (0.506%)  ← Zu hoch!
```

**Lösung:**
1. Kalibrierung wiederholen
2. Netzteilgenauigkeit überprüfen (mit Multimeter)
3. Shunt-Widerstand prüfen (sollte 0.1Ω sein)

### Problem: Kalibrierung wird nicht geladen
```
[BatteryMonitor] ⚠️ No calibration file found, using raw readings
```

**Lösung:**
```bash
# Datei existiert?
ls -la /home/angelo/Projects/Drone-Fieldtest/ina219_calibration.json

# Berechtigungen setzen
chmod 644 /home/angelo/Projects/Drone-Fieldtest/ina219_calibration.json

# Service neu starten
sudo systemctl restart drone-recorder
```

## 🔄 Kalibrierung aktualisieren

Neue Kalibrierung erforderlich bei:
- Hardware-Austausch (neues INA219-Modul)
- Shunt-Widerstand-Wechsel
- Signifikante Temperaturänderungen (>30°C)
- Jährliche Wartung

**Prozess:**
```bash
# Alte Kalibrierung sichern
cp ina219_calibration.json ina219_calibration_backup_$(date +%Y%m%d).json

# Neue Kalibrierung durchführen
.venv/bin/python calibrate_ina219_3point.py

# System neu starten
sudo systemctl restart drone-recorder
```

## 📈 Genauigkeit

**Vor Kalibrierung:**
- Typischer Fehler: 0.2V - 0.3V (1.2% - 1.8%)
- Ursache: Shunt-Toleranz, ADC-Offset

**Nach 3-Punkt Kalibrierung:**
- Typischer Fehler: < 0.01V (< 0.06%)
- Ausreichend für präzise Battery-Protection

**Messunsicherheit:**
- INA219 ADC: ±0.4% (Datenblatt)
- Shunt-Widerstand: ±1% Toleranz
- Thermisches Driften: ±0.1%/°C
- **Gesamt nach Kalibrierung: ±0.5% (±0.08V @ 16V)**

## ✅ Checkliste

Vor Kalibrierung:
- [ ] Einstellbares Netzteil vorhanden (14.6V - 16.8V)
- [ ] INA219 korrekt angeschlossen (I2C Bus 7, 0x40)
- [ ] System funktionsfähig (Build erfolgreich)
- [ ] Terminal-Zugriff auf Jetson

Während Kalibrierung:
- [ ] Netzteil stabilisiert (10s Wartezeit pro Punkt)
- [ ] Alle 3 Messpunkte durchgeführt
- [ ] Maximaler Fehler < 0.02V
- [ ] Kalibrierung gespeichert

Nach Kalibrierung:
- [ ] System neu gestartet
- [ ] Kalibrierung geladen (Logs prüfen)
- [ ] Web UI zeigt korrekte Spannungen
- [ ] Testmessung mit bekannter Spannung erfolgreich

## 📚 Technische Details

### Lineare Kalibrierung

**Formel:**
```
Calibrated_Voltage = slope × Raw_Voltage + offset
```

**Berechnung (Least Squares):**
```python
# Mittelwerte
mean_raw = Σ(raw_readings) / n
mean_ref = Σ(reference_voltages) / n

# Slope
numerator = Σ((raw[i] - mean_raw) * (ref[i] - mean_ref))
denominator = Σ((raw[i] - mean_raw)²)
slope = numerator / denominator

# Offset
offset = mean_ref - slope * mean_raw
```

### Warum 3 Punkte?

**1 Punkt (Offset):**
- ❌ Korrigiert nur systematischen Fehler
- ❌ Keine Kompensation von Gain-Fehlern

**2 Punkte (Linear):**
- ✅ Korrigiert Offset + Gain
- ⚠️ Keine Validierung (könnte falsch sein)

**3 Punkte (Linear mit Validierung):**
- ✅ Korrigiert Offset + Gain
- ✅ Mittelpunkt validiert Kalibrierung
- ✅ Erkennt Messfehler
- ✅ Optimale Genauigkeit

---

## 📞 Support

Bei Problemen:
1. Logs prüfen: `sudo journalctl -u drone-recorder -f`
2. Kalibrierung wiederholen
3. Hardware-Verbindungen überprüfen
4. Dokumentation konsultieren: `docs/BATTERY_MONITORING_v1.5.5.md`
