#include <jni.h>
#include <android/log.h>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <sstream>
#include "whisper.h"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "SpeechNotesJNI", __VA_ARGS__)

static std::mutex g_mutex;
static whisper_context * g_ctx = nullptr;
static std::string g_model_path;
static long long g_model_load_ms = 0;

static std::string jstr(JNIEnv *env, jstring s) {
    if (!s) return {};
    const char *p = env->GetStringUTFChars(s, nullptr);
    std::string out = p ? p : "";
    if (p) env->ReleaseStringUTFChars(s, p);
    return out;
}

static long long elapsed_ms(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_chatgpt_speechnotes_WhisperBridge_loadModel(
        JNIEnv *env, jclass, jstring modelPathJ) {
    const std::string modelPath = jstr(env, modelPathJ);
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_ctx && g_model_path == modelPath) return 0;

    if (g_ctx) {
        whisper_free(g_ctx);
        g_ctx = nullptr;
        g_model_path.clear();
        g_model_load_ms = 0;
    }

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;
    cparams.flash_attn = false;
    auto start = std::chrono::steady_clock::now();
    g_ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
    g_model_load_ms = elapsed_ms(start);
    if (!g_ctx) {
        LOGE("Could not load model: %s", modelPath.c_str());
        g_model_load_ms = 0;
        return -1;
    }
    g_model_path = modelPath;
    return (jlong) g_model_load_ms;
}

extern "C" JNIEXPORT void JNICALL
Java_com_chatgpt_speechnotes_WhisperBridge_unloadModel(JNIEnv *, jclass) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_ctx) whisper_free(g_ctx);
    g_ctx = nullptr;
    g_model_path.clear();
    g_model_load_ms = 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_chatgpt_speechnotes_WhisperBridge_isModelLoaded(
        JNIEnv *env, jclass, jstring modelPathJ) {
    const std::string modelPath = jstr(env, modelPathJ);
    std::lock_guard<std::mutex> lock(g_mutex);
    return (g_ctx && g_model_path == modelPath) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_chatgpt_speechnotes_WhisperBridge_currentModelLoadMs(JNIEnv *, jclass) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return (jlong) g_model_load_ms;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_chatgpt_speechnotes_WhisperBridge_transcribeLoadedRaw(
        JNIEnv *env, jclass,
        jstring pcmPathJ,
        jstring languageJ,
        jstring initialPromptJ,
        jint threadsJ,
        jboolean noContextJ) {
    const std::string pcmPath = jstr(env, pcmPathJ);
    const std::string language = jstr(env, languageJ);
    const std::string initialPrompt = jstr(env, initialPromptJ);

    auto pcmStart = std::chrono::steady_clock::now();
    std::ifstream f(pcmPath, std::ios::binary | std::ios::ate);
    if (!f) return env->NewStringUTF("[PCM-Datei konnte nicht geöffnet werden]");
    std::streamsize bytes = f.tellg();
    f.seekg(0, std::ios::beg);
    if (bytes <= 1) return env->NewStringUTF("");

    std::vector<int16_t> pcm16((size_t) bytes / sizeof(int16_t));
    if (!f.read(reinterpret_cast<char *>(pcm16.data()),
                (std::streamsize) (pcm16.size() * sizeof(int16_t)))) {
        return env->NewStringUTF("[PCM-Datei konnte nicht gelesen werden]");
    }
    std::vector<float> pcmf32(pcm16.size());
    for (size_t i = 0; i < pcm16.size(); ++i) pcmf32[i] = pcm16[i] / 32768.0f;
    const long long pcmMs = elapsed_ms(pcmStart);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ctx) return env->NewStringUTF("[Kein Whisper-Modell geladen]");

    whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    p.n_threads = std::max(1, (int) threadsJ);
    p.translate = false;
    p.print_special = false;
    p.print_progress = false;
    p.print_realtime = false;
    p.print_timestamps = false;
    p.no_context = (noContextJ == JNI_TRUE);
    p.single_segment = false;
    if (!language.empty()) p.language = language.c_str();
    if (!initialPrompt.empty()) p.initial_prompt = initialPrompt.c_str();

    whisper_reset_timings(g_ctx);
    auto whisperStart = std::chrono::steady_clock::now();
    const int rc = whisper_full(g_ctx, p, pcmf32.data(), (int) pcmf32.size());
    const long long whisperMs = elapsed_ms(whisperStart);
    if (rc != 0) return env->NewStringUTF("[Whisper-Inferenz fehlgeschlagen]");

    std::string text;
    const int n = whisper_full_n_segments(g_ctx);
    for (int i = 0; i < n; ++i) {
        const char *seg = whisper_full_get_segment_text(g_ctx, i);
        if (seg) text += seg;
    }

    whisper_timings *t = whisper_get_timings(g_ctx);
    const long long melMs         = t ? (long long) t->mel_ms : 0;
    const long long nativeTotalMs = t ? (long long) t->total_ms : 0;

    const long long encodeMs      = t ? (long long) t->encode_ms : 0;
    const long long encodeTotalMs = t ? (long long) t->encode_total_ms : 0;
    const long long encodeN       = t ? (long long) t->encode_n : 0;
    const long long decodeMs      = t ? (long long) t->decode_ms : 0;
    const long long decodeTotalMs = t ? (long long) t->decode_total_ms : 0;
    const long long decodeN       = t ? (long long) t->decode_n : 0;
    const long long sampleMs      = t ? (long long) t->sample_ms : 0;
    const long long sampleTotalMs = t ? (long long) t->sample_total_ms : 0;
    const long long sampleN       = t ? (long long) t->sample_n : 0;
    const long long batchMs       = t ? (long long) t->batchd_ms : 0;
    const long long batchTotalMs  = t ? (long long) t->batchd_total_ms : 0;
    const long long batchN        = t ? (long long) t->batchd_n : 0;
    const long long promptMs      = t ? (long long) t->prompt_ms : 0;
    const long long promptTotalMs = t ? (long long) t->prompt_total_ms : 0;
    const long long promptN       = t ? (long long) t->prompt_n : 0;

    std::ostringstream out;
    out << "pcm=" << pcmMs
        << ";whisper=" << whisperMs
        << ";native_total=" << nativeTotalMs
        << ";mel=" << melMs
        << ";encode=" << encodeMs
        << ";encode_total=" << encodeTotalMs
        << ";encode_n=" << encodeN
        << ";decode=" << decodeMs
        << ";decode_total=" << decodeTotalMs
        << ";decode_n=" << decodeN
        << ";sample=" << sampleMs
        << ";sample_total=" << sampleTotalMs
        << ";sample_n=" << sampleN
        << ";batch=" << batchMs
        << ";batch_total=" << batchTotalMs
        << ";batch_n=" << batchN
        << ";prompt=" << promptMs
        << ";prompt_total=" << promptTotalMs
        << ";prompt_n=" << promptN
        << "\n__SN_TEXT__\n" << text;
    delete t;
    return env->NewStringUTF(out.str().c_str());
}
