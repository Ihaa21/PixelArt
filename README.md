# PixelArt

Demonstrates a shader for rendering pixel art on higher resolution displays without flickering artifacts. Supports smooth translation and rotation of pixel art. The idea behind the effect is to only do blending in pixels that have more than one texel overlapped, and to calculate each texels coverage to do the blending. The technique uses the blending hardware for bilinear and we just modify the input uvs to achieve the desired blend. This comes from a video by Casey Muratori here: https://www.youtube.com/watch?v=Yu8k7a1hQuU&ab_channel=MollyRocket

# Todo

- Add scaling support for pixel art via mip maps
- Optimize the shader to not use branches

# How To Build

Run build.bat in the code folder to generate a exe in the build folder. You can then create a visual studio project out of the exe and set the working directory to the data directory and run the application. 
