from pathlib import Path

p = Path('SpeechNotes/app/src/main/cpp/whispercpp/ggml/src/CMakeLists.txt')
s = p.read_text()
needle = '''            ggml_add_cpu_backend_variant(android_armv9.0_1    DOTPROD MATMUL_INT8 FP16_VECTOR_ARITHMETIC SVE2)\n'''
if needle not in s:
    raise SystemExit('android_armv9.0_1 insertion point not found')
insert = needle + '''\n            # Speech Notes v1.8: isolated KleidiAI A/B backend. Keep the stock\n            # android_armv9.0_1 target untouched and build a second module with\n            # the identical ISA feature set plus KleidiAI optimized kernels.\n            set(GGML_CPU_KLEIDIAI ON)\n            ggml_add_cpu_backend_variant(android_armv9.0_1_kai DOTPROD MATMUL_INT8 FP16_VECTOR_ARITHMETIC SVE2)\n            set(GGML_CPU_KLEIDIAI OFF)\n'''
s = s.replace(needle, insert, 1)
p.write_text(s)
print('Added isolated android_armv9.0_1_kai backend')
