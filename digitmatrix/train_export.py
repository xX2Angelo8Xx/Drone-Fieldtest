import os, gzip, struct, base64, json, random, time
from pathlib import Path
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import DataLoader, random_split
from torchvision import datasets, transforms

SEED=20260807
random.seed(SEED); np.random.seed(SEED); torch.manual_seed(SEED); torch.set_num_threads(max(1, min(4, os.cpu_count() or 2)))
ROOT=Path(__file__).resolve().parent
DATA=ROOT/'data'
ASSET=ROOT/'app/src/main/assets/model.b64'
REPORT=ROOT/'training_report.json'

train_tf=transforms.Compose([
    transforms.RandomAffine(degrees=15, translate=(0.12,0.12), scale=(0.78,1.18), shear=(-6,6,-4,4), fill=0),
    transforms.RandomApply([transforms.GaussianBlur(3,(0.1,1.0))],p=0.20),
    transforms.ToTensor(),
    transforms.Normalize((0.1307,), (0.3081,)),
])
plain_tf=transforms.Compose([transforms.ToTensor(),transforms.Normalize((0.1307,), (0.3081,))])

# Download official MNIST. A separate deterministic validation split is held out from the 60k training images.
full_aug=datasets.MNIST(DATA, train=True, download=True, transform=train_tf)
full_plain=datasets.MNIST(DATA, train=True, download=False, transform=plain_tf)
test=datasets.MNIST(DATA, train=False, download=True, transform=plain_tf)
g=torch.Generator().manual_seed(SEED)
indices=torch.randperm(len(full_aug),generator=g).tolist()
val_idx=indices[:5000]; train_idx=indices[5000:]
train_ds=torch.utils.data.Subset(full_aug,train_idx)
val_ds=torch.utils.data.Subset(full_plain,val_idx)
train_ld=DataLoader(train_ds,batch_size=256,shuffle=True,num_workers=2,persistent_workers=True)
val_ld=DataLoader(val_ds,batch_size=512,num_workers=2)
test_ld=DataLoader(test,batch_size=512,num_workers=2)

class CNN(nn.Module):
    def __init__(self):
        super().__init__()
        self.c1=nn.Conv2d(1,32,5,padding=2)
        self.c2=nn.Conv2d(32,64,3,padding=1)
        self.f1=nn.Linear(64*7*7,128)
        self.f2=nn.Linear(128,10)
    def forward(self,x):
        x=F.max_pool2d(F.relu(self.c1(x)),2)
        x=F.max_pool2d(F.relu(self.c2(x)),2)
        x=F.relu(self.f1(x.flatten(1)))
        return self.f2(x)

m=CNN()
opt=torch.optim.AdamW(m.parameters(),lr=1.0e-3,weight_decay=1e-4)
sched=torch.optim.lr_scheduler.CosineAnnealingLR(opt,T_max=5,eta_min=8e-5)
lossfn=nn.CrossEntropyLoss(label_smoothing=0.02)

def accuracy(ld):
    m.eval(); correct=total=0
    with torch.inference_mode():
        for x,y in ld:
            z=m(x); correct+=(z.argmax(1)==y).sum().item(); total+=len(y)
    return correct/total

best=-1.0; best_state=None; history=[]; t0=time.time()
for epoch in range(1,6):
    m.train(); correct=total=0; loss_sum=0.0; et=time.time()
    for x,y in train_ld:
        opt.zero_grad(set_to_none=True); z=m(x); loss=lossfn(z,y); loss.backward(); opt.step()
        correct+=(z.argmax(1)==y).sum().item(); total+=len(y); loss_sum+=loss.item()*len(y)
    va=accuracy(val_ld); sched.step()
    row={'epoch':epoch,'train_accuracy':correct/total,'train_loss':loss_sum/total,'validation_accuracy':va,'seconds':time.time()-et}
    history.append(row); print(row,flush=True)
    if va>best:
        best=va; best_state={k:v.detach().cpu().clone() for k,v in m.state_dict().items()}

m.load_state_dict(best_state)
test_acc=accuracy(test_ld)
print('test_accuracy',test_acc,flush=True)

# Symmetric per-tensor int8 weight quantization; biases remain float32. The Android app dequantizes once at startup.
order=[('c1.weight','c1.bias'),('c2.weight','c2.bias'),('f1.weight','f1.bias'),('f2.weight','f2.bias')]
buf=bytearray(b'DMW1')
for wk,bk in order:
    w=best_state[wk].numpy().astype(np.float32); b=best_state[bk].numpy().astype(np.float32)
    scale=float(np.max(np.abs(w))/127.0)
    q=np.clip(np.rint(w/scale),-127,127).astype(np.int8)
    buf += struct.pack('<fII',scale,q.size,b.size)
    buf += q.tobytes(order='C')
    buf += b.astype('<f4').tobytes(order='C')
ASSET.parent.mkdir(parents=True,exist_ok=True)
ASSET.write_bytes(base64.b64encode(gzip.compress(bytes(buf),compresslevel=9)))
REPORT.write_text(json.dumps({'seed':SEED,'parameters':sum(p.numel() for p in m.parameters()),'best_validation_accuracy':best,'test_accuracy_float32':test_acc,'history':history,'asset_bytes':ASSET.stat().st_size,'elapsed_seconds':time.time()-t0},indent=2))
print('wrote',ASSET,ASSET.stat().st_size,'bytes',flush=True)
