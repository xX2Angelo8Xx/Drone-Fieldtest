#!/usr/bin/env python3
"""
ZED Performance and AI Model Strategy Analyzer
Analysiert die optimale Auflösung für KI-Training basierend auf:
- Jetson Orin Nano Performance
- AI Model Komplexität 
- Flight Controller Anforderungen (30 FPS max)
"""

import time
import psutil
import os

class JetsonPerformanceAnalyzer:
    def __init__(self):
        self.gpu_available = self.check_gpu()
        self.cpu_cores = psutil.cpu_count()
        self.memory_gb = psutil.virtual_memory().total / (1024**3)
        
    def check_gpu(self):
        """Prüfe GPU-Verfügbarkeit"""
        try:
            import pycuda.driver as cuda
            cuda.init()
            return cuda.Device.count() > 0
        except:
            return False
    
    def estimate_fps_capability(self, resolution, model_complexity):
        """
        Schätze FPS-Fähigkeit basierend auf Auflösung und Modell-Komplexität
        
        resolution: 'VGA', 'HD720', 'HD1080', 'HD2K'
        model_complexity: 'light', 'medium', 'heavy'
        """
        
        # Basis-FPS ohne AI (gemessen)
        base_fps = {
            'VGA': 100,
            'HD720': 60, 
            'HD1080': 30,
            'HD2K': 15
        }
        
        # AI Overhead Faktoren
        ai_overhead = {
            'light': 0.8,   # 20% Overhead (z.B. YOLOv5n)
            'medium': 0.5,  # 50% Overhead (z.B. YOLOv5m) 
            'heavy': 0.3    # 70% Overhead (z.B. YOLOv5x)
        }
        
        # GPU Boost (wenn verfügbar)
        gpu_boost = 1.5 if self.gpu_available else 1.0
        
        estimated_fps = base_fps[resolution] * ai_overhead[model_complexity] * gpu_boost
        
        return min(estimated_fps, 30)  # Flight Controller Limit
    
    def get_recommendations(self):
        """Generiere Empfehlungen für verschiedene Use Cases"""
        
        scenarios = {
            'training_data_collection': {
                'priority': 'highest_quality',
                'recommended': 'HD1080',
                'fps': 30,
                'reason': 'Maximale Datenqualität für Training'
            },
            'real_time_light_model': {
                'priority': 'speed',
                'recommended': 'HD720', 
                'fps': 30,
                'reason': 'Optimale Balance für leichte AI-Modelle'
            },
            'real_time_heavy_model': {
                'priority': 'speed',
                'recommended': 'VGA',
                'fps': 30, 
                'reason': 'Sicherheit bei schweren AI-Modellen'
            },
            'development_testing': {
                'priority': 'flexibility',
                'recommended': 'HD720',
                'fps': 30,
                'reason': 'Guter Kompromiss für Entwicklung'
            }
        }
        
        return scenarios

def print_analysis():
    analyzer = JetsonPerformanceAnalyzer()
    
    print("🚁 JETSON ORIN NANO - AI DRONE STRATEGY")
    print("=" * 50)
    print(f"GPU Available: {analyzer.gpu_available}")
    print(f"CPU Cores: {analyzer.cpu_cores}")
    print(f"Memory: {analyzer.memory_gb:.1f} GB")
    print()
    
    print("📊 ESTIMATED AI PERFORMANCE")
    print("=" * 50)
    
    resolutions = ['VGA', 'HD720', 'HD1080']
    models = ['light', 'medium', 'heavy']
    
    for res in resolutions:
        print(f"\n{res}:")
        for model in models:
            fps = analyzer.estimate_fps_capability(res, model)
            status = "✅" if fps >= 30 else "⚠️" if fps >= 15 else "❌"
            print(f"  {model:6} Model: {fps:4.1f} FPS {status}")
    
    print("\n🎯 EMPFEHLUNGEN")
    print("=" * 50)
    
    recs = analyzer.get_recommendations()
    for scenario, info in recs.items():
        print(f"\n{scenario.replace('_', ' ').title()}:")
        print(f"  Resolution: {info['recommended']}")
        print(f"  Target FPS: {info['fps']}")
        print(f"  Reason: {info['reason']}")

if __name__ == "__main__":
    print_analysis()