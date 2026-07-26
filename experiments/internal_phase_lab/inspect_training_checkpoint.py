from pathlib import Path
import json
import torch

from nam.models import factory


ROOT = Path(r"D:\Desktop\NAM\2026-06-21-21-10-40")
checkpoint = torch.load(
    ROOT / "packed_best_submodel_0.ckpt", map_location="cpu", weights_only=False
)

print("checkpoint keys:", sorted(checkpoint))
state = checkpoint["state_dict"]
for key, value in state.items():
    shape = tuple(value.shape) if hasattr(value, "shape") else type(value).__name__
    print(f"{key}: {shape}")

config = json.loads((ROOT / "config_model.json").read_text())
print("factory members:", [name for name in dir(factory) if not name.startswith("_")])
print("model config name:", config["net"]["name"])
