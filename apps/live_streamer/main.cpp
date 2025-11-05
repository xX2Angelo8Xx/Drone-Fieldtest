#include "../../common/streaming/zed_streamer.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>

// Global signal handler
ZEDLiveStreamer* g_streamer = nullptr;
bool g_running = true;

void signalHandler(int signal) {
    std::cout << "\n[STREAM-APP] Signal received: " << signal << std::endl;
    g_running = false;
    if (g_streamer) {
        g_streamer->stopStream();
    }
}

void simulateTelemetryUpdates(ZEDLiveStreamer* streamer) {
    float battery = 100.0f;
    float altitude = 0.0f;
    float speed = 0.0f;
    int update_count = 0;
    
    while (g_running) {
        // Simulate drone telemetry changes
        battery = std::max(0.0f, battery - 0.1f); // Battery slowly drains
        altitude = 50.0f + 20.0f * sin(update_count * 0.1f); // Oscillating altitude
        speed = 15.0f + 5.0f * cos(update_count * 0.05f); // Varying speed
        
        std::string gps = "34.0522°N, 118.2437°W"; // Fixed GPS for demo
        
        streamer->updateTelemetry(battery, altitude, speed, gps);
        
        update_count++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(int argc, char* argv[]) {
    std::cout << "🚁 ZED LIVE STREAMER - AI DEPLOYMENT MODE" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    // Parse command line arguments
    std::string rtmp_url = "rtmp://localhost:1935/live/drone";
    StreamQuality quality = StreamQuality::MEDIUM_QUALITY;
    
    if (argc > 1) {
        rtmp_url = argv[1];
        std::cout << "Using custom RTMP URL: " << rtmp_url << std::endl;
    }
    
    if (argc > 2) {
        int quality_int = std::stoi(argv[2]);
        if (quality_int >= 0 && quality_int <= 2) {
            quality = static_cast<StreamQuality>(quality_int);
        }
    }
    
    // Quality description
    std::string quality_desc;
    switch (quality) {
        case StreamQuality::LOW_BANDWIDTH: quality_desc = "LOW (1.5 Mbps)"; break;
        case StreamQuality::MEDIUM_QUALITY: quality_desc = "MEDIUM (3 Mbps)"; break;
        case StreamQuality::HIGH_QUALITY: quality_desc = "HIGH (6 Mbps)"; break;
    }
    
    std::cout << "Stream Quality: " << quality_desc << std::endl;
    std::cout << "RTMP URL: " << rtmp_url << std::endl;
    std::cout << "=========================================" << std::endl;
    
    // Install signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Initialize streamer
    ZEDLiveStreamer streamer;
    g_streamer = &streamer;
    
    if (!streamer.init(quality)) {
        std::cerr << "Failed to initialize ZED streamer" << std::endl;
        return 1;
    }
    
    // Enable depth overlay for demonstration
    streamer.enableDepthOverlay(true);
    
    // Start streaming
    if (!streamer.startStream(rtmp_url)) {
        std::cerr << "Failed to start streaming" << std::endl;
        return 1;
    }
    
    // Start telemetry simulation thread
    std::thread telemetry_thread(simulateTelemetryUpdates, &streamer);
    
    std::cout << "\n🎥 LIVE STREAMING ACTIVE" << std::endl;
    std::cout << "Press Ctrl+C to stop streaming..." << std::endl;
    std::cout << "\nReal-time stats:" << std::endl;
    
    // Main monitoring loop
    while (g_running && streamer.isStreaming()) {
        // Display real-time performance stats
        std::cout << "\r[STATS] FPS: " << std::fixed << std::setprecision(1) 
                  << streamer.getCurrentFPS() 
                  << " | Bitrate: " << streamer.getStreamBitrate() << " Mbps"
                  << " | Dropped: " << streamer.getDroppedFrames() << " frames    ";
        std::cout.flush();
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "\n\n[STREAM-APP] Shutting down..." << std::endl;
    
    // Cleanup
    g_running = false;
    streamer.stopStream();
    
    if (telemetry_thread.joinable()) {
        telemetry_thread.join();
    }
    
    std::cout << "[STREAM-APP] Shutdown complete" << std::endl;
    return 0;
}

/*
USAGE EXAMPLES:

1. Basic streaming (default quality):
   ./live_streamer

2. Custom RTMP server:
   ./live_streamer rtmp://your-server.com:1935/live/drone

3. Low bandwidth mode for cellular:
   ./live_streamer rtmp://server.com:1935/live/drone 0

4. High quality for local network:
   ./live_streamer rtmp://192.168.1.100:1935/live/drone 2

BANDWIDTH REQUIREMENTS:
- Quality 0 (LOW): ~1.5 Mbps upload
- Quality 1 (MEDIUM): ~3 Mbps upload  
- Quality 2 (HIGH): ~6 Mbps upload

STREAMING SERVERS:
- Local testing: nginx with rtmp module
- Cloud: YouTube Live, Twitch, custom RTMP server
- Low latency: WebRTC server for <100ms latency
*/