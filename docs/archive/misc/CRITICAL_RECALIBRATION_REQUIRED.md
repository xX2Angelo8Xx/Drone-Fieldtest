# KRITISCHER BUG FIX - INA219 Kalibrierung NEU MACHEN!

**Datum:** 19. November 2025  
**Problem:** Kalibrierung zeigt 18.3V bei 16.8V Netzteil (1.5V Fehler!)  
**Ursache:** Dritter Kalibrierungspunkt war bei 16.0V statt 16.8V

---

## 🔴 Sofort-Anleitung: Neu-Kalibrierung

### Schritt 1: System stoppen
```bash
sudo systemctl stop drone-recorder
```

### Schritt 2: Alte (kaputte) Kalibrierung löschen
```bash
rm /home/angelo/Projects/Drone-Fieldtest/ina219_calibration.json
```

### Schritt 3: Kalibrierungs-Tool ausführen
```bash
cd /home/angelo/Projects/Drone-Fieldtest
sudo ./build/tools/calibrate_battery_monitor
```

### Schritt 4: Kalibrierungspunkte EXAKT einstellen
```
⚠️ KRITISCH: Netzteil MUSS auf exakte Werte eingestellt sein!

1. Punkt: 14.6V (3.65V/Zelle) - KEIN RECORDING!
   → Netzteil auf 14.60V einstellen
   → Mit Voltmeter prüfen
   → ENTER drücken

2. Punkt: 15.7V (3.925V/Zelle) - KEIN RECORDING!
   → Netzteil auf 15.70V einstellen
   → Mit Voltmeter prüfen
   → ENTER drücken

3. Punkt: 16.8V (4.20V/Zelle) - KEIN RECORDING!
   → Netzteil auf 16.80V einstellen (NICHT 16.0V!!!)
   → Mit Voltmeter prüfen
   → ENTER drücken
```

### Schritt 5: System neu starten
```bash
sudo systemctl start drone-recorder
```

### Schritt 6: Verifizieren
```bash
# Web UI öffnen: http://192.168.4.1:8080
# Power Tab prüfen:

Netzteil 14.6V → GUI sollte 14.60V zeigen (±0.01V)
Netzteil 15.7V → GUI sollte 15.70V zeigen (±0.01V)
Netzteil 16.8V → GUI sollte 16.80V zeigen (±0.01V) ✅ NICHT 18.3V!
```

---

## 🔍 Was war das Problem?

### Alte (kaputte) Kalibrierung:
```json
"raw_readings": [14.400, 15.515, 16.0],  ← Dritter Punkt nur 16.0V!
"reference_voltages": [14.6, 15.7, 16.8]
```

**Berechnung Segment 2:**
```
Zwei Punkte: (15.515, 15.7) und (16.0, 16.8)
Slope = (16.8 - 15.7) / (16.0 - 15.515) = 1.1 / 0.485 = 2.27

Aber bei echten 16.8V Netzteil:
Raw ≈ 16.4V (gemessen)
Kalibriert = 2.27 × 16.4 - 19.5 = 17.7V ❌

Bei 18.3V angezeigt:
Raw = (18.3 + 19.5) / 2.27 = 16.65V
```

**Das Problem:** Segment 2 wurde mit nur 16.0V trainiert, aber soll bis 16.8V gehen!

### Neue (korrekte) Kalibrierung wird haben:
```json
"raw_readings": [~14.4, ~15.5, ~16.6],  ← Dritter Punkt bei 16.8V!
"reference_voltages": [14.6, 15.7, 16.8]
```

---

## ✅ Erwartete Verbesserung

### Vorher (kaputt):
| Netzteil | GUI Anzeige | Fehler |
|----------|-------------|--------|
| 14.6V    | 14.60V      | 0.00V  |
| 15.7V    | 15.70V      | 0.00V  |
| 16.8V    | **18.30V**  | **+1.50V** ❌ |

### Nachher (korrekt):
| Netzteil | GUI Anzeige | Fehler |
|----------|-------------|--------|
| 14.6V    | 14.60V      | 0.00V  |
| 15.7V    | 15.70V      | 0.00V  |
| 16.8V    | 16.80V      | 0.00V ✅ |

---

## 🛠️ Bug Fix Summary

### Zwei Fixes implementiert:

#### 1. Critical Battery Shutdown (unabhängig von Recording)
```cpp
// VORHER: Nur während Recording gecheckt
if (battery.is_critical && recording_active_) {
    critical_battery_counter_++;
    // ...
}

// JETZT: IMMER gecheckt (auch ohne Recording!)
if (battery.is_critical) {
    critical_battery_counter_++;
    // ...
    if (critical_battery_counter_ >= 10) {
        if (recording_active_) {
            stopRecording();
        }
        system_shutdown_requested_ = true;
        shutdown_requested_ = true;
        break;  // Exit monitor loop
    }
}
```

#### 2. Kalibrierungs-Tool erstellt
- `/home/angelo/Projects/Drone-Fieldtest/tools/calibrate_battery_monitor.cpp`
- Guided 3-point calibration
- Automatic 2-segment calculation
- JSON export
- Error validation

---

## 📊 Testing nach Neu-Kalibrierung

### Test 1: Voltage Accuracy
```bash
# Netzteil auf 14.6V
# GUI Power Tab prüfen → sollte 14.60V zeigen

# Netzteil auf 15.7V
# GUI Power Tab prüfen → sollte 15.70V zeigen

# Netzteil auf 16.8V
# GUI Power Tab prüfen → sollte 16.80V zeigen (NICHT 18.3V!)
```

### Test 2: Critical Battery Shutdown (OHNE Recording)
```bash
# System läuft im Idle (kein Recording!)
# Netzteil auf 14.3V senken (unter 14.4V Schwelle)
# Console beobachten:

# Erwarte:
"⚠️⚠️⚠️ CRITICAL BATTERY VOLTAGE detected (count=1/10)"
"⚠️⚠️⚠️ CRITICAL BATTERY VOLTAGE detected (count=2/10)"
...
"⚠️⚠️⚠️ CRITICAL BATTERY VOLTAGE detected (count=10/10)"
"CRITICAL THRESHOLD REACHED!"
"INITIATING EMERGENCY SHUTDOWN"
"System will power off in 5 seconds..."

# System sollte herunterfahren ✅
```

### Test 3: Critical Battery Shutdown (MIT Recording)
```bash
# Recording starten
# Netzteil auf 14.3V senken
# Erwarte:

"⚠️⚠️⚠️ CRITICAL BATTERY VOLTAGE detected (count=10/10)"
"Stopping active recording before shutdown..."
"Recording stopped due to critical battery voltage"
"System will power off in 5 seconds..."

# System sollte herunterfahren ✅
```

---

## 🚨 WICHTIG: Kalibrierung NUR im Idle State!

**DO:**
- ✅ Kalibrierung OHNE Recording
- ✅ System läuft, aber keine Kamera aktiv
- ✅ Strom: <1A (Idle)
- ✅ Netzteil EXAKT auf 14.6V, 15.7V, 16.8V einstellen

**DON'T:**
- ❌ **NIEMALS während Recording kalibrieren!**
- ❌ Strom >2A (verfälscht Messung durch Kabel-Spannungsabfall)
- ❌ Ungenaue Netzteil-Einstellung (z.B. 16.0V statt 16.8V)
- ❌ Zu schnell messen (warte 5 Sekunden nach Netzteil-Anpassung)

---

**Nach Neu-Kalibrierung sollte alles perfekt sein! 🚀**
