package com.openai.digitmatrix;

import android.graphics.*;
import java.util.*;

public final class ImageProcessor {
    public static final class DigitBox {
        public Rect box; public int digit; public float confidence;
        DigitBox(Rect r){box=r;}
        int cx(){return (box.left+box.right)/2;} int cy(){return (box.top+box.bottom)/2;}
    }
    public static final class Result {
        public Bitmap processedBitmap; public ArrayList<DigitBox> digits; public String matrixText; public String csv; public String json;
    }
    private static final class Comp { int l,t,r,b,area; Comp(int x,int y){l=r=x;t=b=y;} }

    public static Result recognize(Bitmap src, DigitClassifier clf) {
        Bitmap bmp=downscale(src,1600); int W=bmp.getWidth(),H=bmp.getHeight();
        int[] pix=new int[W*H]; bmp.getPixels(pix,0,W,0,0,W,H); byte[] gray=new byte[W*H]; int[] hist=new int[256];
        for(int i=0;i<pix.length;i++){int c=pix[i];int g=(77*Color.red(c)+150*Color.green(c)+29*Color.blue(c))>>8;gray[i]=(byte)g;hist[g]++;}
        int th=otsu(hist,W*H); boolean[] ink=new boolean[W*H]; int count=0;
        for(int i=0;i<ink.length;i++){ink[i]=(gray[i]&255)<th; if(ink[i])count++;}
        if(count>ink.length/2){ for(int i=0;i<ink.length;i++)ink[i]=!ink[i]; }
        removeLongLines(ink,W,H);
        ArrayList<Comp> comps=components(ink,W,H);
        ArrayList<DigitBox> digits=new ArrayList<>();
        int minArea=Math.max(14,(W*H)/1500000); int minDim=Math.max(2,Math.min(W,H)/800);
        for(Comp c:comps){int bw=c.r-c.l+1,bh=c.b-c.t+1; if(c.area<minArea||bw<minDim||bh<minDim)continue;
            if(bw>W*0.55||bh>H*0.55)continue; float ar=bw/(float)bh; if(ar>5.5f||ar<0.08f)continue;
            Rect box=new Rect(c.l,c.t,c.r+1,c.b+1); float[] in=mnistCrop(gray,ink,W,H,box); DigitClassifier.Prediction p=clf.predict(in);
            if(p.confidence<0.38f) continue;
            DigitBox d=new DigitBox(box);d.digit=p.digit;d.confidence=p.confidence;digits.add(d);
        }
        Collections.sort(digits,Comparator.comparingInt(DigitBox::cy).thenComparingInt(DigitBox::cx));
        Layout lay=layout(digits,W,H);
        Result res=new Result();res.processedBitmap=bmp;res.digits=digits;res.matrixText=lay.text;res.csv=lay.csv;res.json=lay.json;return res;
    }

    private static Bitmap downscale(Bitmap b,int max){int w=b.getWidth(),h=b.getHeight(); if(Math.max(w,h)<=max)return b.copy(Bitmap.Config.ARGB_8888,false);float s=max/(float)Math.max(w,h);return Bitmap.createScaledBitmap(b,Math.round(w*s),Math.round(h*s),true);}
    private static int otsu(int[] h,int n){double sum=0;for(int i=0;i<256;i++)sum+=i*(double)h[i];double sb=0;int wb=0,best=127;double vmax=-1;for(int t=0;t<256;t++){wb+=h[t];if(wb==0)continue;int wf=n-wb;if(wf==0)break;sb+=t*(double)h[t];double mb=sb/wb,mf=(sum-sb)/wf,v=wb*(double)wf*(mb-mf)*(mb-mf);if(v>vmax){vmax=v;best=t;}}return Math.max(45,Math.min(225,best));}
    private static void removeLongLines(boolean[] a,int w,int h){int hr=Math.max(60,w/4),vr=Math.max(60,h/4);boolean[] kill=new boolean[a.length];
        for(int y=0;y<h;y++){int x=0;while(x<w){while(x<w&&!a[y*w+x])x++;int s=x;while(x<w&&a[y*w+x])x++;if(x-s>=hr)for(int yy=Math.max(0,y-2);yy<=Math.min(h-1,y+2);yy++)for(int xx=s;xx<x;xx++)kill[yy*w+xx]=true;}}
        for(int x=0;x<w;x++){int y=0;while(y<h){while(y<h&&!a[y*w+x])y++;int s=y;while(y<h&&a[y*w+x])y++;if(y-s>=vr)for(int xx=Math.max(0,x-2);xx<=Math.min(w-1,x+2);xx++)for(int yy=s;yy<y;yy++)kill[yy*w+xx]=true;}}
        for(int i=0;i<a.length;i++)if(kill[i])a[i]=false;
    }
    private static ArrayList<Comp> components(boolean[] a,int w,int h){boolean[] seen=new boolean[a.length];int[] q=new int[a.length];ArrayList<Comp> out=new ArrayList<>();int[] dx={-1,0,1,-1,1,-1,0,1},dy={-1,-1,-1,0,0,1,1,1};
        for(int y=0;y<h;y++)for(int x=0;x<w;x++){int st=y*w+x;if(!a[st]||seen[st])continue;int qs=0,qe=0;q[qe++]=st;seen[st]=true;Comp c=new Comp(x,y);while(qs<qe){int p=q[qs++],px=p%w,py=p/w;c.area++;if(px<c.l)c.l=px;if(px>c.r)c.r=px;if(py<c.t)c.t=py;if(py>c.b)c.b=py;for(int k=0;k<8;k++){int nx=px+dx[k],ny=py+dy[k];if(nx<0||nx>=w||ny<0||ny>=h)continue;int ni=ny*w+nx;if(a[ni]&&!seen[ni]){seen[ni]=true;q[qe++]=ni;}}}out.add(c);}return out;}
    private static float[] mnistCrop(byte[] gray,boolean[] ink,int W,int H,Rect b){int bw=b.width(),bh=b.height();float scale=20f/Math.max(bw,bh);int nw=Math.max(1,Math.round(bw*scale)),nh=Math.max(1,Math.round(bh*scale));float[] out=new float[784];int ox=(28-nw)/2,oy=(28-nh)/2;
        for(int yy=0;yy<nh;yy++)for(int xx=0;xx<nw;xx++){float sx=b.left+(xx+0.5f)/scale-0.5f,sy=b.top+(yy+0.5f)/scale-0.5f;int ix=Math.max(b.left,Math.min(b.right-1,Math.round(sx))),iy=Math.max(b.top,Math.min(b.bottom-1,Math.round(sy)));int idx=iy*W+ix;float v=ink[idx]?(255-(gray[idx]&255))/255f:0f;out[(oy+yy)*28+ox+xx]=Math.max(0f,Math.min(1f,v));}
        return out;}

