# 📝 AUFNAHMEZEIT ANPASSEN - COMPLETE GUIDE

## ✅ **ERFOLG**: Profile sind bereits angepasst und getestet!

### **Aktuelle Profile** (nach deinen Änderungen):

| Profil | Auflösung | FPS | Dauer | Status |
|--------|-----------|-----|-------|---------|
| `quick_test` | HD720 | 30 | **5s** | ✅ Getestet |
| `development` | HD720 | 60 | **10s** | ✅ Angepasst |
| `realtime_light` | HD720 | 15 | **30s** | ✅ Feldtauglich |
| `realtime_30fps` | HD720 | 30 | **30s** | ✅ Frame-Drop-Free |
| `realtime_heavy` | VGA | 100 | **30s** | ✅ High-Speed |
| `ultra_quality` | HD2K | 15 | **30s** | ✅ Max Resolution |
| `training` | HD1080 | 30 | **120s** | ✅ Extended (2 Min) |
| `long_mission` | HD720 | 30 | **300s** | ✅ Long Mission (5 Min) |

## 🔧 **WIE DU ZEITEN ÄNDERST**

### **1. Source Code editieren**:
```bash
nano /home/angelo/Projects/Drone-Fieldtest/apps/smart_recorder/main.cpp
```

### **2. Profile-Definitionen finden** (Zeile ~32-70):
```cpp
std::map<std::string, RecordingProfile> getRecordingProfiles() {
    return {
        {"quick_test", {
            RecordingMode::HD720_30FPS,
            5,   // ← HIER die Sekunden ändern
            "Quick Function Test",
            "Für schnelle Funktionstests"
        }},
        // ... weitere Profile
    };
}
```

### **3. Gewünschte Zeit einsetzen**:
- **5** = 5 Sekunden
- **30** = 30 Sekunden  
- **120** = 2 Minuten
- **300** = 5 Minuten
- **600** = 10 Minuten

### **4. Build & Test**:
```bash
cd /home/angelo/Projects/Drone-Fieldtest
./scripts/build.sh
./build/apps/smart_recorder/smart_recorder [profil_name]
```

## 🎯 **PRAKTISCHE BEISPIELE**

### **Neue Zeiten setzen**:
```cpp
// 15 Sekunden für schnelle Tests
{"fast_test", {
    RecordingMode::HD720_30FPS,
    15,  // 15 Sekunden
    "Fast Test Mode",
    "Für 15-Sekunden Tests"
}},

// 8 Minuten für typische Drohnenflüge  
{"drone_mission", {
    RecordingMode::HD720_30FPS,
    480,  // 8 Minuten (8 × 60 = 480)
    "Standard Drone Mission",
    "Für 8-Minuten Drohnenflüge"
}},

// 30 Minuten für sehr lange Aufnahmen
{"ultra_long", {
    RecordingMode::HD720_15FPS,  // Niedrigere FPS für weniger Daten
    1800,  // 30 Minuten (30 × 60 = 1800)
    "Ultra Long Recording",
    "Für sehr lange Feldstudien"
}},
```

## 📊 **DATEIGRÖSSE CALCULATOR**

### **HD720@30FPS**: ~25 MB/s
- **5s**: ~125 MB
- **30s**: ~750 MB  
- **2 Min**: ~3 GB
- **5 Min**: ~7.5 GB
- **10 Min**: ~15 GB

### **HD1080@30FPS**: ~40 MB/s
- **30s**: ~1.2 GB
- **2 Min**: ~4.8 GB
- **5 Min**: ~12 GB

### **HD720@15FPS**: ~13 MB/s  
- **30s**: ~390 MB
- **5 Min**: ~3.9 GB
- **30 Min**: ~23.4 GB

## ⚠️ **USB-STICK LIMITS**

### **32GB USB-Stick**:
- HD720@30FPS: Max **20 Minuten**
- HD720@15FPS: Max **40 Minuten**
- HD1080@30FPS: Max **13 Minuten**

### **64GB USB-Stick**:
- HD720@30FPS: Max **40 Minuten**
- HD720@15FPS: Max **80 Minuten**  
- HD1080@30FPS: Max **26 Minuten**

## 🚀 **WORKFLOW ZUSAMMENFASSUNG**

### **Schritt 1**: Zeit anpassen
```cpp
// In main.cpp, Zeile ~40:
{"mein_profil", {
    RecordingMode::HD720_30FPS,
    180,  // 3 Minuten
    "Mein Custom Profil",
    "Beschreibung"
}},
```

### **Schritt 2**: Build
```bash
./scripts/build.sh
```

### **Schritt 3**: Test
```bash
./build/apps/smart_recorder/smart_recorder mein_profil
```

### **Schritt 4**: Autostart anpassen (optional)
```bash
# In autostart.sh:
"$EXECUTABLE_PATH" mein_profil
```

## ✅ **FERTIG!**

**Du kannst jetzt beliebige Aufnahmezeiten einstellen:**
- ✅ **Code anpassen** → Build → Test → Deploy
- ✅ **Alle Zeiten möglich**: 1 Sekunde bis 30+ Minuten  
- ✅ **Automatische Datei-Verwaltung**: System handelt alles
- ✅ **USB-Stick Support**: Bis zur verfügbaren Kapazität

**Dein System ist jetzt vollständig konfigurierbar!** 🎉