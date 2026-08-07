package com.openai.digitmatrix;

import android.graphics.*;
import java.util.*;

public final class ImageProcessor {
    public static final class DigitBox {
        public Rect box; public int digit=-1; public String label="?"; public float confidence;
        DigitBox(Rect r){box=r;}
        int cx(){return (box.left+box.right)/2;} int cy(){return (box.top+box.bottom)/2;}
    }
    public static final class Result {
        public Bitmap processedBitmap; public ArrayList<DigitBox> digits; public String matrixText; public String csv; public String json;
    }
    private static final class Comp { int l,t,r,b,area; Comp(int x,int y){l=r=x;t=b=y;} Rect rect(){return new Rect(l,t,r+1,b+1);} }
    private static final class Candidate { Comp c; boolean[] mask; DigitClassifier.Prediction p; float score; Candidate(Comp cc,boolean[]m){c=cc;mask=m;} }

    public static Result recognize(Bitmap src, DigitClassifier clf) {
        Bitmap bmp=downscale(src,1600); int W=bmp.getWidth(),H=bmp.getHeight();
        int[] pix=new int[W*H]; bmp.getPixels(pix,0,W,0,0,W,H); byte[] gray=new byte[W*H]; int[] hist=new int[256];
        int[] rr=new int[pix.length],gg=new int[pix.length],bb=new int[pix.length];
        for(int i=0;i<pix.length;i++){int c=pix[i];int r=Color.red(c),g=Color.green(c),b=Color.blue(c);rr[i]=r;gg[i]=g;bb[i]=b;int q=(77*r+150*g+29*b)>>8;gray[i]=(byte)q;hist[q]++;}
        int th=otsu(hist,W*H);
        ArrayList<boolean[]> masks=new ArrayList<>();
        boolean[] dark=new boolean[W*H],light=new boolean[W*H],yellow=new boolean[W*H];
        for(int i=0;i<dark.length;i++){
            int q=gray[i]&255,r=rr[i],g=gg[i],b=bb[i];
            dark[i]=q<Math.max(25,th-4);
            light[i]=q<245 && q>Math.min(235,th+8);
            int yellowScore=((r+g)>>1)-b;
            yellow[i]=yellowScore>32 && r>105 && g>85 && Math.abs(r-g)<105;
        }
        masks.add(dark); masks.add(light); masks.add(yellow);
        for(boolean[] m:masks){removeLongLines(m,W,H); close3(m,W,H);}

        ArrayList<Candidate> raw=new ArrayList<>(); int minArea=Math.max(18,(W*H)/1800000); int minDim=Math.max(2,Math.min(W,H)/900);
        for(boolean[] mask:masks){
            for(Comp c:components(mask,W,H)){
                int bw=c.r-c.l+1,bh=c.b-c.t+1;
                if(c.area<minArea||bw<minDim||bh<minDim)continue;
                if(bw>W*0.78||bh>H*0.78||c.area>W*H*0.42)continue;
                float ar=bw/(float)bh; if(ar>7f||ar<0.045f)continue;
                Candidate z=new Candidate(c,mask); float[] in=mnistCrop(mask,W,H,c.rect()); z.p=clf.predict(in);
                float size=(float)Math.sqrt((bw*(double)bh)/(W*(double)H)); z.score=z.p.confidence*(0.65f+4.0f*size); raw.add(z);
            }
        }
        raw.sort((a,b)->Float.compare(b.score,a.score));
        ArrayList<Candidate> unique=new ArrayList<>();
        for(Candidate c:raw){boolean dup=false;for(Candidate u:unique)if(iou(c.c.rect(),u.c.rect())>0.58f){dup=true;break;}if(!dup)unique.add(c);if(unique.size()>180)break;}

        // Keep plausible digit candidates, then select the dominant character-size cluster. This strongly suppresses poster text and faces.
        ArrayList<Candidate> plausible=new ArrayList<>();
        for(Candidate c:unique){int h=c.c.b-c.c.t+1;float minConf=h>H*0.08f?0.18f:0.52f;if(c.p.confidence>=minConf)plausible.add(c);}
        ArrayList<Candidate> chosen=dominantScale(plausible,W,H);
        ArrayList<DigitBox> digits=new ArrayList<>();
        for(Candidate c:chosen){DigitBox d=new DigitBox(c.c.rect());d.digit=c.p.digit;d.label=Integer.toString(c.p.digit);d.confidence=c.p.confidence;digits.add(d);}

        // Add decimal comma/dot geometrically. Punctuation is only accepted BETWEEN two selected digits on the same row.
        if(!digits.isEmpty()){
            ArrayList<Integer> hs=new ArrayList<>();for(DigitBox d:digits)hs.add(d.box.height());Collections.sort(hs);float mh=hs.get(hs.size()/2);
            ArrayList<Comp> tiny=new ArrayList<>();for(boolean[] mask:masks)for(Comp c:components(mask,W,H)){int h=c.b-c.t+1,w=c.r-c.l+1;if(h>=mh*0.10f&&h<=mh*0.48f&&w<=mh*0.42f&&c.area>=minArea)tiny.add(c);}
            for(Comp c:tiny){Rect r=c.rect();int cx=(r.left+r.right)/2,cy=(r.top+r.bottom)/2;boolean inside=false;for(DigitBox d:digits)if(Rect.intersects(r,d.box)){inside=true;break;}if(inside)continue;
                DigitBox left=null,right=null;for(DigitBox d:digits){if(Math.abs(d.cy()-cy)>mh*0.60f)continue;if(d.box.right<=cx&&(left==null||d.box.right>left.box.right))left=d;if(d.box.left>=cx&&(right==null||d.box.left<right.box.left))right=d;}
                if(left==null||right==null)continue;if(cx-left.box.right>mh*0.45f||right.box.left-cx>mh*0.45f)continue;
                DigitBox p=new DigitBox(r);p.label=r.height()>r.width()*1.25f?",":".";p.confidence=1f;digits.add(p);
            }
        }
        Collections.sort(digits,Comparator.comparingInt(DigitBox::cy).thenComparingInt(DigitBox::cx));
        Layout lay=layout(digits,W,H);
        Result res=new Result();res.processedBitmap=bmp;res.digits=digits;res.matrixText=lay.text;res.csv=lay.csv;res.json=lay.json;return res;
    }