    private static final class Token {ArrayList<DigitBox>d=new ArrayList<>();int l,r,t,b;String value;int cx(){return(l+r)/2;}}
    private static final class Row {ArrayList<DigitBox>d=new ArrayList<>();float cy;}
    private static final class Layout {String text,csv,json;}
    private static Layout layout(ArrayList<DigitBox> ds,int W,int H){Layout z=new Layout();if(ds.isEmpty()){z.text="No digits detected";z.csv="";z.json="[]";return z;}
        ArrayList<Integer> hs=new ArrayList<>(),ws=new ArrayList<>();for(DigitBox d:ds){hs.add(d.box.height());ws.add(d.box.width());}Collections.sort(hs);Collections.sort(ws);float mh=hs.get(hs.size()/2),mw=ws.get(ws.size()/2);
        ArrayList<Row> rows=new ArrayList<>();ArrayList<DigitBox> byY=new ArrayList<>(ds);byY.sort(Comparator.comparingInt(DigitBox::cy));for(DigitBox d:byY){Row best=null;float bd=Float.MAX_VALUE;for(Row r:rows){float q=Math.abs(d.cy()-r.cy);if(q<bd&&q<Math.max(mh*0.65f,8)){bd=q;best=r;}}if(best==null){best=new Row();best.cy=d.cy();rows.add(best);}best.d.add(d);float s=0;for(DigitBox x:best.d)s+=x.cy();best.cy=s/best.d.size();}rows.sort(Comparator.comparingDouble(r->r.cy));
        ArrayList<ArrayList<Token>> tokenRows=new ArrayList<>();for(Row row:rows){row.d.sort(Comparator.comparingInt(DigitBox::cx));ArrayList<Token> ts=new ArrayList<>();Token cur=null;for(DigitBox d:row.d){if(cur==null||d.box.left-cur.r>Math.max(mw*0.62f,mh*0.30f)){cur=new Token();cur.l=d.box.left;cur.r=d.box.right;cur.t=d.box.top;cur.b=d.box.bottom;ts.add(cur);}cur.d.add(d);cur.r=Math.max(cur.r,d.box.right);cur.t=Math.min(cur.t,d.box.top);cur.b=Math.max(cur.b,d.box.bottom);}for(Token t:ts){StringBuilder s=new StringBuilder();for(DigitBox d:t.d)s.append(d.digit);t.value=s.toString();}tokenRows.add(ts);}
        ArrayList<Float> cols=new ArrayList<>();float colTol=Math.max(mw*2.2f,W*0.025f);for(ArrayList<Token> ts:tokenRows)for(Token t:ts){int c=t.cx(),best=-1;float bd=1e9f;for(int i=0;i<cols.size();i++){float d=Math.abs(c-cols.get(i));if(d<bd){bd=d;best=i;}}if(best<0||bd>colTol)cols.add((float)c);else cols.set(best,(cols.get(best)+c)/2f);}Collections.sort(cols);
        String[][] grid=new String[tokenRows.size()][cols.size()];for(int r=0;r<grid.length;r++)Arrays.fill(grid[r],"");for(int r=0;r<tokenRows.size();r++)for(Token t:tokenRows.get(r)){int bi=0;float bd=1e9f;for(int i=0;i<cols.size();i++){float d=Math.abs(t.cx()-cols.get(i));if(d<bd){bd=d;bi=i;}}grid[r][bi]=t.value;}
        int[] widths=new int[cols.size()];for(int c=0;c<cols.size();c++){widths[c]=1;for(String[] row:grid)widths[c]=Math.max(widths[c],row[c].length());}
        StringBuilder text=new StringBuilder(),csv=new StringBuilder(),json=new StringBuilder("[");for(int r=0;r<grid.length;r++){if(r>0)json.append(',');json.append('[');for(int c=0;c<cols.size();c++){if(c>0){text.append("   ");csv.append(',');json.append(',');}String v=grid[r][c];text.append(String.format(Locale.US,"%"+widths[c]+"s",v));csv.append(v);json.append(v.isEmpty()?"null":v);}text.append('\n');csv.append('\n');json.append(']');}json.append(']');z.text=text.toString().trim();z.csv=csv.toString();z.json=json.toString();return z;}
}
