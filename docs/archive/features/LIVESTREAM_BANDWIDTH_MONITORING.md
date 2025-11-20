# Livestream Bandwidth Monitoring Guide
**Für Jetson Orin Nano + ZED 2i | WiFi AP Bandbreiten-Diagnose**

## 🎯 Problem: iftop Display ist verwirrend

iftop zeigt standardmäßig **3 verschiedene Werte** gleichzeitig, was sehr verwirrend ist:

```
10.42.0.1           => 192.168.1.100    3.00Mb  1.50Mb  750Kb
                    <=                   100Kb   50Kb   25Kb
```

**Was bedeuten diese Spalten?**
- **Spalte 1 (3.00Mb)**: Letzte 2 Sekunden
- **Spalte 2 (1.50Mb)**: Letzte 10 Sekunden  
- **Spalte 3 (750Kb)**: Letzte 40 Sekunden

**Das Problem:** Alle drei sind **Durchschnittswerte über verschiedene Zeiträume**, NICHT die aktuelle Bandbreite!

### CUM Display (noch verwirrender)

Wenn du `T` drückst, schaltet iftop zu **CUM** (Cumulative) Display um:

```
TX:             cum:   2s   10s   40s
10.42.0.1       15.3MB  750Kb  300Kb  150Kb
```

- **cum**: Gesamte Bytes seit Start von iftop
- **2s, 10s, 40s**: Durchschnitte wie oben

**Das ist NICHT hilfreich für Live-Monitoring!**

---

## ✅ Lösung 1: iftop mit besseren Einstellungen

### Beste iftop Konfiguration für Livestream:

```bash
sudo iftop -i wlP1p1s0 -B -P -t -s 2
```

**Parameter erklärt:**
- `-i wlP1p1s0`: WiFi AP Interface
- `-B`: **Bytes** statt Bits (75 KB/s statt 600 Kbps)
- `-P`: **Port-Nummern** anzeigen (8080 = Web UI)
- `-t`: **Text-Modus** ohne ncurses (läuft im Hintergrund)
- `-s 2`: Alle **2 Sekunden** aktualisieren

**Output Beispiel:**
```
   # Host name (port/service)            last 2s   last 10s   last 40s cumulative
------------------------------------------------------------------------------------
   1 10.42.0.1:8080                  =>   150.0KB     120.0KB     90.0KB     2.50MB
     192.168.1.100:54321             <=     5.0KB       4.0KB      3.0KB    50.0KB
```

**Lese die "last 2s" Spalte für Live-Werte!**

### Noch besser: iftop mit Filter nur auf Port 8080

```bash
sudo iftop -i wlP1p1s0 -B -P -f "port 8080"
```

Zeigt nur Traffic vom/zum Web Server (eliminiert System-Rauschen).

---

## ✅ Lösung 2: nethogs (Per-Prozess Monitoring)

**Vorteil:** Zeigt **nur den aktuellen Wert**, keine Durchschnitte!

### Installation:
```bash
sudo apt-get install -y nethogs
```

### Verwendung:
```bash
sudo nethogs wlP1p1s0 -d 2
```

**Parameter:**
- `wlP1p1s0`: WiFi AP Interface
- `-d 2`: Alle **2 Sekunden** aktualisieren (statt Standard 1s)

**Output Beispiel:**
```
  PID USER     PROGRAM              DEV       SENT      RECEIVED
 1234 angelo   drone_web_controller wlP1p1s0  150.3 KB/s   5.1 KB/s
    0 ?        unknown TCP          wlP1p1s0    0.0 KB/s   0.2 KB/s

  TOTAL                                        150.3 KB/s   5.3 KB/s
```

**Perfekt für Live-Monitoring!** Der **SENT** Wert ist die aktuelle Upload-Bandbreite (TX).

---

## ✅ Lösung 3: watch + /proc/net/dev (Einfachste Methode)

Wenn nethogs/iftop zu kompliziert sind:

```bash
watch -n 2 "cat /proc/net/dev | grep wlP1p1s0"
```

