#pragma once

#include "framework_vulkan\framework_vulkan.h"

/*

  NOTE: Goals of this demo are the following:

    - Showing accurate pixel art rendering of sprites under zoom, rotation, translation

    Much of this is sourced from the below links:

    - https://www.shadertoy.com/view/MlB3D3
    - https://www.youtube.com/watch?v=Yu8k7a1hQuU&ab_channel=MollyRocket

  TODO: In the future, I can try the following:

    - https://www.shadertoy.com/view/XsjSzR (Timothy Lottes CRT Filter)
    - Render 3d geometry in pixel art (try using MSAA or if there are other sampling stuff to remove flicker artifacts)
        - We can try to render geometry to a sprite at low res and then sample it like the above sprites at a higher resolution using the
          sub pixel stuff
  
 */

struct demo_state
{
    linear_arena Arena;
    linear_arena TempArena;

    // NOTE: Samplers
    VkSampler PointSampler;
    VkSampler LinearSampler;
    
    // NOTE: Render Target Entries
    render_target_entry ColorEntry;
    render_target_entry DepthEntry;
    render_target GeometryRenderTarget;

    render_target_entry SwapChainEntry;
    render_target CopyToSwapTarget;
    render_fullscreen_pass CopyToSwapPass;
    VkDescriptorSetLayout CopyToSwapDescLayout;
    VkDescriptorSet CopyToSwapDescriptor;
    
    // NOTE: Models
    procedural_mesh Quad;

    // NOTE: Pixel Art Sprite Data
    u32 NumInstances;
    v3* Positions;
    f32* Rotations;
    f32* Scales;
    VkBuffer Transforms;
    vk_image NeonCatTexture;
    VkDescriptorSetLayout PixelArtSpriteLayout;
    VkDescriptorSet PixelArtSpritePointSet;
    VkDescriptorSet PixelArtSpriteLinearSet;
    vk_pipeline* PixelArtSpritePso;
};

global demo_state* DemoState;
