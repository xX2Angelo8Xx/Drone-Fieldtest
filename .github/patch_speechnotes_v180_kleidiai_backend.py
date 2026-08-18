from pathlib import Path

p = Path('SpeechNotes/app/src/main/cpp/whispercpp/ggml/src/ggml-cpu/CMakeLists.txt')
s = p.read_text()

# Upstream v1.9.1 uses add_compile_definitions() inside the KleidiAI block.
# That mutates the directory-wide compile definitions and therefore also marks
# already-created stock CPU backend targets as GGML_USE_CPU_KLEIDIAI even though
# those targets do not contain the KleidiAI implementation. The result is an
# undefined ggml_backend_cpu_kleidiai_buffer_type symbol at link time.
# Scope the definition to the backend currently being created instead.
old_def = '        add_compile_definitions(GGML_USE_CPU_KLEIDIAI)\n'
new_def = '        target_compile_definitions(${GGML_CPU_NAME} PRIVATE GGML_USE_CPU_KLEIDIAI)\n'
if s.count(old_def) != 1:
    raise SystemExit(f'Expected exactly one KleidiAI global compile definition, found {s.count(old_def)}')
s = s.replace(old_def, new_def, 1)
p.write_text(s)

p = Path('SpeechNotes/app/src/main/cpp/whispercpp/ggml/src/CMakeLists.txt')
s = p.read_text()
needle = '''            ggml_add_cpu_backend_variant(android_armv9.0_1    DOTPROD MATMUL_INT8 FP16_VECTOR_ARITHMETIC SVE2)\n'''
if needle not in s:
    raise SystemExit('android_armv9.0_1 insertion point not found')
insert = needle + '''\n            # Speech Notes v1.8: isolated KleidiAI A/B backend. Keep the stock\n            # android_armv9.0_1 target untouched and build a second module with\n            # the identical ISA feature set plus KleidiAI optimized kernels.\n            set(GGML_CPU_KLEIDIAI ON)\n            ggml_add_cpu_backend_variant(android_armv9.0_1_kai DOTPROD MATMUL_INT8 FP16_VECTOR_ARITHMETIC SVE2)\n            set(GGML_CPU_KLEIDIAI OFF)\n'''
s = s.replace(needle, insert, 1)
p.write_text(s)
print('Added isolated android_armv9.0_1_kai backend with target-scoped KleidiAI definition')
