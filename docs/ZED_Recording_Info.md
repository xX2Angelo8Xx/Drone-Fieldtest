# ZED 2i Video Recording - Technische Details

## Was wird aktuell aufgezeichnet?

### SVO2-Format (Stereo Video Only)
- **Eine Datei** enthält BEIDE Kamera-Streams (Left + Right)
- **Komprimiert** mit H264/H265 oder verlustfrei
- **Metadaten** inklusive: Kalibrierung, Timestamps, IMU-Daten
- **Auflösung**: HD720 @ 30fps (aktuell)

### Mögliche Auflösungen und Framerates:
```cpp
// HD720 @ 30fps (aktuell) - ~2-5 MB/s
// HD1080 @ 30fps         - ~8-15 MB/s  
// HD2K @ 15fps           - ~15-25 MB/s
// VGA @ 100fps           - ~3-8 MB/s
```

## SVO2-Dateien abspielen

### 1. ZED Explorer (GUI)
```bash
/usr/local/zed/tools/ZED_Explorer
# Dann: File -> Open SVO
```

### 2. Python SDK
```python
import pyzed.sl as sl

# Kamera mit SVO-Datei öffnen
init = sl.InitParameters()
init.set_from_svo_file("video.svo2")
zed = sl.Camera()
zed.open(init)

# Frame für Frame lesen
while True:
    if zed.grab() == sl.ERROR_CODE.SUCCESS:
        # Linkes Bild
        left_image = sl.Mat()
        zed.retrieve_image(left_image, sl.VIEW.LEFT)
        
        # Rechtes Bild  
        right_image = sl.Mat()
        zed.retrieve_image(right_image, sl.VIEW.RIGHT)
        
        # Depth Map
        depth_map = sl.Mat()
        zed.retrieve_measure(depth_map, sl.MEASURE.DEPTH)
```

### 3. C++ SDK
```cpp
sl::InitParameters init_params;
init_params.input.setFromSVOFile("video.svo2");
sl::Camera zed;
zed.open(init_params);

sl::Mat left_image, right_image, depth_map;
while (zed.grab() == sl::ERROR_CODE::SUCCESS) {
    zed.retrieveImage(left_image, sl::VIEW::LEFT);
    zed.retrieveImage(right_image, sl::VIEW::RIGHT);
    zed.retrieveMeasure(depth_map, sl::MEASURE::DEPTH);
}
```

## Video-Speicher-Strategien

### Option 1: SVO2 (Aktuell - EMPFOHLEN)
✅ **Vorteile:**
- Stereo + Depth in einer Datei
- Verlustfrei oder komprimiert
- Synchronisiert mit IMU-Daten
- Native ZED-Format

❌ **Nachteile:**  
- Nur mit ZED SDK abspielbar
- Größere Dateien

### Option 2: Separate Videos
✅ **Vorteile:**
- Standard MP4/AVI-Format
- Überall abspielbar

❌ **Nachteile:**
- Synchronisation schwieriger
- Depth-Map geht verloren
- Mehr Dateien zu verwalten

### Option 3: Hybrid
- SVO2 für Rohdaten
- Zusätzlich MP4 für Preview

## Performance-Limits

### Geschätzte Datenraten:
- **HD720@30fps + IMU**: ~5-10 MB/s
- **HD2K@15fps + IMU**: ~20-30 MB/s  
- **VGA@100fps + IMU**: ~10-15 MB/s

### USB 3.0 Stick Limits:
- **Schreibgeschwindigkeit**: 20-100 MB/s
- **Sollte ausreichen** für alle Modi

### Jetson Limits:
- **CPU**: Sollte für Kompression ausreichen
- **Storage**: Abhängig von USB-Stick