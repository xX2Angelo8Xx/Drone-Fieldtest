# 🚁 DRONE AI SYSTEM - STRATEGISCHE EMPFEHLUNGEN

## 📊 **Performance-Analyse Zusammenfassung**

Basierend auf den Tests mit dem Jetson Orin Nano und ZED 2i:

### **✅ EMPFOHLENE KONFIGURATION FÜR DEIN KI-SYSTEM:**

#### **1. Training Data Collection** 
```bash
./smart_recorder training
```
- **Auflösung**: HD1080@30fps (1920×1080)
- **Dauer**: 60 Sekunden
- **Datenrate**: ~29 MB/s
- **Zweck**: Beste Datenqualität für AI-Model Training

#### **2. Real-time AI Inference (EMPFOHLEN)** 
```bash
./smart_recorder realtime_light  
```
- **Auflösung**: HD720@30fps (1280×720) 
- **Dauer**: 30 Sekunden
- **Datenrate**: ~26 MB/s
- **Zweck**: Optimale Balance für Echtzeit-KI

#### **3. Heavy AI Models** 
```bash
./smart_recorder realtime_heavy
```
- **Auflösung**: VGA@100fps (672×376)
- **Dauer**: 30 Sekunden  
- **Datenrate**: ~15 MB/s
- **Zweck**: Sicherheit bei schweren AI-Modellen

## 🎯 **KONKRETE EMPFEHLUNG FÜR DICH:**

### **Phase 1: Training Data Collection**
- **Verwende**: `training` Profil (HD1080@30fps)
- **Sammle Daten** in verschiedenen Flugbedingungen
- **Speichere** sowohl Video als auch IMU-Daten

### **Phase 2: Model Development** 
- **Starte mit**: `realtime_light` Profil (HD720@30fps)
- **Teste** verschiedene YOLO-Varianten:
  - YOLOv5n (nano) - sehr schnell
  - YOLOv5s (small) - gute Balance
  - YOLOv8n/s - neueste Version

### **Phase 3: Production System**
- **Wenn Model < 50ms Inferenz**: HD720@30fps ✅
- **Wenn Model > 50ms Inferenz**: VGA@100fps (downsampled)
- **Flight Controller**: Maximal 30 FPS sowieso ausreichend!

## 🔧 **TECHNICAL SPECS**

| Profil | Auflösung | FPS | AI-Ready | Flight Controller | Speicher/Min |
|--------|-----------|-----|----------|-------------------|--------------|
| **realtime_light** | 1280×720 | 30 | ✅ | ✅ | ~1.5 GB |
| **training** | 1920×1080 | 30 | 🔄 | ✅ | ~1.7 GB |
| **realtime_heavy** | 672×376 | 100→30 | ✅ | ✅ | ~0.9 GB |

## 🚀 **SVO2 DATEIEN VERWENDEN**

### **Abspielen:**
```bash
ZED_Explorer  # GUI-Tool
# oder
python3 -c "
import pyzed.sl as sl
init = sl.InitParameters()
init.set_from_svo_file('video.svo2')  
zed = sl.Camera()
zed.open(init)
"
```

### **Für AI-Training konvertieren:**
- SVO2 → Individual Frames (PNG/JPG)
- Depth Maps extrahieren
- IMU-Daten synchronisieren

## 💡 **FINALE EMPFEHLUNG:**

**Starte mit `realtime_light` (HD720@30fps)**
- ✅ Perfekte Balance zwischen Qualität und Performance
- ✅ 30 FPS matcht Flight Controller Requirements  
- ✅ Genug Auflösung für robuste Objekterkennung
- ✅ Schnell genug für Echtzeit-Inferenz
- ✅ Moderate Speicheranforderungen

**Du kannst jederzeit zwischen Profilen wechseln**, je nach Model-Komplexität!