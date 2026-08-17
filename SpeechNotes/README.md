# Speech Notes

A fully offline Android dictation prototype built in a single ChatGPT conversation.

- 16 kHz mono PCM16 capture
- Android microphone foreground service; recording survives screen-off/background
- Whisper.cpp on-device transcription
- Bundled multilingual Whisper Tiny/Base/Small Q5_1 models
- Optional WAV persistence
- Local transcript history, word totals and inference metrics
- arm64-v8a build targeted at modern Android phones

The CI workflow downloads the upstream model files and whisper.cpp source at build time and packages everything into the signed APK. The installed application performs no network downloads.
