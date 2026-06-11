# Vulkan Test TODO

- 04: Textured cube (indexed geometry, depth testing)
- 05: Phong lighting (ambient + diffuse + specular, light direction uniform)
- 06: Normal mapping (TBN matrix, tangent as vertex attribute)
- 07: Push constants (per-draw data without descriptor overhead)
- 08: Offscreen rendering + fullscreen blit (render to texture, then display)
- 09: Shadow mapping — offscreen depth-only pass, sample in main pass as a
  texture; requires understanding of depth bias and shadow acne
- 10: Instanced rendering at scale — draw 10k objects with one call; explore
  the limit where UBO arrays lose to storage buffers
- 11: Compute shader — image write; requires pipeline barrier between compute
  and fragment reads
- 12: Ping-pong compute — two textures alternating as input/output each frame;
  foundation for fluid sim, bloom, SSAO
- 13: Deferred rendering — geometry pass writes position/normal/albedo to
  multiple attachments (MRT), lighting pass reads them; eliminates overdraw
  cost for many lights
- 14: Transparency / order-independent transparency — naive alpha blending
  breaks with depth; OIT via weighted blended is a tractable first approach
- 15: Skeletal animation — bone matrices in a UBO, skinning in the vertex
  shader; more a data pipeline problem than a GPU one
- 16: Dynamic rendering (VK_KHR_dynamic_rendering) — drop VkRenderPass and
  VkFramebuffer; simplifies init_program_spec significantly
- 17: Text rendering
