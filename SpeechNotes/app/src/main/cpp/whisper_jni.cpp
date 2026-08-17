#include <jni.h>
#include <android/log.h>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <algorithm>
#include "whisper.h"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "SpeechNotesJNI", __VA_ARGS__)

static std::string jstr(JNIEnv *env, jstring s) {
    if (!s) return {};
    const char *p = env->GetStringUTFChars(s, nullptr);
    std::string out = p ? p : "";
    if (p) env->ReleaseStringUTFChars(s, p);
    return out;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_chatgpt_speechnotes_WhisperBridge_transcribePcm16(
        JNIEnv *env, jclass,
        jstring modelPathJ,
        jstring pcmPathJ,
        jstring languageJ,
        jint threadsJ) {
    std::string modelPath = jstr(env, modelPathJ);
    std::string pcmPath = jstr(env, pcmPathJ);
    std::string language = jstr(env, languageJ);

    std::ifstream f(pcmPath, std::ios::binary | std::ios::ate);
    if (!f) return env->NewStringUTF("[PCM-Datei konnte nicht geöffnet werden]");
    std::streamsize bytes = f.tellg();
    f.seekg(0, std::ios::beg);
    if (bytes <= 1) return env->NewStringUTF("");

    std::vector<int16_t> pcm16((size_t)bytes / sizeof(int16_t));
    if (!f.read(reinterpret_cast<char *>(pcm16.data()), (std::streamsize)(pcm16.size() * sizeof(int16_t)))) {
        return env->NewStringUTF("[PCM-Datei konnte nicht gelesen werden]");
    }
    std::vector<float> pcmf32(pcm16.size());
    for (size_t i = 0; i < pcm16.size(); ++i) pcmf32[i] = pcm16[i] / 32768.0f;

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;
    cparams.flash_attn = false;
    whisper_context *ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
    if (!ctx) {
        LOGE("Could not load model: %s", modelPath.c_str());
        return env->NewStringUTF("[Whisper-Modell konnte nicht geladen werden]");
    }

    whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    p.n_threads = std::max(1, (int)threadsJ);
    p.translate = false;
    p.print_special = false;
    p.print_progress = false;
    p.print_realtime = false;
    p.print_timestamps = false;
    p.no_context = false;
    p.single_segment = false;
    if (!language.empty()) p.language = language.c_str();

    int rc = whisper_full(ctx, p, pcmf32.data(), (int)pcmf32.size());
    if (rc != 0) {
        whisper_free(ctx);
        return env->NewStringUTF("[Whisper-Inferenz fehlgeschlagen]");
    }

    std::string text;
    int n = whisper_full_n_segments(ctx);
    for (int i = 0; i < n; ++i) {
        const char *seg = whisper_full_get_segment_text(ctx, i);
        if (seg) text += seg;
    }
    whisper_free(ctx);
    return env->NewStringUTF(text.c_str());
}
