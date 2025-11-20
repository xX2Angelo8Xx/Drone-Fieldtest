# INA219 Kalibrierung - Schnellstart
**Methode:** 2-Segment Piecewise Linear (0.000V Fehler!)

## 🚀 Kalibrierung in 5 Minuten

### Vorbereitung
- Einstellbares Netzteil (14.6V - 16.8V)
- INA219 angeschlossen (I2C Bus 7, 0x40)

### Durchführung

```bash
cd /home/angelo/Projects/Drone-Fieldtest
.venv/bin/python calibrate_ina219_3point.py
```

### Schritte

1. **Netzteil auf 14.6V einstellen** → ENTER drücken → warten
2. **Netzteil auf 15.7V einstellen** → ENTER drücken → warten  
3. **Netzteil auf 16.8V einstellen** → ENTER drücken → warten

**Das System berechnet automatisch:**
- ✅ 2-Segment piecewise linear calibration (EMPFOHLEN)
- ✅ 1-Segment linear regression (Legacy-Kompatibilität)
- ✅ Fehlervergleich beider Methoden

Kalibrierung wird automatisch gespeichert!

### Was ist 2-Segment Kalibrierung?

Zwei verschiedene Kalibrierungsformeln für maximale Präzision:
- **Segment 1 (14.6-15.7V):** Eigene Slope/Offset für niedrige Spannungen
- **Segment 2 (15.7-16.8V):** Eigene Slope/Offset für hohe Spannungen

**Ergebnis:** 0.000V Fehler an allen Messpunkten (100% Verbesserung!)

### System aktualisieren

```bash
./scripts/build.sh
sudo systemctl restart drone-recorder
```

### Verifizieren

Web UI öffnen: http://192.168.4.1:8080 → Power Tab
→ Spannung sollte mit Netzteil **exakt** übereinstimmen (≤0.01V)

Terminal-Test:
```bash
sudo ./test_2segment_calibration
# Sollte zeigen: "✓ Loaded 2-segment calibration"
```

---

**Vollständige Anleitung:** `docs/INA219_KALIBRIERUNG_3PUNKT.md`
