
#include "pixel_art_demo.h"

inline void DemoAllocGlobals(linear_arena* Arena)
{
    // IMPORTANT: These are always the top of the program memory
    DemoState = PushStruct(Arena, demo_state);
    RenderState = PushStruct(Arena, render_state);
}

DEMO_INIT(Init)
{
    // NOTE: Init Memory
    {
        linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
        DemoAllocGlobals(&Arena);
        *DemoState = {};
        *RenderState = {};
        DemoState->Arena = Arena;
        DemoState->TempArena = LinearSubArena(&DemoState->Arena, MegaBytes(10));
    }

    // NOTE: Init Vulkan
    {
        {
            render_init_params InitParams = {};
            InitParams.ValidationEnabled = true;
            InitParams.WindowWidth = WindowWidth;
            InitParams.WindowHeight = WindowHeight;
            InitParams.StagingBufferSize = MegaBytes(400);
            VkInit(VulkanLib, hInstance, WindowHandle, &DemoState->Arena, &DemoState->TempArena, InitParams);
        }
        
        // NOTE: Init descriptor pool
        {
            VkDescriptorPoolSize Pools[5] = {};
            Pools[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            Pools[0].descriptorCount = 1000;
            Pools[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            Pools[1].descriptorCount = 1000;
            Pools[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            Pools[2].descriptorCount = 1000;
            Pools[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            Pools[3].descriptorCount = 1000;
            Pools[4].type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            Pools[4].descriptorCount = 1000;
            
            VkDescriptorPoolCreateInfo CreateInfo = {};
            CreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            CreateInfo.maxSets = 1000;
            CreateInfo.poolSizeCount = ArrayCount(Pools);
            CreateInfo.pPoolSizes = Pools;
            VkCheckResult(vkCreateDescriptorPool(RenderState->Device, &CreateInfo, 0, &RenderState->DescriptorPool));
        }
    }
    
    // NOTE: Create samplers
    DemoState->PointSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f);
    DemoState->LinearSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f);
        
    // NOTE: Init render target entries
    DemoState->ColorEntry = RenderTargetEntryCreate(&RenderState->GpuArena, RenderState->WindowWidth, RenderState->WindowHeight,
                                                    VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                                    VK_IMAGE_ASPECT_COLOR_BIT);
    DemoState->SwapChainEntry = RenderTargetSwapChainEntryCreate(RenderState->WindowWidth, RenderState->WindowHeight,
                                                               RenderState->SwapChainFormat);
    DemoState->DepthEntry = RenderTargetEntryCreate(&RenderState->GpuArena, RenderState->WindowWidth, RenderState->WindowHeight,
                                                  VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                  VK_IMAGE_ASPECT_DEPTH_BIT);

    // NOTE: Geometry RT
    {
        render_target_builder Builder = RenderTargetBuilderBegin(&DemoState->Arena, &DemoState->TempArena, RenderState->WindowWidth,
                                                                 RenderState->WindowHeight);
        RenderTargetAddTarget(&Builder, &DemoState->ColorEntry, VkClearColorCreate(0, 0, 0, 1));
        RenderTargetAddTarget(&Builder, &DemoState->DepthEntry, VkClearDepthStencilCreate(1, 0));
                            
        vk_render_pass_builder RpBuilder = VkRenderPassBuilderBegin(&DemoState->TempArena);

        u32 ColorId = VkRenderPassAttachmentAdd(&RpBuilder, DemoState->ColorEntry.Format, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        u32 DepthId = VkRenderPassAttachmentAdd(&RpBuilder, DemoState->DepthEntry.Format, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        VkRenderPassSubPassBegin(&RpBuilder, VK_PIPELINE_BIND_POINT_GRAPHICS);
        VkRenderPassColorRefAdd(&RpBuilder, ColorId, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderPassDepthRefAdd(&RpBuilder, DepthId, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        VkRenderPassSubPassEnd(&RpBuilder);

        DemoState->GeometryRenderTarget = RenderTargetBuilderEnd(&Builder, VkRenderPassBuilderEnd(&RpBuilder, RenderState->Device));
    }

    // NOTE: Copy To Swap
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&DemoState->CopyToSwapDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        {
            DemoState->CopyToSwapDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, DemoState->CopyToSwapDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, DemoState->CopyToSwapDescriptor, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   DemoState->ColorEntry.View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        
        render_target_builder Builder = RenderTargetBuilderBegin(&DemoState->Arena, &DemoState->TempArena, RenderState->WindowWidth,
                                                                 RenderState->WindowHeight);
        RenderTargetAddTarget(&Builder, &DemoState->SwapChainEntry, VkClearColorCreate(0, 0, 0, 1));
        
        vk_render_pass_builder RpBuilder = VkRenderPassBuilderBegin(&DemoState->TempArena);

        u32 ColorId = VkRenderPassAttachmentAdd(&RpBuilder, RenderState->SwapChainFormat, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VkRenderPassSubPassBegin(&RpBuilder, VK_PIPELINE_BIND_POINT_GRAPHICS);
        VkRenderPassColorRefAdd(&RpBuilder, ColorId, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderPassSubPassEnd(&RpBuilder);

        DemoState->CopyToSwapTarget = RenderTargetBuilderEnd(&Builder, VkRenderPassBuilderEnd(&RpBuilder, RenderState->Device));
        DemoState->CopyToSwapPass = FullScreenPassCreate("shader_copy_to_swap_frag.spv", "main", &DemoState->CopyToSwapTarget, 1,
                                                         &DemoState->CopyToSwapDescLayout, 1, &DemoState->CopyToSwapDescriptor);
    }

    // NOTE: Pixel Art Sprite PSO
    {
        // NOTE: Create descriptor set layout
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&DemoState->PixelArtSpriteLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        // NOTE: Create Pipeline
        {
            vk_pipeline_builder Builder = VkPipelineBuilderBegin(&DemoState->TempArena);

            // NOTE: Shaders
            VkPipelineShaderAdd(&Builder, "sprite_model_vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT);
            VkPipelineShaderAdd(&Builder, "sprite_model_frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
                
            // NOTE: Specify input vertex data format
            VkPipelineVertexBindingBegin(&Builder);
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32B32_SFLOAT, sizeof(v3));
            VkPipelineVertexAttributeAddOffset(&Builder, sizeof(v3));
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32_SFLOAT, sizeof(v2));
            VkPipelineVertexBindingEnd(&Builder);

            VkPipelineInputAssemblyAdd(&Builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
            VkPipelineDepthStateAdd(&Builder, VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);
            
            // NOTE: Set the blending state
            VkPipelineColorAttachmentAdd(&Builder, VK_FALSE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
                                         VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO);

            VkDescriptorSetLayout Layouts[] =
            {
                DemoState->PixelArtSpriteLayout,
            };
            DemoState->PixelArtSpritePso = VkPipelineBuilderEnd(&Builder, RenderState->Device, &RenderState->PipelineManager,
                                                                DemoState->GeometryRenderTarget.RenderPass, 0, Layouts, ArrayCount(Layouts));
        }        
    }

    // NOTE: Pixel Art Sprite Data
    {
        DemoState->NumInstances = 1;
        DemoState->Positions = PushArray(&DemoState->Arena, v3, DemoState->NumInstances);
        DemoState->Rotations = PushArray(&DemoState->Arena, f32, DemoState->NumInstances);
        DemoState->Scales = PushArray(&DemoState->Arena, f32, DemoState->NumInstances);

        ZeroMem(DemoState->Positions, sizeof(v3)*DemoState->NumInstances);
        ZeroMem(DemoState->Rotations, sizeof(f32)*DemoState->NumInstances);

        for (u32 InstanceId = 0; InstanceId < DemoState->NumInstances; ++InstanceId)
        {
            DemoState->Scales[InstanceId] = 0.25f;
        }
                
        DemoState->Transforms = VkBufferCreate(RenderState->Device, &RenderState->GpuArena,
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                               sizeof(m4)*DemoState->NumInstances);
    }
    
    // NOTE: Upload assets
    vk_commands Commands = RenderState->Commands;
    VkCommandsBegin(RenderState->Device, Commands);
    {
        // NOTE: Push geometry
        DemoState->Quad = AssetsPushQuad();
        
        // NOTE: Push pixel art sprite
        DemoState->NeonCatTexture = TextureLoad("textures\\neon_cat.png", VK_FORMAT_R8G8B8A8_UNORM, false, sizeof(u8), 4);
        // NOTE: Point sampler
        {
            DemoState->PixelArtSpritePointSet = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, DemoState->PixelArtSpriteLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, DemoState->PixelArtSpritePointSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   DemoState->NeonCatTexture.View, DemoState->PointSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, DemoState->PixelArtSpritePointSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    DemoState->Transforms);
        }

        // NOTE: Linear sampler
        {
            DemoState->PixelArtSpriteLinearSet = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, DemoState->PixelArtSpriteLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, DemoState->PixelArtSpriteLinearSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   DemoState->NeonCatTexture.View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, DemoState->PixelArtSpriteLinearSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    DemoState->Transforms);
        }
        
        VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
        VkTransferManagerFlush(&RenderState->TransferManager, RenderState->Device, Commands.Buffer, &RenderState->BarrierManager);
    }
    
    VkCommandsSubmit(RenderState->GraphicsQueue, Commands);
}

DEMO_DESTROY(Destroy)
{
}

DEMO_CODE_RELOAD(CodeReload)
{
    linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
    // IMPORTANT: We are relying on the memory being the same here since we have the same base ptr with the VirtualAlloc so we just need
    // to patch our global pointers here
    DemoAllocGlobals(&Arena);

    VkGetGlobalFunctionPointers(VulkanLib);
    VkGetInstanceFunctionPointers();
    VkGetDeviceFunctionPointers();
}

DEMO_MAIN_LOOP(MainLoop)
{
    u32 ImageIndex;
    VkCheckResult(vkAcquireNextImageKHR(RenderState->Device, RenderState->SwapChain,
                                        UINT64_MAX, RenderState->ImageAvailableSemaphore,
                                        VK_NULL_HANDLE, &ImageIndex));
    DemoState->SwapChainEntry.View = RenderState->SwapChainViews[ImageIndex];
    
    vk_commands Commands = RenderState->Commands;
    VkCommandsBegin(RenderState->Device, Commands);

    // NOTE: Update pipelines
    VkPipelineUpdateShaders(RenderState->Device, &RenderState->CpuArena, &RenderState->PipelineManager);

    RenderTargetUpdateEntries(&DemoState->Arena, &DemoState->CopyToSwapTarget);

    // NOTE: Update instances
    {
        for (u32 InstanceId = 0; InstanceId < DemoState->NumInstances; ++InstanceId)
        {
            DemoState->Positions;
        }
    }
    
    // NOTE: Upload uniforms
    {
        // NOTE: Push instance buffer
        {
            m4* Data = VkTransferPushWriteArray(&RenderState->TransferManager, DemoState->Transforms, m4, DemoState->NumInstances,
                                                BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                BarrierMask(VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT));

            local_global f32 T = 0.0f;

            v3 OffsetPos = 0.5f*V3(Sin(T), Cos(T), 0.0f);
            T += 0.0001f;
            if (T > 2.0f*Pi32)
            {
                T = 0.0f;
            }

            OffsetPos = V3(0);
            
            for (u32 InstanceId = 0; InstanceId < DemoState->NumInstances; ++InstanceId, ++Data)
            {
                f32 Scale = 3.0f*DemoState->Scales[InstanceId];
                f32 Rotation = DemoState->Rotations[InstanceId];

                Rotation = 2.0f*T; //Pi32/2.0f;
                
                *Data = {};
                *Data = (M4Pos(DemoState->Positions[InstanceId] + OffsetPos)*
                         M4Rotation(0, 0, Rotation)*
                         M4Scale(V3(Scale, Scale, Scale)));
            }
        }

        VkTransferManagerFlush(&RenderState->TransferManager, RenderState->Device, RenderState->Commands.Buffer, &RenderState->BarrierManager);
    }

    // NOTE: Draw PBR Textures
    RenderTargetRenderPassBegin(&DemoState->GeometryRenderTarget, Commands, RenderTargetRenderPass_SetViewPort | RenderTargetRenderPass_SetScissor);
    {
        vkCmdBindPipeline(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->PixelArtSpritePso->Handle);
        VkDescriptorSet DescriptorSets[] =
        {
            //DemoState->PixelArtSpritePointSet,
            DemoState->PixelArtSpriteLinearSet,
        };
        vkCmdBindDescriptorSets(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->PixelArtSpritePso->Layout, 0,
                                ArrayCount(DescriptorSets), DescriptorSets, 0, 0);

        VkDeviceSize Offset = 0;
        vkCmdBindVertexBuffers(Commands.Buffer, 0, 1, &DemoState->Quad.Vertices, &Offset);
        vkCmdBindIndexBuffer(Commands.Buffer, DemoState->Quad.Indices, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(Commands.Buffer, DemoState->Quad.NumIndices, DemoState->NumInstances, 0, 0, 0);
    }
    RenderTargetRenderPassEnd(Commands);
    
    FullScreenPassRender(Commands, &DemoState->CopyToSwapPass);

    VkCheckResult(vkEndCommandBuffer(Commands.Buffer));
                    
    // NOTE: Render to our window surface
    // NOTE: Tell queue where we render to surface to wait
    VkPipelineStageFlags WaitDstMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.pWaitSemaphores = &RenderState->ImageAvailableSemaphore;
    SubmitInfo.pWaitDstStageMask = &WaitDstMask;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &Commands.Buffer;
    SubmitInfo.signalSemaphoreCount = 1;
    SubmitInfo.pSignalSemaphores = &RenderState->FinishedRenderingSemaphore;
    VkCheckResult(vkQueueSubmit(RenderState->GraphicsQueue, 1, &SubmitInfo, Commands.Fence));
            
    VkPresentInfoKHR PresentInfo = {};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = &RenderState->FinishedRenderingSemaphore;
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = &RenderState->SwapChain;
    PresentInfo.pImageIndices = &ImageIndex;
    VkResult Result = vkQueuePresentKHR(RenderState->PresentQueue, &PresentInfo);

    switch (Result)
    {
        case VK_SUCCESS:
        {
        } break;

        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
        {
            // NOTE: Window size changed
            InvalidCodePath;
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }
}
