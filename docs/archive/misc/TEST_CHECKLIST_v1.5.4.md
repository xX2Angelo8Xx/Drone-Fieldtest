# Test-Checkliste: 2-Segment Kalibrierung + Sauberer Shutdown (v1.5.4)

**Datum:** 19. November 2025  
**Tester:** Angelo  
**System:** Jetson Orin Nano + ZED 2i + INA219

## 🎯 Test-Ziele

### 1. ✅ 2-Segment Voltage Kalibrierung
- [ ] Voltage in GUI stimmt exakt mit Netzteil überein (±0.01V)
- [ ] Battery Percentage korrekt berechnet (0% bei 14.6V, 100% bei 16.8V)
- [ ] Remaining Time Berechnung nutzt präzise Voltage-Werte

### 2. ✅ Sauberer Shutdown (v1.5.4 Dual-Flag Fix)
- [ ] GUI Shutdown Button → System fährt herunter
- [ ] Ctrl+C → Programm stoppt OHNE System-Shutdown
- [ ] Recording wird sauber beendet (keine Korruption)
- [ ] WiFi AP wird korrekt abgeschaltet
- [ ] Keine Thread-Deadlocks

### 3. ✅ Recording Integrität
- [ ] SVO2-Datei ist nach Shutdown nicht korrupt
- [ ] Alle Frames gespeichert (keine Lücken)
- [ ] sensor_data.csv vollständig
- [ ] recording.log zeigt sauberes Ende

---

## 📋 Test-Ablauf

### Vorbereitung
```bash
# 1. System neu starten mit aktueller Version
sudo systemctl restart drone-recorder

# 2. Netzteil anschließen (14.6V - 16.8V einstellbar)
# 3. Web UI öffnen: http://192.168.4.1:8080
```

---

## Test 1: Voltage Kalibrierung (14.6V)

### Setup
```bash
# Netzteil auf 14.6V einstellen (kritische Schwelle)
```

### Erwartete Werte in GUI (Power Tab)
```
Voltage:          14.60V ± 0.01V
Cell Voltage:     3.65V ± 0.003V
Battery %:        0% (oder 1-2% wenn leicht drüber)
Status:           ⚠️ CRITICAL (weil genau auf Schwelle)
```

### ✅ Pass-Kriterien
- [ ] Voltage-Anzeige weicht max. 0.01V vom Netzteil ab
- [ ] Battery % zeigt 0-2% (wir sind am Nullpunkt!)
- [ ] Status ist CRITICAL (rot)

---

## Test 2: Voltage Kalibrierung (15.7V - Mittelpunkt)

### Setup
```bash
# Netzteil auf 15.7V einstellen (war 0.274V Fehler bei 1-Segment!)
```

### Erwartete Werte in GUI
```
Voltage:          15.70V ± 0.01V
Cell Voltage:     3.925V ± 0.003V
Battery %:        50% (Mittelpunkt zwischen 14.6V und 16.8V)
Status:           ⚠️ WARNING (zwischen 14.8V und 16.8V)
```

### ✅ Pass-Kriterien
- [ ] Voltage EXAKT 15.70V (±0.01V) → Beweis für 0.000V Fehler!
- [ ] Battery % ca. 50%
- [ ] Status ist WARNING (gelb)

**WICHTIG:** Bei 1-Segment hätte hier 15.97V gestanden (+0.27V Fehler)!

---

## Test 3: Voltage Kalibrierung (16.8V - Maximum)

### Setup
```bash
# Netzteil auf 16.8V einstellen (Vollladung)
```

### Erwartete Werte in GUI
```
Voltage:          16.80V ± 0.01V
Cell Voltage:     4.20V ± 0.003V
Battery %:        100%
Status:           ✓ HEALTHY (grün)
```

### ✅ Pass-Kriterien
- [ ] Voltage exakt 16.80V
- [ ] Battery % = 100%
- [ ] Status ist HEALTHY (grün)

---

## Test 4: Recording + Sauberer Shutdown (GUI Button)

### Setup
```bash
# Netzteil auf 16.0V (sicherer Bereich)
# USB Stick: DRONE_DATA (NTFS/exFAT)
```

### Test-Ablauf
1. **Recording starten**
   ```
   GUI: Recording Tab → Start Recording
   Dauer: 30 Sekunden
   ```

2. **GUI Shutdown Button drücken** (während Recording läuft!)
   ```
   GUI: Settings Tab → Shutdown System Button
   ```

### Erwartetes Verhalten
```
✅ Recording stoppt SOFORT (max. 3 Sekunden)
✅ LCD zeigt: "Recording Stopped" (3 Sekunden sichtbar)
✅ LCD zeigt: "Shutting Down..." 
✅ System fährt herunter (nicht nur Programm!)
✅ WiFi AP wird abgeschaltet
```

### Nach Reboot: Recording Verifikation
```bash
# USB Stick prüfen
cd /media/angelo/DRONE_DATA/
ls -lh flight_*/

# Erwartete Struktur:
# flight_20251119_HHMMSS/
#   ├── video.svo2          (ca. 1-2 GB für 30s @ HD720@60fps)
#   ├── sensor_data.csv     (vollständig)
#   └── recording.log       (zeigt "Recording stopped cleanly")
```

### ✅ Pass-Kriterien
- [ ] SVO2-Datei existiert und ist nicht korrupt
- [ ] `zed_svo_export` kann Datei öffnen (keine "Invalid file" Fehler)
- [ ] sensor_data.csv hat keine abgebrochenen Zeilen
- [ ] recording.log zeigt "Recording completed successfully" oder "Recording stopped"
- [ ] KEIN "Recording aborted" oder "ERROR" in Logs

---

## Test 5: Ctrl+C Graceful Exit (OHNE System-Shutdown)

