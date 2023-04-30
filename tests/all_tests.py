import os
import subprocess
from PIL import Image

scene_files = os.listdir('scenes/')

def raytrace(scenefile):
    subprocess.run(["RayTracer.exe", scenefile], cwd='temp/')

# raytrace test cases
for file in scene_files:
    raytrace('../scenes/' + file)
    print()

# convert the frame from PPM to PNG
ppm_files = os.listdir('temp/')

for file in ppm_files:
    im = Image.open(f"temp/{file}")
    im.save(f"temp/{file.split('.')[0]}.png")