**Output Beispiel:**
```
wlP1p1s0:  52340123  45678  0  0  0  0  0  0  312456789  98765  0  0  0  0  0  0
           ^^^^^^^^                            ^^^^^^^^^
           RX bytes                            TX bytes
```

**Bandbreite berechnen:**
1. Notiere TX bytes (z.B. 312456789)
2. Warte 2 Sekunden
3. Notiere neuen TX bytes (z.B. 312756789)
4. Differenz: 312756789 - 312456789 = 300000 bytes
5. Bandbreite: 300000 bytes / 2 sec = **150 KB/s**

**Nachteil:** Manuelle Berechnung nötig, aber super zuverlässig!

---

## 📊 Erwartete Werte (Livestream @ verschiedene FPS)

| FPS  | JPEG Size | TX Bandwidth | iftop 2s Spalte | nethogs SENT |
|------|-----------|--------------|-----------------|--------------|
| 1    | ~75 KB    | **75 KB/s**  | ~75 KB          | ~75 KB/s     |
| 2    | ~75 KB    | **150 KB/s** | ~150 KB         | ~150 KB/s    |
| 5    | ~75 KB    | **375 KB/s** | ~375 KB         | ~375 KB/s    |
| 10   | ~75 KB    | **750 KB/s** | ~750 KB         | ~750 KB/s    |
| 15   | ~75 KB    | **1.1 MB/s** | ~1.1 MB         | ~1.1 MB/s    |

**Formel:** `Bandbreite = FPS × JPEG Size`

**Bei HD720 + JPEG Quality 85:** ~75 KB pro Frame

---

## 🧪 Komplettes Test-Setup (3 Terminals)

### Terminal 1: Starte Controller
```bash
cd ~/Projects/Drone-Fieldtest
sudo ./build/apps/drone_web_controller/drone_web_controller
```
**Warte auf:** `[MAIN] 🌐 Web Interface: http://10.42.0.1:8080`

### Terminal 2: Starte nethogs (EMPFOHLEN)
```bash
sudo nethogs wlP1p1s0 -d 2
```
**Schaue auf:** `drone_web_controller` SENT Wert (sollte 0 KB/s sein bis Client verbindet)

### Terminal 3: Alternative - iftop mit Bytes
```bash
sudo iftop -i wlP1p1s0 -B -P
```
**Schaue auf:** "last 2s" Spalte (ignoriere 10s/40s)

**Dann:**
1. Handy/Laptop verbinden mit WiFi: `DroneController` / `drone123`
2. Browser öffnen: `http://10.42.0.1:8080`
3. Tab "Livestream" öffnen
4. **Livestream aktivieren** ✓
5. **nethogs sollte jetzt ~150 KB/s SENT zeigen** (bei 2 FPS)

### Stress Test @ 10 FPS:
1. FPS-Dropdown auf **10 FPS (Stress Test)** ändern
2. nethogs sollte auf **~750 KB/s SENT** springen
3. CPU in htop sollte ~70-75% sein
4. Browser Bild sollte flüssig mit **10 Hz** aktualisieren

### Limit Test @ 15 FPS:
1. FPS-Dropdown auf **15 FPS (Network Test)** ändern
2. nethogs sollte **~1.1 MB/s SENT** zeigen
3. **Beobachte:** Funktioniert WiFi AP stabil? Timeout Fehler im Browser F12 Console?

---

## 🔍 Troubleshooting

### Problem: nethogs zeigt 0.0 KB/s SENT
**Ursachen:**
1. ✅ Controller läuft nicht → Starte in Terminal 1
2. ✅ Client nicht verbunden → Verbinde mit WiFi `DroneController`
3. ✅ Livestream nicht aktiviert → Klicke Checkbox im Browser
4. ✅ Falsches Interface → Prüfe mit `ip addr | grep wl` (sollte wlP1p1s0 sein)

### Problem: iftop zeigt nur RX Traffic (eingehend)
**Lösung:** Drücke `t` Taste um TX/RX Display zu wechseln:
- `t` einmal: Nur TX (Upload)
- `t` zweimal: TX + RX getrennt
- `t` dreimal: Zurück zu kombiniert

