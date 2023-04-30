import subprocess
import os
import numpy as np
from PIL import Image

config = {

    # point in world space to rotate around
    'rotationCenter' : [0, 0, -10],

    # length of the animation (seconds)
    'animationLength': 5,

    # number of times to repeat the animation in the video (currently unused)
    'numRepeats': 5,

    # number of video frames per second
    'fps': 60
}

def animate():

    # make a directory for temporary files
    if not os.path.exists('temp'):
        os.makedirs('temp')

    # create a series of scenes for each frame of the video
    framecount = config['animationLength'] * config["fps"]
    for framenumber in range(0, framecount):
        print(f"\nProcessing frame ({framenumber + 1}/{framecount})")

        # rotate the scene
        scene = get_base_scene()
        rotation = framenumber * (360 / framecount)
        print(f"Current rotation: {rotation}")
        rotated_scene = rotate_scene(scene, rotation)

        # set the output image name
        formattednum = '{n:05d}'.format(n=framenumber)
        rotated_scene['output'] = f"frame_{formattednum}.ppm"

        # save a temporary scene file
        write_scene_to_file(f"temp/frame_{formattednum}.txt", rotated_scene)

        # render the scene objects with the raytracer executable
        raytrace(f"frame_{formattednum}.txt")

        # convert the frame from PPM to PNG
        im = Image.open(f"temp/frame_{formattednum}.ppm")
        im.save(f"temp/frame_{formattednum}.png")

    # stitch the frames together with ffmpeg
    stitch_video()

def get_base_scene():
    return {
        'near': 1,
        'left': -1.8,
        'right': 1.8,
        'bottom': -1,
        'top': 1,
        'res': { 'x': 1920, 'y': 1080 },
        #'res': { 'x': 192, 'y': 108 },
        'spheres': [
            {
                'name': "s1",
                'pos': [4, 1, -10],
                'scl': [2, 2, 2],
                'color': [0.5, 0, 0],
                'illum': [0, 0, 1, 0, 100]
            },
            {
                'name': "s2",
                'pos': [0, 0, -10],
                'scl': [2, 2, 1],
                'color': [0, 0.5, 0],
                'illum': [0, 0, 1, 0, 10]
            },
            {
                'name': "s3",
                'pos': [-4, 2, -10],
                'scl': [2, 2, 2],
                'color': [0, 0, 0.5],
                'illum': [0, 0, 1, 0, 1000]
            },
        ],
        'lights': [
            {
                'name': "l1",
                'pos': [0, -5, 0],
                'color': [0.9, 0, 0]
            },
            {
                'name': "l2",
                'pos': [10, 5, 0],
                'color': [0, 0.9, 0]
            },
            {
                'name': "l3",
                'pos': [-10, 5, 0],
                'color': [0, 0, 0.9]
            }
        ],
        'back': [1, 1, 1],
        'ambient': [0.85, 0.85, 0.85],
        'output': "test.ppm"
    }

def rotate(theta):
    c, s = np.cos(theta), np.sin(theta)
    return np.array((
        (  c,  0,  s,  0 ),
        (  0,  1,  0,  0 ),
        ( -s,  0,  c,  0 ),
        (  0,  0,  0,  1 )
    ))

def translate(x, y, z):
    return np.array((
        (  1,  0,  0,  x ),
        (  0,  1,  0,  y ),
        (  0,  0,  1,  z ),
        (  0,  0,  0,  1 )
    ))

def rotate_scene(scene, degrees):
    theta = np.radians(degrees)
    center = config['rotationCenter']

    # create a transformation matrix to rotate the scene
    translation1 = translate(-center[0], -center[1], -center[2])
    rotation = rotate(theta)
    translation2 = translate(center[0], center[1], center[2])
    transformation = np.matmul(translation2, np.matmul(rotation, translation1))
    
    # rotate each sphere using the matrix
    for i in range(len(scene['spheres'])):
        pos = scene['spheres'][i]['pos']
        pos_homogenous = np.array([pos[0], pos[1], pos[2], 1])
        newpos = transformation.dot(pos_homogenous)
        scene['spheres'][i]['pos'] = newpos[:3]

        """
        scl = scene['spheres'][i]['scl']
        scl_homogenous = np.array([scl[0], scl[1], scl[2], 1])
        newscl = rotate(theta).transpose().dot(scl_homogenous)
        scene['spheres'][i]['scl'] = newscl[:3]
        """

    # rotate each light using the matrix
    for i in range(len(scene['lights'])):
        pos = scene['lights'][i]['pos']
        pos_homogenous = np.array([pos[0], pos[1], pos[2], 1])
        newpos = transformation.dot(pos_homogenous)
        scene['lights'][i]['pos'] = newpos[:3]

    return scene

def write_scene_to_file(filepath, scene):
    with open(filepath, 'w') as f:

        # image plane info
        f.write(f"NEAR {scene['near']}\n")
        f.write(f"LEFT {scene['left']}\n")
        f.write(f"RIGHT {scene['right']}\n")
        f.write(f"BOTTOM {scene['bottom']}\n")
        f.write(f"TOP {scene['top']}\n")
        f.write(f"RES {scene['res']['x']} {scene['res']['y']}\n")

        # spheres
        # TODO: clean this up
        for sphere in scene['spheres']:
            f.write(f"SPHERE {sphere['name']} ")
            f.write(f"{sphere['pos'][0]} {sphere['pos'][1]} {sphere['pos'][2]} ")
            f.write(f"{sphere['scl'][0]} {sphere['scl'][1]} {sphere['scl'][2]} ")
            f.write(f"{sphere['color'][0]} {sphere['color'][1]} {sphere['color'][2]} ")
            f.write(f"{sphere['illum'][0]} {sphere['illum'][1]} {sphere['illum'][2]} {sphere['illum'][3]} {sphere['illum'][4]}")
            f.write("\n")

        # lights
        for light in scene['lights']:
            f.write(f"LIGHT {light['name']} ")
            f.write(f"{light['pos'][0]} {light['pos'][1]} {light['pos'][2]} ")
            f.write(f"{light['color'][0]} {light['color'][1]} {light['color'][2]}\n")

        # misc
        f.write(f"BACK {scene['back'][0]} {scene['back'][1]} {scene['back'][2]}\n")
        f.write(f"AMBIENT {scene['ambient'][0]} {scene['ambient'][1]} {scene['ambient'][2]}\n")
        f.write(f"OUTPUT {scene['output']}\n")

def raytrace(scenefile):
    subprocess.run(["bin/RayTracer.exe", scenefile], cwd="./temp")
    
def stitch_video():
    subprocess.run(
        [
            "bin/ffmpeg.exe",
            "-i",
            "temp/frame_%05d.png",
            "-c:v",
            "libx264",
            "-vf",
            f"fps={config['fps']}",
            "-pix_fmt",
            "yuv420p",
            "out.mp4",
            "-y"
        ],
    )

if __name__ == "__main__":
    animate()
