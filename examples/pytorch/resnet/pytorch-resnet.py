import torch
import time
import argparse
import torchvision.models as models
from torch.profiler import profile, record_function, ProfilerActivity

model = models.resnet50()
model.eval()
inputs = torch.randn(32, 3, 224, 224)

parser = argparse.ArgumentParser()
parser.add_argument("--ipex", default=False, action="store_true", help="enable ipex or not.")

args = parser.parse_args()

if args.ipex :
    import intel_extension_for_pytorch as ipex
    model = ipex.optimize(model)

loop = 100
with profile(activities=[ProfilerActivity.CPU], record_shapes=True) as prof:
    # warm up
    for x in range(10):
        with record_function("model_inference_%d"%(x)):
            model(inputs)
    start = time.time()
    for x in range(loop):
        with record_function("model_inference_%d"%(x)):
            model(inputs)
    end = time.time()
    print("time:", (end - start)/loop)
