# 🚀 LIVESTREAM PERFORMANCE TEST - Schritt für Schritt

## ⚠️ WICHTIG: Du brauchst 3 Terminals!

Das Problem vorhin: `iftop` zeigte 0, weil:
1. ❌ Der Controller war nicht gestartet → kein Hotspot aktiv
2. ❌ Kein Client verbunden → kein Traffic
3. ❌ Livestream nicht aktiviert → keine Daten

---

## 📋 Setup (3 Terminals nebeneinander)

### Terminal 1: Network Monitor 📡
```bash
# Warte bis Controller gestartet ist (Terminal 3)
# Dann erst:
sudo iftop -i wlP1p1s0

# Keybindings:
# - 'n' drücken → DNS lookup ausschalten (schneller!)
# - 't' drücken → Zeit-Averaging ändern
# - 'q' → Beenden
```

**Was du sehen wirst:**
- **Am Anfang:** Alles bei 0 (normal, kein Client verbunden)
- **Nach Client-Verbindung:** Kleine Spitzen (~0.5 Mbps für Status-API)
- **Mit Livestream @ 5 FPS:** TX ~3.0 Mbps 🎯
- **Mit Livestream @ 10 FPS:** TX ~6.0 Mbps 🎯
- **Mit Livestream @ 15 FPS:** TX ~9.0 Mbps ⚠️

---

### Terminal 2: CPU Monitor 💻
```bash
htop

# Useful keys:
# - F4 → Filter eingeben: "drone" → Enter
# - F5 → Tree view (Prozess-Hierarchie)
# - Space → Prozess markieren (highlight)
# - q → Beenden
```

**Was du sehen wirst:**
- **Idle:** 5-10% CPU
- **Recording SVO2:** 65-70% CPU (Bottleneck!)
- **Recording + Livestream @ 5 FPS:** 65-70% CPU (kein Anstieg!)
- **Recording + Livestream @ 10 FPS:** 70-75% CPU (+5%)
- **Recording + Livestream @ 15 FPS:** 75-80% CPU (+10%)

---

### Terminal 3: Controller 🚁
```bash
cd ~/Projects/Drone-Fieldtest

# Option A: Automatisches Setup-Script
./test_livestream.sh

# Option B: Manuell
sudo ./build/apps/drone_web_controller/drone_web_controller
```

**Warte auf diese Zeile:**
```
[MAIN] 🌐 Web Interface: http://10.42.0.1:8080
```
→ Dann ist der Hotspot bereit!

---

## 📱 Test-Ablauf

### Schritt 1: Verbinde Client (Laptop/Phone)
```
WiFi-Name: DroneController
Passwort:  drone123
```

**Nach Verbindung:**
- Terminal 1 (iftop): Sollte jetzt kleine Aktivität zeigen (~0.5 Mbps)
- Terminal 2 (htop): Immer noch 5-10% CPU

---

### Schritt 2: Öffne Web-Interface
```
Browser: http://10.42.0.1:8080
```

**Alternative URLs (falls 10.42.0.1 nicht geht):**
- http://192.168.4.1:8080
- http://localhost:8080 (nur auf Jetson selbst)

---

### Schritt 3: Baseline Test (kein Recording, kein Livestream)
```
Web UI:
  - Bleibe auf Recording Tab
  - Starte NICHTS

Erwartung:
  Terminal 1 (iftop): TX ~0-0.5 Mbps (nur Status-API)
  Terminal 2 (htop):  CPU ~5-10%
```

✅ **Wenn das funktioniert, ist dein Setup korrekt!**

---

### Schritt 4: Recording Test (ohne Livestream)
```
Web UI:
  - Recording Tab
  - START RECORDING klicken
  - Warte 15 Sekunden
  - Beobachte Terminals

Erwartung:
  Terminal 1 (iftop): TX ~0 Mbps (kein Livestream!)
  Terminal 2 (htop):  CPU ~65-70% ⚠️ (Recording = Bottleneck)

  Stop Recording nach Test.
```

---

### Schritt 5: Livestream @ 2 FPS (kein Recording)
```
Web UI:
  - Tab zu "Livestream" wechseln
  - Livestream FPS: 2 FPS (Balanced) ⭐
  - Checkbox "Enable Livestream" aktivieren

Erwartung:
  Terminal 1 (iftop): TX ~1.2 Mbps 📊
  Terminal 2 (htop):  CPU ~5-10% ✅ (sehr leicht!)
  Browser: Bild aktualisiert alle 0.5 Sekunden
```

---

### Schritt 6: Livestream @ 5 FPS
```
Web UI:
  - Dropdown auf "5 FPS (Very Smooth)" ändern

Erwartung:
  Terminal 1 (iftop): TX ~3.0 Mbps 📊
  Terminal 2 (htop):  CPU ~5-10% ✅
  Browser: Bild aktualisiert alle 0.2 Sekunden
```

---

### Schritt 7: Recording + Livestream @ 5 FPS
```
Web UI:
  - Livestream läuft weiter @ 5 FPS
  - Tab zu "Recording" wechseln
  - START RECORDING klicken

Erwartung:
  Terminal 1 (iftop): TX ~3.0 Mbps 📊 (unchanged!)
  Terminal 2 (htop):  CPU ~65-70% (Recording dominiert!)
  Browser: Livestream läuft parallel zur Aufnahme
```

**KRITISCHE ERKENNTNIS:**
→ Livestream fügt nur ~0-5% CPU hinzu!
→ Recording ist der Bottleneck, nicht Livestream!