    private static ArrayList<Candidate> dominantScale(ArrayList<Candidate> in,int W,int H){
        if(in.size()<=2)return in;float best=-1f;ArrayList<Candidate> out=new ArrayList<>();
        for(Candidate seed:in){float sh=seed.c.b-seed.c.t+1;ArrayList<Candidate> g=new ArrayList<>();float s=0f;for(Candidate c:in){float h=c.c.b-c.c.t+1;float ratio=h/sh;if(ratio>=0.62f&&ratio<=1.62f){g.add(c);float area=(c.c.r-c.c.l+1)*(float)(c.c.b-c.c.t+1);s+=c.p.confidence*(float)Math.sqrt(area);}}if(s>best){best=s;out=g;}}
        return out;
    }
    private static float iou(Rect a,Rect b){int l=Math.max(a.left,b.left),t=Math.max(a.top,b.top),r=Math.min(a.right,b.right),bot=Math.min(a.bottom,b.bottom);if(r<=l||bot<=t)return 0f;float inter=(r-l)*(float)(bot-t);float ua=a.width()*(float)a.height()+b.width()*(float)b.height()-inter;return inter/ua;}
    private static Bitmap downscale(Bitmap b,int max){int w=b.getWidth(),h=b.getHeight(); if(Math.max(w,h)<=max)return b.copy(Bitmap.Config.ARGB_8888,false);float s=max/(float)Math.max(w,h);return Bitmap.createScaledBitmap(b,Math.round(w*s),Math.round(h*s),true);}
    private static int otsu(int[] h,int n){double sum=0;for(int i=0;i<256;i++)sum+=i*(double)h[i];double sb=0;int wb=0,best=127;double vmax=-1;for(int t=0;t<256;t++){wb+=h[t];if(wb==0)continue;int wf=n-wb;if(wf==0)break;sb+=t*(double)h[t];double mb=sb/wb,mf=(sum-sb)/wf,v=wb*(double)wf*(mb-mf)*(mb-mf);if(v>vmax){vmax=v;best=t;}}return Math.max(35,Math.min(225,best));}
    private static void close3(boolean[] a,int w,int h){boolean[] d=new boolean[a.length];for(int y=1;y<h-1;y++)for(int x=1;x<w-1;x++){boolean v=false;for(int yy=-1;yy<=1&&!v;yy++)for(int xx=-1;xx<=1;xx++)if(a[(y+yy)*w+x+xx]){v=true;break;}d[y*w+x]=v;}boolean[] e=new boolean[a.length];for(int y=1;y<h-1;y++)for(int x=1;x<w-1;x++){boolean v=true;for(int yy=-1;yy<=1&&v;yy++)for(int xx=-1;xx<=1;xx++)if(!d[(y+yy)*w+x+xx]){v=false;break;}e[y*w+x]=v;}System.arraycopy(e,0,a,0,a.length);}
    private static void removeLongLines(boolean[] a,int w,int h){int hr=Math.max(80,w/3),vr=Math.max(80,h/3);boolean[] kill=new boolean[a.length];
        for(int y=0;y<h;y++){int x=0;while(x<w){while(x<w&&!a[y*w+x])x++;int s=x;while(x<w&&a[y*w+x])x++;if(x-s>=hr)for(int yy=Math.max(0,y-1);yy<=Math.min(h-1,y+1);yy++)for(int xx=s;xx<x;xx++)kill[yy*w+xx]=true;}}
        for(int x=0;x<w;x++){int y=0;while(y<h){while(y<h&&!a[y*w+x])y++;int s=y;while(y<h&&a[y*w+x])y++;if(y-s>=vr)for(int xx=Math.max(0,x-1);xx<=Math.min(w-1,x+1);xx++)for(int yy=s;yy<y;yy++)kill[yy*w+xx]=true;}}
        for(int i=0;i<a.length;i++)if(kill[i])a[i]=false;
    }
    private static ArrayList<Comp> components(boolean[] a,int w,int h){boolean[] seen=new boolean[a.length];int[] q=new int[a.length];ArrayList<Comp> out=new ArrayList<>();int[] dx={-1,0,1,-1,1,-1,0,1},dy={-1,-1,-1,0,0,1,1,1};
        for(int y=0;y<h;y++)for(int x=0;x<w;x++){int st=y*w+x;if(!a[st]||seen[st])continue;int qs=0,qe=0;q[qe++]=st;seen[st]=true;Comp c=new Comp(x,y);while(qs<qe){int p=q[qs++],px=p%w,py=p/w;c.area++;if(px<c.l)c.l=px;if(px>c.r)c.r=px;if(py<c.t)c.t=py;if(py>c.b)c.b=py;for(int k=0;k<8;k++){int nx=px+dx[k],ny=py+dy[k];if(nx<0||nx>=w||ny<0||ny>=h)continue;int ni=ny*w+nx;if(a[ni]&&!seen[ni]){seen[ni]=true;q[qe++]=ni;}}}out.add(c);}return out;}
    private static float[] mnistCrop(boolean[] ink,int W,int H,Rect b){int bw=b.width(),bh=b.height();float scale=20f/Math.max(bw,bh);int nw=Math.max(1,Math.round(bw*scale)),nh=Math.max(1,Math.round(bh*scale));float[] out=new float[784];int ox=(28-nw)/2,oy=(28-nh)/2;
        for(int yy=0;yy<nh;yy++)for(int xx=0;xx<nw;xx++){float sx=b.left+(xx+0.5f)/scale-0.5f,sy=b.top+(yy+0.5f)/scale-0.5f;int ix=Math.max(b.left,Math.min(b.right-1,Math.round(sx))),iy=Math.max(b.top,Math.min(b.bottom-1,Math.round(sy)));out[(oy+yy)*28+ox+xx]=ink[iy*W+ix]?1f:0f;}return out;}

