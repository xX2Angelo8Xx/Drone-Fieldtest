package com.chatgpt.speechnotes;

/** Small dependency-free 80-bin log-mel extractor for the speaker embedding model. */
public final class FbankExtractor {
    private static final int SAMPLE_RATE = 16000;
    private static final int FRAME_LEN = 400;   // 25 ms
    private static final int FRAME_SHIFT = 160; // 10 ms
    private static final int FFT = 512;
    private static final int MEL_BINS = 80;
    private static final double LOW_HZ = 20.0;
    private static final double HIGH_HZ = 7600.0;
    private static final float[][] MEL = buildMelBank();

    private FbankExtractor() {}

    public static float[][] extract(short[] pcm) {
        if (pcm == null || pcm.length < FRAME_LEN) return new float[0][MEL_BINS];
        int frames = 1 + (pcm.length - FRAME_LEN) / FRAME_SHIFT;
        float[][] out = new float[frames][MEL_BINS];
        double[] real = new double[FFT];
        double[] imag = new double[FFT];
        double[] power = new double[FFT / 2 + 1];

        for (int f = 0; f < frames; f++) {
            int base = f * FRAME_SHIFT;
            for (int i = 0; i < FFT; i++) { real[i] = 0.0; imag[i] = 0.0; }
            double prev = base > 0 ? pcm[base - 1] : pcm[base];
            for (int i = 0; i < FRAME_LEN; i++) {
                double x = pcm[base + i];
                double pre = x - 0.97 * prev;
                prev = x;
                double a = 0.5 - 0.5 * Math.cos(2.0 * Math.PI * i / (FRAME_LEN - 1));
                double povey = Math.pow(a, 0.85);
                real[i] = pre * povey;
            }
            fft(real, imag);
            for (int k = 0; k < power.length; k++) {
                power[k] = (real[k] * real[k] + imag[k] * imag[k]) / FFT;
            }
            for (int m = 0; m < MEL_BINS; m++) {
                double e = 0.0;
                float[] w = MEL[m];
                for (int k = 0; k < w.length; k++) e += power[k] * w[k];
                out[f][m] = (float) Math.log(Math.max(e, 1e-10));
            }
        }

        // Per-utterance CMN is important for speaker models trained with Kaldi fbanks.
        for (int m = 0; m < MEL_BINS; m++) {
            double mean = 0.0;
            for (int f = 0; f < frames; f++) mean += out[f][m];
            mean /= frames;
            for (int f = 0; f < frames; f++) out[f][m] -= (float) mean;
        }
        return out;
    }

    public static float[] flatten(float[][] feats) {
        if (feats == null || feats.length == 0) return new float[0];
        float[] out = new float[feats.length * MEL_BINS];
        int p = 0;
        for (float[] frame : feats) {
            if (frame.length != MEL_BINS) throw new IllegalArgumentException("Expected 80-bin fbank");
            System.arraycopy(frame, 0, out, p, MEL_BINS);
            p += MEL_BINS;
        }
        return out;
    }

    private static float[][] buildMelBank() {
        int bins = FFT / 2 + 1;
        float[][] bank = new float[MEL_BINS][bins];
        double lowMel = hzToMel(LOW_HZ);
        double highMel = hzToMel(HIGH_HZ);
        double[] hz = new double[MEL_BINS + 2];
        for (int i = 0; i < hz.length; i++) {
            double mel = lowMel + (highMel - lowMel) * i / (MEL_BINS + 1.0);
            hz[i] = melToHz(mel);
        }
        for (int m = 0; m < MEL_BINS; m++) {
            double left = hz[m], center = hz[m + 1], right = hz[m + 2];
            for (int k = 0; k < bins; k++) {
                double freq = k * SAMPLE_RATE / (double) FFT;
                double w = 0.0;
                if (freq >= left && freq <= center && center > left) w = (freq - left) / (center - left);
                else if (freq > center && freq <= right && right > center) w = (right - freq) / (right - center);
                bank[m][k] = (float) Math.max(0.0, w);
            }
        }
        return bank;
    }

    private static double hzToMel(double hz) { return 2595.0 * Math.log10(1.0 + hz / 700.0); }
    private static double melToHz(double mel) { return 700.0 * (Math.pow(10.0, mel / 2595.0) - 1.0); }

    private static void fft(double[] real, double[] imag) {
        int n = real.length;
        for (int i = 1, j = 0; i < n; i++) {
            int bit = n >> 1;
            for (; (j & bit) != 0; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) {
                double tr = real[i]; real[i] = real[j]; real[j] = tr;
                double ti = imag[i]; imag[i] = imag[j]; imag[j] = ti;
            }
        }
        for (int len = 2; len <= n; len <<= 1) {
            double ang = -2.0 * Math.PI / len;
            double wLenR = Math.cos(ang), wLenI = Math.sin(ang);
            for (int i = 0; i < n; i += len) {
                double wr = 1.0, wi = 0.0;
                for (int j = 0; j < len / 2; j++) {
                    int a = i + j, b = a + len / 2;
                    double vr = real[b] * wr - imag[b] * wi;
                    double vi = real[b] * wi + imag[b] * wr;
                    double ur = real[a], ui = imag[a];
                    real[a] = ur + vr; imag[a] = ui + vi;
                    real[b] = ur - vr; imag[b] = ui - vi;
                    double nwr = wr * wLenR - wi * wLenI;
                    wi = wr * wLenI + wi * wLenR;
                    wr = nwr;
                }
            }
        }
    }
}
