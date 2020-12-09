#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(set = 0, binding = 0) uniform sampler2D ColorTexture;
layout(set = 0, binding = 1) buffer transforms
{
    mat4 Transforms[];
};

#if VERTEX_SHADER

layout(location = 0) in vec3 InPos;
layout(location = 1) in vec2 InUv;

layout(location = 0) out vec2 OutUv;

void main()
{
    mat4 WVPTransform = Transforms[gl_InstanceIndex];
    gl_Position = WVPTransform * vec4(InPos, 1);
    OutUv = InUv;
}

#endif

#if FRAGMENT_SHADER

layout(location = 0) in vec2 InUv;

layout(location = 0) out vec4 OutColor;

vec2 ConvertToTexels(vec2 Uv)
{
    vec2 Result = Uv * vec2(textureSize(ColorTexture, 0)) - vec2(0.5);
    return Result;
}

vec2 ConvertToUv(vec2 Texels)
{
    vec2 Result = (Texels + vec2(0.5)) / vec2(textureSize(ColorTexture, 0));
    return Result;
}

void main()
{
    /*
      NOTE: Normally, when we sample textures, we assume that they represent some higher detail object so we use filtering to try and
            reconstruct it better. In the case of pixel art, this assumption is wrong, the texture we have is all the data. When we
            render pixel art with point sampling, we get a lot of flicker however since we only move in fixed pixel increments which are
            still visible, even at 1080p.

            To fix this, we want to do some blending only on the fragments that have more than one texel on them from the pixel art.
            Fragments that have just one texel receive no blending (which isn't true with regular bilinear). The idea is to make the pixel
            art not move in fixed pixel strides but add subpixel motion/rendering to it.

            The algorithm calculates the coverage of the multiple texel fragments for each texel that collides, and uses that to do
            blending that is correct in the case of fixed translations/scaling. In the case of rotation, this approximates the result
            I think.

            We still use bilinear blending but we want to change the uv coordinates such that we achieve the math we want from above.

      NOTE: Derivation:

            Bilinear1d(t, C0, C1):
            
                (1 - t)*C0 + t*C1
                
            Bilinear2d(u, v, C0, C1, C2, C3): 

                B0 = (1 - u)*C0 + u*C1
                B1 = (1 - u)*C2 + u*C3
                Result = (1 - v)*B0 + v*B1
            
            In the case that a fragment is entirely inside a fragment, we just want that texels color. We can do this via the following:

              vec2 uv = ConverToUv(ConvertToPixels(uv) - fract(ConvertToPixels(uv)))

            In the case that a fragment contains more than one texel, we first derive what happens in 1d:

              f32 

        TODO: Fails when we rotate
        TODO: Implement without the if statement
        TODO: Why does the derivative appear to be incorrect for some rotations? It just goes out empty for dFdxFine and dFdyFine
              
     */

    vec2 Test = dFdyFine(InUv);
    
    //vec2 DUv = vec2(dFdx(InUv.x), dFdy(InUv.y)); // NOTE: Works for no rotation
    vec2 DUv = dFdx(InUv)  + dFdy(InUv);
    
    // TODO: Right now we assume everything is sized well so we don't need mip levels
    vec2 LeftUv = InUv - 0.5*DUv;
    vec2 RightUv = InUv + 0.5*DUv;

    // NOTE: The bias of -0.5 is because tahts where the texel centers are but we are working in whole texel centers
    vec2 LeftTexel = ConvertToTexels(LeftUv);
    vec2 RightTexel = ConvertToTexels(RightUv);

    uvec2 LeftTexelAbsolute = uvec2(floor(LeftTexel));
    uvec2 RightTexelAbsolute = uvec2(floor(RightTexel));

    vec2 OutUv = vec2(0, 0);
    if (LeftTexelAbsolute.x == RightTexelAbsolute.x)
    {
        OutUv.x = LeftTexelAbsolute.x;
    }
    else
    {
        float a = (ceil(LeftTexel.x) - LeftTexel.x) / (RightTexel.x - LeftTexel.x);
        // NOTE: We can lerp the uv and avoid conversion
        // TODO: Why is it 1-a and not just a?
        OutUv.x = mix(float(LeftTexelAbsolute.x), float(RightTexelAbsolute.x), 1-a);
    }

    if (LeftTexelAbsolute.y == RightTexelAbsolute.y)
    {
        OutUv.y = LeftTexelAbsolute.y;
    }
    else
    {
        float a = (ceil(LeftTexel.y) - LeftTexel.y) / (RightTexel.y - LeftTexel.y);
        // NOTE: We can lerp the uv and avoid conversion
        OutUv.y = mix(float(LeftTexelAbsolute.y), float(RightTexelAbsolute.y), 1-a);
    }
    
    OutUv = ConvertToUv(OutUv);
    vec4 TextureColor = texture(ColorTexture, OutUv);
    OutColor = TextureColor;
    OutColor = vec4(1000000.0*Test, 0, 1);
    //OutColor = vec4(InUv, 0, 1);
}

#endif