    private static final class Token {ArrayList<DigitBox>d=new ArrayList<>();int l,r,t,b;String value;int cx(){return(l+r)/2;}}
    private static final class Row {ArrayList<DigitBox>d=new ArrayList<>();float cy;}
    private static final class Layout {String text,csv,json;}
    private static Layout layout(ArrayList<DigitBox> ds,int W,int H){Layout z=new Layout();if(ds.isEmpty()){z.text="No digits detected";z.csv="";z.json="[]";return z;}
        ArrayList<Integer> hs=new ArrayList<>(),ws=new ArrayList<>();for(DigitBox d:ds)if(d.digit>=0){hs.add(d.box.height());ws.add(d.box.width());}if(hs.isEmpty()){z.text="No digits detected";z.csv="";z.json="[]";return z;}Collections.sort(hs);Collections.sort(ws);float mh=hs.get(hs.size()/2),mw=ws.get(ws.size()/2);
        ArrayList<Row> rows=new ArrayList<>();ArrayList<DigitBox> byY=new ArrayList<>(ds);byY.sort(Comparator.comparingInt(DigitBox::cy));for(DigitBox d:byY){Row best=null;float bd=Float.MAX_VALUE;for(Row r:rows){float q=Math.abs(d.cy()-r.cy);if(q<bd&&q<Math.max(mh*0.68f,8)){bd=q;best=r;}}if(best==null){best=new Row();best.cy=d.cy();rows.add(best);}best.d.add(d);float s=0;for(DigitBox x:best.d)s+=x.cy();best.cy=s/best.d.size();}rows.sort(Comparator.comparingDouble(r->r.cy));
        ArrayList<ArrayList<Token>> tokenRows=new ArrayList<>();for(Row row:rows){row.d.sort(Comparator.comparingInt(DigitBox::cx));ArrayList<Token> ts=new ArrayList<>();Token cur=null;for(DigitBox d:row.d){float gapThresh=Math.max(mw*0.78f,mh*0.34f);if(cur==null||d.box.left-cur.r>gapThresh){cur=new Token();cur.l=d.box.left;cur.r=d.box.right;cur.t=d.box.top;cur.b=d.box.bottom;ts.add(cur);}cur.d.add(d);cur.r=Math.max(cur.r,d.box.right);cur.t=Math.min(cur.t,d.box.top);cur.b=Math.max(cur.b,d.box.bottom);}for(Token t:ts){StringBuilder s=new StringBuilder();for(DigitBox d:t.d)s.append(d.label);t.value=s.toString();}tokenRows.add(ts);}
        ArrayList<Float> cols=new ArrayList<>();float colTol=Math.max(mw*2.4f,W*0.025f);for(ArrayList<Token> ts:tokenRows)for(Token t:ts){int c=t.cx(),best=-1;float bd=1e9f;for(int i=0;i<cols.size();i++){float d=Math.abs(c-cols.get(i));if(d<bd){bd=d;best=i;}}if(best<0||bd>colTol)cols.add((float)c);else cols.set(best,(cols.get(best)+c)/2f);}Collections.sort(cols);
        String[][] grid=new String[tokenRows.size()][cols.size()];for(int r=0;r<grid.length;r++)Arrays.fill(grid[r],"");for(int r=0;r<tokenRows.size();r++)for(Token t:tokenRows.get(r)){int bi=0;float bd=1e9f;for(int i=0;i<cols.size();i++){float d=Math.abs(t.cx()-cols.get(i));if(d<bd){bd=d;bi=i;}}grid[r][bi]=t.value;}
        int[] widths=new int[cols.size()];for(int c=0;c<cols.size();c++){widths[c]=1;for(String[] row:grid)widths[c]=Math.max(widths[c],row[c].length());}
        StringBuilder text=new StringBuilder(),csv=new StringBuilder(),json=new StringBuilder("[");for(int r=0;r<grid.length;r++){if(r>0)json.append(',');json.append('[');for(int c=0;c<cols.size();c++){if(c>0){text.append("   ");csv.append(',');json.append(',');}String v=grid[r][c];text.append(String.format(Locale.US,"%"+widths[c]+"s",v));csv.append(v);if(v.isEmpty())json.append("null");else{try{json.append(Double.parseDouble(v.replace(',','.')));}catch(Exception e){json.append('"').append(v).append('"');}}}text.append('\n');csv.append('\n');json.append(']');}json.append(']');z.text=text.toString().trim();z.csv=csv.toString();z.json=json.toString();return z;}
}
