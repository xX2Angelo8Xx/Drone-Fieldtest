import os, gzip, struct, base64, json, random, time, glob
from pathlib import Path
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import DataLoader, Dataset, ConcatDataset
from torchvision import datasets, transforms
from PIL import Image, ImageDraw, ImageFont, ImageFilter

SEED=20260807
random.seed(SEED); np.random.seed(SEED); torch.manual_seed(SEED); torch.set_num_threads(max(1, min(4, os.cpu_count() or 2)))
ROOT=Path(__file__).resolve().parent
DATA=ROOT/'data'
ASSET=ROOT/'app/src/main/assets/model.b64'
REPORT=ROOT/'training_report.json'

train_tf=transforms.Compose([
    transforms.RandomAffine(degrees=16, translate=(0.13,0.13), scale=(0.74,1.22), shear=(-7,7,-5,5), fill=0),
    transforms.RandomApply([transforms.GaussianBlur(3,(0.1,1.1))],p=0.22),
    transforms.ToTensor(),
    transforms.Normalize((0.1307,), (0.3081,)),
])
plain_tf=transforms.Compose([transforms.ToTensor(),transforms.Normalize((0.1307,), (0.3081,))])

full_aug=datasets.MNIST(DATA, train=True, download=True, transform=train_tf)
full_plain=datasets.MNIST(DATA, train=True, download=False, transform=plain_tf)
test=datasets.MNIST(DATA, train=False, download=True, transform=plain_tf)
g=torch.Generator().manual_seed(SEED)
indices=torch.randperm(len(full_aug),generator=g).tolist()
val_idx=indices[:5000]; train_idx=indices[5000:]
mnist_train=torch.utils.data.Subset(full_aug,train_idx)
val_ds=torch.utils.data.Subset(full_plain,val_idx)

fonts=[]
for pat in ['/usr/share/fonts/truetype/dejavu/*.ttf','/usr/share/fonts/truetype/liberation2/*.ttf','/usr/share/fonts/truetype/freefont/*.ttf']:
    fonts += glob.glob(pat)
fonts=[f for f in fonts if not any(x in os.path.basename(f).lower() for x in ['italic','oblique'])]
print('synthetic fonts',len(fonts),fonts[:5],flush=True)

class PrintedDigits(Dataset):
    def __init__(self,n=45000): self.n=n
    def __len__(self): return self.n
    def __getitem__(self,i):
        rng=random.Random(SEED*17+i)
        y=rng.randrange(10)
        im=Image.new('L',(40,40),0)
        d=ImageDraw.Draw(im)
        fp=fonts[rng.randrange(len(fonts))] if fonts else None
        size=rng.randint(23,34)
        font=ImageFont.truetype(fp,size) if fp else ImageFont.load_default()
        bb=d.textbbox((0,0),str(y),font=font,stroke_width=rng.choice([0,0,1]))
        tw,th=bb[2]-bb[0],bb[3]-bb[1]
        x=(40-tw)//2-bb[0]+rng.randint(-3,3); yy=(40-th)//2-bb[1]+rng.randint(-3,3)
        d.text((x,yy),str(y),font=font,fill=255,stroke_width=rng.choice([0,0,1]),stroke_fill=255)
        if rng.random()<0.30: im=im.filter(ImageFilter.GaussianBlur(rng.uniform(0.15,0.75)))
        im=im.resize((28,28),Image.Resampling.LANCZOS)
        return train_tf(im),y

train_ds=ConcatDataset([mnist_train,PrintedDigits()])
train_ld=DataLoader(train_ds,batch_size=256,shuffle=True,num_workers=2,persistent_workers=True)
val_ld=DataLoader(val_ds,batch_size=512,num_workers=2)
test_ld=DataLoader(test,batch_size=512,num_workers=2)

class CNN(nn.Module):
    def __init__(self):
        super().__init__(); self.c1=nn.Conv2d(1,32,5,padding=2); self.c2=nn.Conv2d(32,64,3,padding=1); self.f1=nn.Linear(64*7*7,128); self.f2=nn.Linear(128,10)
    def forward(self,x):
        x=F.max_pool2d(F.relu(self.c1(x)),2); x=F.max_pool2d(F.relu(self.c2(x)),2); x=F.relu(self.f1(x.flatten(1))); return self.f2(x)

m=CNN(); opt=torch.optim.AdamW(m.parameters(),lr=9e-4,weight_decay=1e-4); sched=torch.optim.lr_scheduler.CosineAnnealingLR(opt,T_max=6,eta_min=6e-5); lossfn=nn.CrossEntropyLoss(label_smoothing=0.02)
def accuracy(ld):
    m.eval(); correct=total=0
    with torch.inference_mode():
        for x,y in ld:
            z=m(x); correct+=(z.argmax(1)==y).sum().item(); total+=len(y)
    return correct/total
best=-1.; best_state=None; history=[]; t0=time.time()
for epoch in range(1,7):
    m.train(); correct=total=0; loss_sum=0.; et=time.time()
    for x,y in train_ld:
        opt.zero_grad(set_to_none=True); z=m(x); loss=lossfn(z,y); loss.backward(); opt.step(); correct+=(z.argmax(1)==y).sum().item(); total+=len(y); loss_sum+=loss.item()*len(y)
    va=accuracy(val_ld); sched.step(); row={'epoch':epoch,'train_accuracy':correct/total,'train_loss':loss_sum/total,'validation_accuracy':va,'seconds':time.time()-et}; history.append(row); print(row,flush=True)
    if va>best: best=va; best_state={k:v.detach().cpu().clone() for k,v in m.state_dict().items()}
m.load_state_dict(best_state); test_acc=accuracy(test_ld); print('test_accuracy',test_acc,flush=True)

order=[('c1.weight','c1.bias'),('c2.weight','c2.bias'),('f1.weight','f1.bias'),('f2.weight','f2.bias')]
buf=bytearray(b'DMW1')
for wk,bk in order:
    w=best_state[wk].numpy().astype(np.float32); b=best_state[bk].numpy().astype(np.float32); scale=float(np.max(np.abs(w))/127.0); q=np.clip(np.rint(w/scale),-127,127).astype(np.int8)
    buf += struct.pack('<fII',scale,q.size,b.size)+q.tobytes(order='C')+b.astype('<f4').tobytes(order='C')
ASSET.parent.mkdir(parents=True,exist_ok=True); ASSET.write_bytes(base64.b64encode(gzip.compress(bytes(buf),compresslevel=9)))
REPORT.write_text(json.dumps({'version':'2.0','seed':SEED,'parameters':sum(p.numel() for p in m.parameters()),'training':'MNIST + 45k synthetic printed digits per epoch with camera augmentation','fonts':len(fonts),'best_validation_accuracy':best,'test_accuracy_float32_mnist':test_acc,'history':history,'asset_bytes':ASSET.stat().st_size,'elapsed_seconds':time.time()-t0},indent=2))
print('wrote',ASSET,ASSET.stat().st_size,'bytes',flush=True)