### Problem: iftop zeigt immer noch komische Werte
**Lösung:** Verwende nethogs! iftop ist zu kompliziert für Livestream-Monitoring.

### Problem: watch + /proc/net/dev zeigt nur wachsende Zahlen
**Das ist normal!** /proc/net/dev zeigt **cumulative bytes seit Boot**.
Du musst die Differenz zwischen zwei Messungen berechnen:
```bash
# Erste Messung
TX1=$(cat /proc/net/dev | grep wlP1p1s0 | awk '{print $10}')
sleep 2
# Zweite Messung  
TX2=$(cat /proc/net/dev | grep wlP1p1s0 | awk '{print $10}')
# Bandbreite
echo "scale=2; ($TX2 - $TX1) / 2 / 1024" | bc
# Output: 150.50 (KB/s)
```

---

## 📝 Empfohlenes Tool: nethogs

**Warum nethogs am besten ist:**
1. ✅ **Zeigt aktuelle Werte**, keine 10s/40s Durchschnitte
2. ✅ **Per-Prozess Ansicht**: Siehst genau `drone_web_controller`
3. ✅ **Einfach zu lesen**: SENT = Upload, RECEIVED = Download
4. ✅ **Auto-Update**: Kein manuelles Rechnen nötig

**Einziger Nachteil:** Benötigt sudo (genau wie iftop)

---

## 🎯 Quick Reference Card

### nethogs Kommando:
```bash
sudo nethogs wlP1p1s0 -d 2
```
**Schaue auf:** `drone_web_controller` **SENT** Spalte

### iftop Kommando (Alternative):
```bash
sudo iftop -i wlP1p1s0 -B -P
```
**Schaue auf:** **"last 2s"** Spalte (erste Zahlen-Spalte)

### Erwartete Werte @ verschiedenen FPS:
- **2 FPS**: ~150 KB/s
- **5 FPS**: ~375 KB/s  
- **10 FPS**: ~750 KB/s
- **15 FPS**: ~1.1 MB/s

### WiFi AP Maximum (Theorie):
- **2.4 GHz WiFi N**: ~30-50 Mbps real-world = **3.75-6.25 MB/s**
- **Unser Livestream @ 15 FPS**: 1.1 MB/s = **Nur 18-35% von Maximum**
- **Sollte kein Problem sein!**

---

## 🚀 Bonus: Automatisches Logging Script

Speichere als `monitor_bandwidth.sh`:

```bash
#!/bin/bash
# Livestream Bandwidth Monitor
# Usage: ./monitor_bandwidth.sh

if ! command -v nethogs &> /dev/null; then
    echo "❌ nethogs not installed! Run: sudo apt-get install nethogs"
    exit 1
fi

echo "📊 Monitoring drone_web_controller bandwidth on wlP1p1s0..."
echo "Press Ctrl+C to stop"
echo ""

sudo nethogs wlP1p1s0 -d 2 -t | grep -E "drone_web_controller|TOTAL"
```

**Verwendung:**
```bash
chmod +x monitor_bandwidth.sh
./monitor_bandwidth.sh > bandwidth_log.txt
```

**Output wird in `bandwidth_log.txt` gespeichert für spätere Analyse!**

---

## 📖 Zusammenfassung

1. **iftop ist verwirrend** → TX/RX untereinander, CUM = cumulative, 3 Zeiträume gleichzeitig
2. **nethogs ist besser** → Aktueller Wert, per-Prozess, einfach zu lesen
3. **Schaue immer auf die "last 2s" Spalte** bei iftop (ignoriere 10s/40s)
4. **Expected @ 2 FPS**: ~150 KB/s SENT
5. **Expected @ 10 FPS**: ~750 KB/s SENT
6. **WiFi AP kann easy 3-6 MB/s** → Livestream ist kein Bottleneck!

**Nächster Schritt:** Test mit neuem Controller (v1.4.3) - Segfault sollte jetzt behoben sein! 🎉
