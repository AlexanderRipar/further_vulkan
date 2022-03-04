CD shaders

glslc.exe   tutorial.vert                                                 -O   -o tutorial.vert.spv
glslc.exe   tutorial.frag                                                 -O   -o tutorial.frag.spv
glslc.exe   buffer_copy.comp                                              -O   -o buffer_copy.comp.spv
glslc.exe   swapchain.comp                                                -O   -o swapchain.comp.spv
glslc.exe   simplex3d_slice.comp                                          -O   -o simplex3d_slice.comp.spv
glslc.exe   sdf_font.frag                                                 -O   -o sdf_font.frag.spv
glslc.exe   sdf_font.vert                                                 -O   -o sdf_font.vert.spv
glslc.exe   simplex3d.comp                                                -O   -o simplex3d.comp.spv
glslc.exe   simplex3d_layered.comp                                        -O   -o simplex3d_layered.comp.spv
glslc.exe   voxel_volume_trace.comp                                       -O   -o voxel_volume_trace.comp.spv
glslc.exe   voxel_volume_init_checkempty.comp    --target-env=vulkan1.1   -O   -o voxel_volume_init_checkempty.comp.spv
glslc.exe   voxel_volume_init_assignindex.comp   --target-env=vulkan1.1   -O   -o voxel_volume_init_assignindex.comp.spv
glslc.exe   voxel_volume_init_fillbricks.comp    --target-env=vulkan1.1   -O   -o voxel_volume_init_fillbricks.comp.spv
