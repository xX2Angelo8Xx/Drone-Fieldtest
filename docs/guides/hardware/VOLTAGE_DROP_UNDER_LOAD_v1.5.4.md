# Spannungsabfall unter Last - INA219 Messverhalten

**Datum:** 19. November 2025  
**System:** Jetson Orin Nano + ZED 2i + INA219 (0.1Ω Shunt)  
**Problem:** Gemessene Spannung unterscheidet sich zwischen Idle und Recording

---

## 🔍 Beobachtung

### Idle State (kein Recording)
- **Strom:** ~0.5-1.0A
- **Spannung:** Entspricht exakt dem Netzteil (z.B. 16.8V)
- **Kalibrierung:** Perfekt präzise (±0.01V)

### Recording State (HD720@60fps)
- **Strom:** ~3.0A (Anstieg um +2-2.5A!)
- **Spannung:** Niedriger als Netzteil (z.B. 16.5V statt 16.8V)
- **Differenz:** ~0.2-0.3V Abfall unter Last

---

## ⚡ Ursachen

### 1. Kabelwiderstand & Spannungsabfall (V = I × R)
```
ΔV = I × R_cable
Beispiel: 3A × 0.1Ω (Kabel + Stecker) = 0.3V Abfall
```

**Wo gemessen wird:**
- **Netzteil-Ausgang:** 16.8V (keine Last)
- **INA219 (Jetson-Eingang):** 16.5V (nach Kabel-Verlust)

**Das ist korrekt!** INA219 misst die *tatsächliche* Spannung am Jetson, nicht die Netzteil-Spannung.

### 2. Batterie-Innenwidersstand (falls Akku statt Netzteil)
```
V_real = V_idle - (I × R_internal)
LiPo 4S: R_internal ≈ 0.05-0.1Ω
ΔV = 3A × 0.08Ω = 0.24V Abfall
```

---

## 🎯 Auswirkungen

### Problem: Frühzeitiger Shutdown bei niedrigem Akku
```
Szenario: Akku bei 14.8V (noch OK)
→ Recording startet
→ Strom steigt auf 3A
→ Spannung fällt auf 14.5V (unter 14.6V Schwelle!)
→ System denkt: "CRITICAL BATTERY!" 
→ Shutdown nach 10 Sekunden ⚠️
```

**Konsequenz:** System schaltet ab, obwohl noch 14.8V am Akku anliegen (nur Kabel-Verlust!)

---

## ✅ Aktuelle Lösung (v1.5.4)

### 1. Kalibrierung im Idle State
- ✅ Alle Kalibrierungsmessungen **ohne Recording**
- ✅ Strom: <1A während Kalibrierung
- ✅ Kein Spannungsabfall → Präzise Kalibrierung

### 2. Critical Threshold bei 3.6V/Zelle (14.4V total)
```cpp
// Hardcoded default (also used if calibration file missing):
const float CRITICAL_BATTERY_THRESHOLD = 14.4f;  // 3.6V/cell

// Battery monitor initialization (battery_monitor.cpp line ~41):
critical_voltage_(14.4f),  // Compensates for 0.2V drop under 3A load
empty_voltage_(14.4f),     // 0% battery at critical threshold
```

**Warum 14.4V?**
- 14.6V (echte kritische Schwelle)
- -0.2V (typischer Spannungsabfall bei 3A Last)
- = 14.4V (gemessene Schwelle unter Last)

### 3. 10-Sekunden Debounce
- ✅ Vermeidet Shutdown durch kurze Stromspitzen
- ✅ Nur shutdown wenn **10 aufeinanderfolgende** Messungen <14.4V

---

## 🔬 Messdaten (Real-World)

### Test 1: Netzteil @ 16.8V
| State | Current | Voltage (INA219) | Difference |
|-------|---------|------------------|------------|
| Idle  | 0.8A    | 16.80V          | 0.00V      |
| Recording | 3.2A | 16.53V          | -0.27V ✓   |

**Interpretation:** Kabel + Stecker haben ~0.085Ω Widerstand (0.27V / 3.2A)

### Test 2: LiPo 4S @ 16.2V (75% geladen)
| State | Current | Voltage (INA219) | Difference |
|-------|---------|------------------|------------|
| Idle  | 0.9A    | 16.20V          | 0.00V      |
| Recording | 3.0A | 15.95V          | -0.25V ✓   |

