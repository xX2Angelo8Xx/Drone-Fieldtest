from pathlib import Path


def replace_once(path: str, old: str, new: str):
    p = Path(path)
    s = p.read_text()
    if old not in s:
        raise SystemExit(f'Expected block not found in {path}: {old[:100]!r}')
    p.write_text(s.replace(old, new, 1))


def replace_count(path: str, old: str, new: str, expected: int):
    p = Path(path)
    s = p.read_text()
    count = s.count(old)
    if count != expected:
        raise SystemExit(f'Expected {expected} occurrences in {path}, found {count}: {old[:100]!r}')
    p.write_text(s.replace(old, new))

main = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/MainActivity.java'
replace_count(
    main,
    'WhisperBridge.isModelLoaded(selectedModelFile().getAbsolutePath())',
    'WhisperBridge.isModelLoaded(selectedModelFile().getAbsolutePath(), WhisperBridge.BACKEND_BEST)',
    2
)
replace_once(
    main,
    'long loadMs = WhisperBridge.loadModel(file.getAbsolutePath());',
    'long loadMs = WhisperBridge.loadModel(file.getAbsolutePath(), getApplicationInfo().nativeLibraryDir, WhisperBridge.BACKEND_BEST);'
)
replace_once(
    main,
    '" · Init " + formatMsPrecise(WhisperBridge.currentModelLoadMs()));',
    '" · Init " + formatMsPrecise(WhisperBridge.currentModelLoadMs()) +\n                            " · CPU " + WhisperBridge.currentBackendName());'
)

service = 'SpeechNotes/app/src/main/java/com/chatgpt/speechnotes/RecordingService.java'
replace_count(
    service,
    'WhisperBridge.isModelLoaded(modelFile.getAbsolutePath())',
    'WhisperBridge.isModelLoaded(modelFile.getAbsolutePath(), WhisperBridge.BACKEND_BEST)',
    2
)
replace_once(
    service,
    'int threads = Math.max(2, Math.min(6, Runtime.getRuntime().availableProcessors() - 2));',
    'int cores = Math.max(1, Runtime.getRuntime().availableProcessors());\n                int threads = model.startsWith("large-v3-turbo")\n                        ? Math.min(8, cores)\n                        : Math.max(2, Math.min(6, Math.max(2, cores - 2)));'
)

print('Patched Speech Notes runtime: ARM Best default + Turbo 8-thread default')
