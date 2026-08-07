package com.openai.digitmatrix;

import android.content.Context;
import android.util.Base64;
import java.io.*;
import java.util.*;
import java.util.zip.GZIPInputStream;

public final class DigitClassifier {
    private final float[] c1w, c1b, c2w, c2b, f1w, f1b, f2w, f2b;
    public static final class Prediction {
        public final int digit; public final float confidence;
        Prediction(int d, float c){ digit=d; confidence=c; }
    }
    public DigitClassifier(Context ctx) throws IOException {
        byte[] encoded;
        try (InputStream raw = ctx.getAssets().open("model.b64")) {
            ByteArrayOutputStream bos=new ByteArrayOutputStream(); byte[] buf=new byte[8192]; int n;
            while((n=raw.read(buf))>0) bos.write(buf,0,n); encoded=bos.toByteArray();
        }
        byte[] gz=Base64.decode(encoded, Base64.DEFAULT);
        try (DataInputStream in = new DataInputStream(new GZIPInputStream(new ByteArrayInputStream(gz)))) {
            byte[] magic = new byte[4]; in.readFully(magic);
            if (!Arrays.equals(magic, new byte[]{'D','M','W','1'})) throw new IOException("Bad model format");
            float[][] a = new float[8][];
            for (int layer=0; layer<4; layer++) {
                float scale = readFloatLE(in); int nw = readIntLE(in); int nb = readIntLE(in);
                byte[] q = new byte[nw]; in.readFully(q); float[] w = new float[nw];
                for (int i=0;i<nw;i++) w[i] = q[i] * scale;
                float[] b = new float[nb]; for(int i=0;i<nb;i++) b[i]=readFloatLE(in);
                a[layer*2]=w; a[layer*2+1]=b;
            }
            c1w=a[0];c1b=a[1];c2w=a[2];c2b=a[3];f1w=a[4];f1b=a[5];f2w=a[6];f2b=a[7];
        }
    }
    private static int readIntLE(DataInputStream in) throws IOException { return Integer.reverseBytes(in.readInt()); }
    private static float readFloatLE(DataInputStream in) throws IOException { return Float.intBitsToFloat(readIntLE(in)); }

    public Prediction predict(float[] input) {
        float[] x = new float[784];
        for(int i=0;i<784;i++) x[i]=(input[i]-0.1307f)/0.3081f;
        float[] p1 = convReluPool(x,1,28,28,c1w,c1b,32,5,2);
        float[] p2 = convReluPool(p1,32,14,14,c2w,c2b,64,3,1);
        float[] h = new float[128];
        for(int o=0;o<128;o++) {
            float s=f1b[o]; int base=o*3136;
            for(int i=0;i<3136;i++) s += f1w[base+i]*p2[i];
            h[o]=Math.max(0f,s);
        }
        float[] z=new float[10]; float max=-Float.MAX_VALUE; int best=0;
        for(int o=0;o<10;o++) {
            float s=f2b[o]; int base=o*128;
            for(int i=0;i<128;i++) s += f2w[base+i]*h[i];
            z[o]=s; if(s>max){max=s;best=o;}
        }
        double den=0; for(float v:z) den += Math.exp(v-max);
        float conf=(float)(Math.exp(z[best]-max)/den);
        return new Prediction(best,conf);
    }

    private static float[] convReluPool(float[] in,int cin,int h,int w,float[] wt,float[] bias,int cout,int k,int pad){
        float[] pooled=new float[cout*(h/2)*(w/2)]; int ph=h/2,pw=w/2;
        for(int co=0;co<cout;co++) for(int py=0;py<ph;py++) for(int px=0;px<pw;px++) {
            float mx=0f;
            for(int dy=0;dy<2;dy++) for(int dx=0;dx<2;dx++) {
                int y=py*2+dy, x=px*2+dx; float s=bias[co];
                for(int ci=0;ci<cin;ci++) {
                    int inBase=ci*h*w; int wBase=(co*cin+ci)*k*k;
                    for(int ky=0;ky<k;ky++) { int iy=y+ky-pad; if(iy<0||iy>=h) continue;
                        int row=inBase+iy*w;
                        for(int kx=0;kx<k;kx++) { int ix=x+kx-pad; if(ix<0||ix>=w) continue; s += in[row+ix]*wt[wBase+ky*k+kx]; }
                    }
                }
                if(s>mx) mx=s;
            }
            pooled[co*ph*pw+py*pw+px]=mx;
        }
        return pooled;
    }
}
