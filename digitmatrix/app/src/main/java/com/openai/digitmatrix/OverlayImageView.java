package com.openai.digitmatrix;

import android.content.Context;
import android.graphics.*;
import android.util.AttributeSet;
import android.widget.ImageView;
import java.util.*;

public class OverlayImageView extends ImageView {
    private ArrayList<ImageProcessor.DigitBox> boxes=new ArrayList<>();
    private int sw=1,sh=1;
    private final Paint p=new Paint(1),tp=new Paint(1);
    public OverlayImageView(Context c){super(c);init();}
    public OverlayImageView(Context c,AttributeSet a){super(c,a);init();}
    private void init(){setScaleType(ScaleType.FIT_CENTER);p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(3);p.setColor(Color.rgb(30,170,80));tp.setTextSize(34);tp.setColor(Color.rgb(20,130,60));tp.setTypeface(Typeface.DEFAULT_BOLD);}
    public void setResult(Bitmap b,ArrayList<ImageProcessor.DigitBox>d){setImageBitmap(b);sw=b.getWidth();sh=b.getHeight();boxes=d;invalidate();}
    @Override protected void onDraw(Canvas c){super.onDraw(c);if(getDrawable()==null)return;float s=Math.min(getWidth()/(float)sw,getHeight()/(float)sh),ox=(getWidth()-sw*s)/2f,oy=(getHeight()-sh*s)/2f;for(ImageProcessor.DigitBox d:boxes){RectF r=new RectF(ox+d.box.left*s,oy+d.box.top*s,ox+d.box.right*s,oy+d.box.bottom*s);c.drawRect(r,p);String txt=d.label+(d.digit>=0?(" "+Math.round(d.confidence*100)+"%") : "");c.drawText(txt,r.left,Math.max(tp.getTextSize(),r.top-4),tp);}}
}