### Test-Ablauf
```bash
# System manuell starten (nicht als Service!)
sudo systemctl stop drone-recorder
cd /home/angelo/Projects/Drone-Fieldtest
sudo ./build/apps/drone_web_controller/drone_web_controller

# Recording starten (GUI)
# 10 Sekunden warten
# Im Terminal: Ctrl+C drücken
```

### Erwartetes Verhalten
```
✅ Programm zeigt: "Shutdown signal received"
✅ Recording stoppt sauber
✅ WiFi AP wird abgeschaltet
✅ Programm beendet sich (exit code 0)
✅ System läuft weiter! (kein shutdown -h now)
✅ Ethernet-Internet funktioniert sofort wieder
```

### ✅ Pass-Kriterien
- [ ] Programm beendet sich innerhalb 5 Sekunden
- [ ] Recording ist nicht korrupt
- [ ] System bleibt online (SSH funktioniert weiter)
- [ ] WiFi AP ist weg, Ethernet funktioniert

**WICHTIG:** Das ist der v1.5.4 Dual-Flag Fix - GUI Shutdown ≠ Ctrl+C!

---

## Test 6: Recording Während Niedrige Spannung

### Setup
```bash
# Netzteil auf 14.7V (knapp über kritisch)
```

### Test-Ablauf
```
1. Recording starten
2. Netzteil langsam auf 14.5V absenken (unter kritisch!)
3. Beobachten, was passiert
```

### Erwartetes Verhalten
```
⚠️ Bei 14.6V: Status wird CRITICAL (rot)
⚠️ Bei < 14.6V: Battery % = 0%
❓ Optional: Auto-Stop Recording bei kritischer Spannung?
   (Falls implementiert, sonst OK wenn weiterläuft)
```

---

## 📊 Testergebnisse Dokumentieren

### Voltage Test Ergebnisse
| Netzteil | GUI Voltage | Abweichung | Battery % | Status | ✅/❌ |
|----------|-------------|------------|-----------|--------|------|
| 14.6V    |             |            |           |        |      |
| 15.7V    |             |            |           |        |      |
| 16.8V    |             |            |           |        |      |

### Shutdown Test Ergebnisse
| Test | Recording OK? | Shutdown OK? | WiFi Down? | ✅/❌ |
|------|---------------|--------------|------------|------|
| GUI Button |       |              |            |      |
| Ctrl+C     |       |              |            |      |

---

## 🔍 Logs zum Prüfen

### System Logs
```bash
# Service Logs
sudo journalctl -u drone-recorder -n 100 --no-pager

# Wichtige Zeilen:
# "✓ Loaded 2-segment calibration"
# "Segment 1 (<15.7V): ..."
# "Segment 2 (>=15.7V): ..."
# "Recording stopped cleanly"
# "Shutdown complete"
```

### Recording Logs
```bash
# Recording Log prüfen
cat /media/angelo/DRONE_DATA/flight_*/recording.log | tail -20

# Erwartete letzte Zeile:
# "[ZED] Recording completed successfully"
# ODER
# "[ZED] Recording stopped (duration: XXs)"
```

### SVO2 Integrität
```bash
# Mit ZED SDK Tools prüfen
cd /media/angelo/DRONE_DATA/flight_*/

# Frame Count
/usr/local/zed/tools/ZED_SVO_Editor -info video.svo2 | grep "Number of frames"

# Erwartung: ~1800 frames für 30s @ 60fps
```

---

## 🎉 Success Criteria

### PASS = Alles erfüllt:
- ✅ Voltage-Abweichung ≤ 0.01V bei allen drei Messpunkten
- ✅ Battery % startet bei 0% (14.6V) und endet bei 100% (16.8V)
- ✅ GUI Shutdown Button → System fährt herunter + Recording sauber
- ✅ Ctrl+C → Programm stoppt + System bleibt online
- ✅ Keine korrupten SVO2-Dateien
- ✅ Keine Thread-Deadlocks in Logs
- ✅ WiFi AP wird in beiden Fällen abgeschaltet

### FAIL = Eines oder mehreres:
- ❌ Voltage-Abweichung > 0.05V (2-Segment versagt!)
- ❌ Recording korrupt nach Shutdown
- ❌ Thread-Deadlock ("Resource deadlock avoided")
- ❌ System fährt bei Ctrl+C herunter (Dual-Flag kaputt!)
- ❌ WiFi AP bleibt nach Exit online (Netzwerk-Cleanup fehlt)

---

## 💡 Troubleshooting

### Problem: Voltage stimmt nicht überein
```bash
# Kalibrierung neu laden
sudo systemctl restart drone-recorder

# Prüfen ob 2-segment geladen wurde
sudo journalctl -u drone-recorder | grep "segment"

# Sollte zeigen:
# "✓ Loaded 2-segment calibration"
```

### Problem: System fährt bei Ctrl+C herunter
```bash
# Code prüfen - Dual-Flag Bug?
grep -A5 "isSystemShutdownRequested" apps/drone_web_controller/drone_web_controller.cpp

# main() sollte haben:
# if (controller.isSystemShutdownRequested()) {
#     system("sudo shutdown -h now");
# }
```

### Problem: Recording korrupt
```bash
# Prüfen ob STOPPING → IDLE Transition vollständig war
sudo journalctl -u drone-recorder | grep "State transitioned"

# Sollte zeigen:
# "State transitioned to IDLE"
# DANN ERST "recording_stop_complete_ = true"
```

---

**Viel Erfolg beim Testen! 🚀**

Melde dich mit Ergebnissen - insbesondere:
1. Voltage-Genauigkeit am Mittelpunkt (15.7V)
2. Recording-Integrität nach Shutdown
3. Ob Dual-Flag (Ctrl+C vs GUI) korrekt funktioniert
