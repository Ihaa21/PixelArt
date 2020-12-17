# PixelArt

Demonstrates a shader for rendering pixel art on higher resolution displays without flickering artifacts. Supports smooth translation and rotation of pixel art. The idea behind the effect is to only do blending in pixels that have more than one texel overlapped, and to calculate each texels coverage to do the blending. The technique uses the blending hardware for bilinear and we just modify the input uvs to achieve the desired blend. This comes from a video by Casey Muratori here: https://www.youtube.com/watch?v=Yu8k7a1hQuU&ab_channel=MollyRocket . You can find all the glsl shader code in shader_sprite_model.cpp.

# Todo

- Add scaling support for pixel art via mip maps
- Optimize the shader to not use branches

# How to Build

- clone with --recurse-submodules
- add assimp to the lib folder or comment out all assimp references in the framework_vulkan code
- Run code/build.bat (you can adjust the commands here for shaders and c++ code like O0 or O2). 
- Open the exe in visual studio as a project and set the working directory to the data folder. You can use the sln for debugging the project.
