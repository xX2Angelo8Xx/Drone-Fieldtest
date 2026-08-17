from pathlib import Path
import re

HEADER = Path("SpeechNotes/app/src/main/cpp/whispercpp/include/whisper.h")
SOURCE = Path("SpeechNotes/app/src/main/cpp/whispercpp/src/whisper.cpp")

header = HEADER.read_text()
new_struct = """    struct whisper_timings {
        float mel_ms;
        float total_ms;
        float sample_ms;
        float sample_total_ms;
        int32_t sample_n;
        float encode_ms;
        float encode_total_ms;
        int32_t encode_n;
        float decode_ms;
        float decode_total_ms;
        int32_t decode_n;
        float batchd_ms;
        float batchd_total_ms;
        int32_t batchd_n;
        float prompt_ms;
        float prompt_total_ms;
        int32_t prompt_n;
    };"""
header_patched, count = re.subn(
    r"    struct whisper_timings \{.*?^    \};",
    new_struct,
    header,
    count=1,
    flags=re.S | re.M,
)
if count != 1:
    raise SystemExit(f"whisper_timings header patch count={count}")
HEADER.write_text(header_patched)

source = SOURCE.read_text()
pattern = r"    whisper_timings \* timings = new whisper_timings;\n(?:.*\n)*?    return timings;"
replacement = """    whisper_timings * timings = new whisper_timings;
    timings->mel_ms = 1e-3f * ctx->state->t_mel_us;
    timings->total_ms = (ggml_time_us() - ctx->t_start_us) / 1000.0f;

    timings->sample_n = ctx->state->n_sample;
    timings->sample_total_ms = 1e-3f * ctx->state->t_sample_us;
    timings->sample_ms = timings->sample_total_ms / std::max(1, timings->sample_n);

    timings->encode_n = ctx->state->n_encode;
    timings->encode_total_ms = 1e-3f * ctx->state->t_encode_us;
    timings->encode_ms = timings->encode_total_ms / std::max(1, timings->encode_n);

    timings->decode_n = ctx->state->n_decode;
    timings->decode_total_ms = 1e-3f * ctx->state->t_decode_us;
    timings->decode_ms = timings->decode_total_ms / std::max(1, timings->decode_n);

    timings->batchd_n = ctx->state->n_batchd;
    timings->batchd_total_ms = 1e-3f * ctx->state->t_batchd_us;
    timings->batchd_ms = timings->batchd_total_ms / std::max(1, timings->batchd_n);

    timings->prompt_n = ctx->state->n_prompt;
    timings->prompt_total_ms = 1e-3f * ctx->state->t_prompt_us;
    timings->prompt_ms = timings->prompt_total_ms / std::max(1, timings->prompt_n);
    return timings;"""
source_patched, count = re.subn(pattern, replacement, source, count=1)
if count != 1:
    raise SystemExit(f"whisper_get_timings implementation patch count={count}")
SOURCE.write_text(source_patched)

print("Extended whisper.cpp timing API successfully")