---

### Schritt 8: Stress Test @ 10 FPS
```
Web UI:
  - Recording noch aktiv
  - Tab zu "Livestream"
  - Dropdown auf "10 FPS (Stress Test)" ändern

Erwartung:
  Terminal 1 (iftop): TX ~6.0 Mbps 📊
  Terminal 2 (htop):  CPU ~70-75% (+5%)
  Browser: Bild aktualisiert sehr flüssig (10 Hz)
```

**Ist das System stabil?**
- ✅ Keine Timeouts im Browser?
- ✅ Bild aktualisiert sich flüssig?
- ✅ Recording läuft weiter (keine Frame-Drops)?

---

### Schritt 9: Network Limit Test @ 15 FPS
```
Web UI:
  - Dropdown auf "15 FPS (Network Test)" ändern

Erwartung:
  Terminal 1 (iftop): TX ~9.0 Mbps 📊 ⚠️
  Terminal 2 (htop):  CPU ~75-80% (+10%)
  Browser: ???
```

**Fragen:**
1. Ist das Bild noch flüssig?
2. Gibt es Timeouts/Freezes?
3. Bleibt iftop bei 9 Mbps oder springt es?
4. Zeigt Browser-Console (F12) Fehler?

**WiFi-Limit erreicht?**
- Theoretisch: 54 Mbps (802.11g) oder 150 Mbps (802.11n)
- Real-world: 10-20 Mbps (Overhead, Interferenzen)
- Wenn TX > 10 Mbps und instabil → WiFi-Sättigung! ⚠️

---

## 🎯 Erwartete Ergebnisse (Tabelle)

| Test | CPU | iftop TX | Stabil? |
|------|-----|----------|---------|
| Idle | 5-10% | 0 Mbps | ✅ |
| Recording only | 65-70% | 0 Mbps | ✅ |
| Livestream 2 FPS | 5-10% | 1.2 Mbps | ✅ |
| Livestream 5 FPS | 5-10% | 3.0 Mbps | ✅ |
| Rec + Live 5 FPS | 65-70% | 3.0 Mbps | ✅ |
| Rec + Live 10 FPS | 70-75% | 6.0 Mbps | ✅? |
| Rec + Live 15 FPS | 75-80% | 9.0 Mbps | ⚠️? |

---

## 🐛 Troubleshooting

### Problem: iftop zeigt nur 0
**Ursachen:**
1. Controller nicht gestartet → siehe Terminal 3
2. Hotspot nicht aktiv → warte auf "Web Interface ready" Meldung
3. Kein Client verbunden → verbinde Laptop/Phone
4. Falsches Interface → versuche `sudo iftop` ohne `-i` Parameter

**Check:**
```bash
# Ist Hotspot aktiv?
nmcli connection show --active | grep DroneController

# Sollte zeigen:
# DroneController  xxx-xxx-xxx  wifi  wlP1p1s0
```

### Problem: htop findet Prozess nicht
**Lösung:**
```bash
# In htop:
# - F4 drücken
# - "drone" eintippen
# - Enter
# Sollte drone_web_controller highlighten
```

### Problem: Browser kann nicht verbinden
**Checkliste:**
- [ ] WiFi "DroneController" verbunden?
- [ ] Terminal 3 zeigt "Web Interface: http://10.42.0.1:8080"?
- [ ] Versuche Alternative: http://192.168.4.1:8080
- [ ] Firewall auf Client-Gerät aus?

---

## 📊 Beispiel iftop Output

### Baseline (Controller läuft, Client verbunden, kein Livestream)
```
───────────────────────────────────────────────────
jetson => 192.168.4.2      ▌ 0.5Mb
                        <=  ▌ 0.1Mb

TX: 0.5 Mbps  RX: 0.1 Mbps  TOTAL: 0.6 Mbps
───────────────────────────────────────────────────
```

### Mit Livestream @ 10 FPS
```
───────────────────────────────────────────────────
jetson => 192.168.4.2      ████████████ 6.2Mb
                        <=  ▌ 0.3Mb

TX: 6.2 Mbps  RX: 0.3 Mbps  TOTAL: 6.5 Mbps
───────────────────────────────────────────────────
```

### Mit Livestream @ 15 FPS (Network Limit)
```
───────────────────────────────────────────────────
jetson => 192.168.4.2      ████████████████ 9.1Mb
                        <=  ▌ 0.4Mb

TX: 9.1 Mbps  RX: 0.4 Mbps  TOTAL: 9.5 Mbps
───────────────────────────────────────────────────
```

---

## ✅ Success Criteria

Du hast das Test-Setup korrekt, wenn:

1. ✅ `iftop` zeigt Traffic nachdem Client verbunden ist
2. ✅ `htop` zeigt ~5-10% CPU ohne Recording
3. ✅ `htop` zeigt ~65-70% CPU mit Recording
4. ✅ Browser zeigt Livestream mit gewählter FPS
5. ✅ `iftop` TX matcht ungefähr die FPS (5 FPS ≈ 3 Mbps, 10 FPS ≈ 6 Mbps)

---

## 🎬 Los geht's!

**Terminal 3 starten:**
```bash
./test_livestream.sh
```

Oder manuell:
```bash
cd ~/Projects/Drone-Fieldtest
sudo ./build/apps/drone_web_controller/drone_web_controller
```

**Terminal 1+2 starten NACHDEM Controller läuft!**

**Viel Erfolg! 🚀**
