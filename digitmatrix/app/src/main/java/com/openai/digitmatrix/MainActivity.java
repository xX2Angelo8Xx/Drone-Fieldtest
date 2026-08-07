package com.openai.digitmatrix;

import android.app.*;
import android.os.*;
import android.provider.MediaStore;
import android.content.*;
import android.graphics.*;
import android.media.ExifInterface;
import android.net.Uri;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.text.*;
import java.util.*;

public class MainActivity extends Activity {
    private static final int CAM=10,PICK=11;
    private Uri cameraUri;
    private OverlayImageView image;
    private TextView status,result;
    private DigitClassifier classifier;
    private Bitmap current;
    private ImageProcessor.Result last;

    @Override public void onCreate(Bundle b){
        super.onCreate(b);
        try{classifier=new DigitClassifier(this);}catch(Exception e){fatal(e);return;}
        buildUi();
    }

    private void buildUi(){
        ScrollView sc=new ScrollView(this);
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setPadding(dp(18),dp(18),dp(18),dp(24));sc.addView(root);
        TextView title=txt("DigitMatrix",28,true);root.addView(title);
        TextView sub=txt("Offline recognition of handwritten digit matrices. Dark pen on light paper works best.",15,false);sub.setPadding(0,dp(4),0,dp(12));root.addView(sub);
        LinearLayout buttons=new LinearLayout(this);buttons.setOrientation(LinearLayout.HORIZONTAL);
        Button cam=button("Take photo"),pick=button("Choose image");
        buttons.addView(cam,new LinearLayout.LayoutParams(0,dp(52),1));buttons.addView(pick,new LinearLayout.LayoutParams(0,dp(52),1));root.addView(buttons);
        cam.setOnClickListener(v->takePhoto());pick.setOnClickListener(v->choose());
        status=txt("Ready",14,false);status.setPadding(0,dp(10),0,dp(6));root.addView(status);
        image=new OverlayImageView(this);root.addView(image,new LinearLayout.LayoutParams(-1,dp(420)));
        result=txt("",22,false);result.setTypeface(Typeface.MONOSPACE);result.setTextIsSelectable(true);result.setPadding(0,dp(14),0,dp(8));root.addView(result);
        LinearLayout actions=new LinearLayout(this);Button copy=button("Copy"),share=button("Share");actions.addView(copy,new LinearLayout.LayoutParams(0,dp(50),1));actions.addView(share,new LinearLayout.LayoutParams(0,dp(50),1));root.addView(actions);
        copy.setOnClickListener(v->copy());share.setOnClickListener(v->share());
        setContentView(sc);
    }

    private TextView txt(String s,int sp,boolean bold){TextView t=new TextView(this);t.setText(s);t.setTextSize(sp);t.setTextColor(Color.rgb(25,25,30));if(bold)t.setTypeface(Typeface.DEFAULT_BOLD);return t;}
    private Button button(String s){Button b=new Button(this);b.setText(s);b.setAllCaps(false);return b;}
    private int dp(int x){return Math.round(x*getResources().getDisplayMetrics().density);}

    private void takePhoto(){
        ContentValues v=new ContentValues();v.put(MediaStore.Images.Media.DISPLAY_NAME,"DigitMatrix_"+new SimpleDateFormat("yyyyMMdd_HHmmss",Locale.US).format(new Date())+".jpg");v.put(MediaStore.Images.Media.MIME_TYPE,"image/jpeg");
        cameraUri=getContentResolver().insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI,v);
        if(cameraUri==null){status.setText("Could not create image destination");return;}
        Intent i=new Intent(MediaStore.ACTION_IMAGE_CAPTURE);i.putExtra(MediaStore.EXTRA_OUTPUT,cameraUri);i.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION|Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try{startActivityForResult(i,CAM);}catch(Exception e){status.setText("No camera app found");}
    }

    private void choose(){Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT);i.setType("image/*");i.addCategory(Intent.CATEGORY_OPENABLE);startActivityForResult(i,PICK);}

    @Override protected void onActivityResult(int req,int code,Intent data){
        super.onActivityResult(req,code,data);if(code!=RESULT_OK)return;
        Uri u=req==CAM?cameraUri:(data==null?null:data.getData());if(u!=null)load(u);
    }

    private void load(Uri u){
        status.setText("Loading image…");
        new Thread(()->{try{Bitmap b=decode(u,2200);runOnUiThread(()->{current=b;image.setImageBitmap(b);status.setText("Recognizing…");recognize();});}catch(Exception e){runOnUiThread(()->status.setText("Image error: "+e.getMessage()));}}).start();
    }

    private Bitmap decode(Uri u,int max) throws Exception {
        int sample=1,w,h;BitmapFactory.Options o=new BitmapFactory.Options();o.inJustDecodeBounds=true;
        try(InputStream in=getContentResolver().openInputStream(u)){BitmapFactory.decodeStream(in,null,o);}w=o.outWidth;h=o.outHeight;
        while(Math.max(w/sample,h/sample)>max)sample*=2;
        o=new BitmapFactory.Options();o.inSampleSize=sample;Bitmap b;
        try(InputStream in=getContentResolver().openInputStream(u)){b=BitmapFactory.decodeStream(in,null,o);}
        if(b==null)throw new IOException("Could not decode image");
        int rot=0;
        try(InputStream in=getContentResolver().openInputStream(u)){ExifInterface ex=new ExifInterface(in);int q=ex.getAttributeInt(ExifInterface.TAG_ORIENTATION,ExifInterface.ORIENTATION_NORMAL);if(q==ExifInterface.ORIENTATION_ROTATE_90)rot=90;else if(q==ExifInterface.ORIENTATION_ROTATE_180)rot=180;else if(q==ExifInterface.ORIENTATION_ROTATE_270)rot=270;}catch(Exception ignored){}
        if(rot!=0){Matrix m=new Matrix();m.postRotate(rot);b=Bitmap.createBitmap(b,0,0,b.getWidth(),b.getHeight(),m,true);}
        return b;
    }

    private void recognize(){
        Bitmap b=current;if(b==null)return;
        new Thread(()->{long t=System.currentTimeMillis();try{ImageProcessor.Result r=ImageProcessor.recognize(b,classifier);long ms=System.currentTimeMillis()-t;runOnUiThread(()->{last=r;image.setResult(r.processedBitmap,r.digits);result.setText(r.matrixText);status.setText(r.digits.size()+" digits detected · "+ms+" ms");});}catch(Exception e){runOnUiThread(()->status.setText("Recognition error: "+e));}}).start();
    }

    private void copy(){if(last==null)return;((android.content.ClipboardManager)getSystemService(CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("DigitMatrix",last.matrixText));Toast.makeText(this,"Copied",Toast.LENGTH_SHORT).show();}
    private void share(){if(last==null)return;Intent i=new Intent(Intent.ACTION_SEND);i.setType("text/plain");i.putExtra(Intent.EXTRA_TEXT,last.matrixText+"\n\nCSV:\n"+last.csv+"\nJSON:\n"+last.json);startActivity(Intent.createChooser(i,"Share recognition"));}
    private void fatal(Exception e){TextView t=new TextView(this);t.setText("Model could not be loaded:\n"+e);t.setPadding(30,30,30,30);setContentView(t);}
}