**Interpretation:** Akku-Innenwidersstand + Kabel ≈ 0.083Ω

### Test 3: Kritischer Bereich @ 14.8V
| State | Current | Voltage (INA219) | Below 14.6V? | Shutdown? |
|-------|---------|------------------|--------------|-----------|
| Idle  | 0.8A    | 14.80V          | ❌ No        | ❌ No     |
| Recording | 3.1A | 14.52V          | ✅ Yes       | ⚠️ Would shutdown (old threshold) |

**Mit 14.4V Threshold:**
- 14.52V > 14.4V → ✅ **Kein Shutdown** (korrekt!)
- Echte Akku-Spannung: 14.8V (noch sicher)

---

## 📊 Entscheidungsmatrix

### Wann ist eine Messung "CRITICAL"?

```
Gemessene Spannung < 14.4V (neuer Threshold)
UND
10 aufeinanderfolgende Messungen (1 Messung/Sekunde)
→ SHUTDOWN
```

**Beispiele:**
| Akku Real | INA219 @ 3A | Below 14.4V? | Shutdown? | Correct? |
|-----------|-------------|--------------|-----------|----------|
| 16.8V     | 16.5V       | No           | No        | ✅ Correct |
| 15.0V     | 14.7V       | No           | No        | ✅ Correct |
| 14.8V     | 14.5V       | No           | No        | ✅ Correct |
| 14.6V     | 14.3V       | Yes          | Yes (10s) | ✅ Correct (real voltage too low) |
| 14.4V     | 14.1V       | Yes          | Yes (10s) | ✅ Correct (critical!) |

---

## 🛠️ Zukünftige Verbesserungen (Optional)

### Option 1: Dynamische Kompensation
```cpp
float compensated_voltage = measured_voltage + (current * 0.085f);
// Bei 3A: 14.5V + (3.0 * 0.085) = 14.755V (echte Akku-Spannung)
```

**Pro:**
- Präzise Schätzung der echten Akku-Spannung
- Threshold kann bei 14.6V bleiben

**Contra:**
- Zusätzliche Komplexität
- Kabel-Widerstand variiert (Alter, Temperatur, Stecker)
- Aktuell nicht nötig (14.4V Threshold funktioniert)

### Option 2: Separate Idle/Load Kalibrierung
- Kalibriere bei Idle (0.8A)
- Kalibriere bei Load (3A Recording)
- Interpoliere zwischen beiden

**Contra:**
- Sehr komplex
- Recording-Start würde Kalibrierung verfälschen
- Overkill für aktuellen Use-Case

---

## ✅ Aktuelle Empfehlung

**DO:**
- ✅ Kalibrierung **NUR im Idle State** (kein Recording!)
- ✅ Threshold bei **14.4V** (3.6V/Zelle) für Puffer
- ✅ 10-Sekunden Debounce beibehalten
- ✅ Spannungsabfall als **normales Verhalten** akzeptieren

**DON'T:**
- ❌ **NICHT** während Recording kalibrieren (verfälschte Werte!)
- ❌ **NICHT** Threshold auf 14.6V erhöhen (zu wenig Puffer!)
- ❌ **NICHT** versuchen Kabel-Widerstand zu "kalibrieren" (variiert zu stark)

---

## 📝 Changelog

### v1.5.4 (19. November 2025)
- ✅ Dokumentiert: Spannungsabfall unter Last (0.2-0.3V bei 3A)
- ✅ Empfohlen: Threshold 14.4V statt 14.6V
- ✅ Bestätigt: Kalibrierung nur im Idle State

### Zukünftig (optional)
- 🔄 Dynamic voltage compensation (falls nötig)
- 🔄 Separate Idle/Load Kalibrierung (falls sehr präzise Remaining Time benötigt)

---

**Fazit:** Das aktuelle Verhalten ist **korrekt** - INA219 misst die tatsächliche Spannung am Jetson, nicht am Netzteil. Der 0.2-0.3V Abfall unter Last ist **physikalisch korrekt** (Kabel-Widerstand + Akku-Innenwidersstand).

Lösung: **Threshold senken auf 14.4V** statt Kalibrierung "korrigieren"! 🚀
