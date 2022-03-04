#include "gpu_info.h"

#include "vulkan_base.h"

#include "och_fmt.h"

struct gpu_info
{
	och::status run(int argc, const char** argv)
	{
		VkInstance instance;

		och::iohandle output_file = och::get_stdout();

		// Open output file
		if (argc > 2)
			check(och::open_file(output_file, argv[2], och::fio::access::write, och::fio::open::truncate, och::fio::open::normal));

		uint32_t supported_api_version = VK_API_VERSION_1_0;

		// Find highest supported instance version.
		{
			PFN_vkEnumerateInstanceVersion instance_version_fn = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));

			if (instance_version_fn == nullptr)
				return to_status(och::error::argument_too_large);

			check(instance_version_fn(&supported_api_version));
		}

		// Check for supported instance layers.

		uint32_t available_instance_layer_cnt;

		check(vkEnumerateInstanceLayerProperties(&available_instance_layer_cnt, nullptr));

		heap_buffer<VkLayerProperties> available_instance_layers(available_instance_layer_cnt);

		check(vkEnumerateInstanceLayerProperties(&available_instance_layer_cnt, available_instance_layers.data()));

		// Check for supported instance extensions.

		uint32_t available_instance_extension_cnt;
		
		check(vkEnumerateInstanceExtensionProperties(nullptr, &available_instance_extension_cnt, nullptr));
		
		heap_buffer<VkExtensionProperties> available_instance_extensions(available_instance_extension_cnt);
		
		check(vkEnumerateInstanceExtensionProperties(nullptr, &available_instance_extension_cnt, available_instance_extensions.data()));

		heap_buffer<const char*> available_instance_extension_names(available_instance_extension_cnt);
		
		for (uint32_t i = 0; i != available_instance_extension_cnt; ++i)
			available_instance_extension_names[i] = available_instance_extensions[i].extensionName;



		VkApplicationInfo application_info{};
		application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		application_info.pNext = nullptr;
		application_info.pApplicationName = "gpu info";
		application_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
		application_info.pEngineName = "no engine";
		application_info.engineVersion = VK_MAKE_VERSION(0, 0, 0);
		application_info.apiVersion = supported_api_version;

		VkInstanceCreateInfo instance_ci{};
		instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_ci.pNext = nullptr;
		instance_ci.flags = 0;
		instance_ci.pApplicationInfo = &application_info;
		instance_ci.enabledLayerCount = 0;
		instance_ci.ppEnabledLayerNames = nullptr;
		instance_ci.enabledExtensionCount = available_instance_extension_cnt;
		instance_ci.ppEnabledExtensionNames = available_instance_extension_names.data();
		
		check(vkCreateInstance(&instance_ci, nullptr, &instance));
		
		och::print(output_file,
			"########################################################################\n"
			"############################### Instance ###############################\n"
			"########################################################################\n"
			"\n"
			"Vulkan API version: {}.{}.{}\n"
			"Supported Instance Layers:\n\n", 
			VK_API_VERSION_MAJOR(supported_api_version), VK_API_VERSION_MINOR(supported_api_version), VK_API_VERSION_PATCH(supported_api_version));

		for (uint32_t i = 0; i != available_instance_layer_cnt; ++i)
			och::print(output_file,
				"     {}\n"
				"          Description:            {}\n",
				"          Spec Version:           {}\n",
				"          Implementation Version: {}\n\n",
				available_instance_layers[i].layerName, 
				available_instance_layers[i].description, 
				available_instance_layers[i].specVersion, 
				available_instance_layers[i].implementationVersion);

		och::print("Supported Instance Extensions:\n\n");

		for (uint32_t i = 0; i != available_instance_extension_cnt; ++i)
			och::print(output_file, 
				"     {}\n"
				"          Spec Version: {}\n\n",
				available_instance_extensions[i].extensionName,
				available_instance_extensions[i].specVersion);

		uint32_t physical_device_cnt;

		check(vkEnumeratePhysicalDevices(instance, &physical_device_cnt, nullptr));

		heap_buffer<VkPhysicalDevice> physical_devices(physical_device_cnt);

		check(vkEnumeratePhysicalDevices(instance, &physical_device_cnt, physical_devices.data()));

		if (VK_VERSION_MAJOR(supported_api_version) != 1 || VK_VERSION_MINOR(supported_api_version) != 0)
		{
			VkPhysicalDeviceAccelerationStructurePropertiesKHR accel_props;
			accel_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
			accel_props.pNext = nullptr;

			VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT blend_props;
			blend_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT;
			blend_props.pNext = &accel_props;

			VkPhysicalDeviceConservativeRasterizationPropertiesEXT conservative_raster_props;
			conservative_raster_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT;
			conservative_raster_props.pNext = &blend_props;

			VkPhysicalDeviceCooperativeMatrixPropertiesNV cooperative_matrix_props_nv;
			cooperative_matrix_props_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_NV;
			cooperative_matrix_props_nv.pNext = &conservative_raster_props;

			VkPhysicalDeviceCustomBorderColorPropertiesEXT custom_border_color_props;
			custom_border_color_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT;
			custom_border_color_props.pNext = &cooperative_matrix_props_nv;

			VkPhysicalDeviceDepthStencilResolveProperties depth_stencil_resolve_props;
			depth_stencil_resolve_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;
			depth_stencil_resolve_props.pNext = &custom_border_color_props;

			VkPhysicalDeviceDescriptorIndexingProperties descriptor_indexing_props;
			descriptor_indexing_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
			descriptor_indexing_props.pNext = &depth_stencil_resolve_props;

			VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV device_generated_command_properties_nv;
			device_generated_command_properties_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV;
			device_generated_command_properties_nv.pNext = &descriptor_indexing_props;

			VkPhysicalDeviceDiscardRectanglePropertiesEXT discard_rectangle_props;
			discard_rectangle_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT;
			discard_rectangle_props.pNext = &device_generated_command_properties_nv;

			VkPhysicalDeviceDriverProperties driver_props;
			driver_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
			driver_props.pNext = &discard_rectangle_props;

			// Missing: VkPhysicalDeviceDrmPropertiesEXT // VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT

			VkPhysicalDeviceExternalMemoryHostPropertiesEXT external_memory_host_props;
			external_memory_host_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;
			external_memory_host_props.pNext = &driver_props;

			VkPhysicalDeviceFloatControlsProperties float_controls_props;
			float_controls_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES;
			float_controls_props.pNext = &external_memory_host_props;

			VkPhysicalDeviceFragmentDensityMap2PropertiesEXT fragment_density_map_2_props;
			fragment_density_map_2_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_PROPERTIES_EXT;
			fragment_density_map_2_props.pNext = &float_controls_props;

			// Missing: VkPhysicalDeviceFragmentDensityMapOffsetPropertiesQCOM // VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_PROPERTIES_QCOM

			VkPhysicalDeviceFragmentDensityMapPropertiesEXT fragment_density_map_props;
			fragment_density_map_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT;
			fragment_density_map_props.pNext = &fragment_density_map_2_props;

			VkPhysicalDeviceFragmentShadingRateEnumsPropertiesNV fragment_shading_rate_enums_props_nv;
			fragment_shading_rate_enums_props_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_PROPERTIES_NV;
			fragment_shading_rate_enums_props_nv.pNext = &fragment_density_map_props;

			VkPhysicalDeviceFragmentShadingRatePropertiesKHR fragment_shading_rate_props;
			fragment_shading_rate_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR;
			fragment_shading_rate_props.pNext = &fragment_shading_rate_enums_props_nv;

			VkPhysicalDeviceIDProperties id_props;
			id_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
			id_props.pNext = &fragment_shading_rate_props;

			VkPhysicalDeviceInlineUniformBlockPropertiesEXT inline_uniform_block_props;
			inline_uniform_block_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT;
			inline_uniform_block_props.pNext = &id_props;

			VkPhysicalDeviceLineRasterizationPropertiesEXT line_rasterization_props;
			line_rasterization_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_EXT;
			line_rasterization_props.pNext = &inline_uniform_block_props;

			VkPhysicalDeviceMaintenance3Properties maintenance_3_props;
			maintenance_3_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
			maintenance_3_props.pNext = &line_rasterization_props;

			// Missing: VkPhysicalDeviceMaintenance4Properties // VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES

			VkPhysicalDeviceMeshShaderPropertiesNV mesh_shader_props_nv;
			mesh_shader_props_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV;
			mesh_shader_props_nv.pNext = &maintenance_3_props;

			// Missing: VkPhysicalDeviceMultiDrawPropertiesEXT // VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_PROPERTIES_EXT

			VkPhysicalDeviceMultiviewPerViewAttributesPropertiesNVX multi_view_per_view_attributes_props_nvx;
			multi_view_per_view_attributes_props_nvx.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX;
			multi_view_per_view_attributes_props_nvx.pNext = &mesh_shader_props_nv;

			VkPhysicalDeviceMultiviewProperties multi_view_props;
			multi_view_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;
			multi_view_props.pNext = &multi_view_per_view_attributes_props_nvx;

			VkPhysicalDevicePCIBusInfoPropertiesEXT pci_bus_info_props;
			pci_bus_info_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT;
			pci_bus_info_props.pNext = &multi_view_props;

			VkPhysicalDevicePerformanceQueryPropertiesKHR performance_query_props;
			performance_query_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_PROPERTIES_KHR;
			performance_query_props.pNext = &pci_bus_info_props;

			VkPhysicalDevicePointClippingProperties point_clipping_props;
			point_clipping_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES;
			point_clipping_props.pNext = &performance_query_props;

			// Missing: VkPhysicalDevicePortabilitySubsetPropertiesKHR // VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR

			VkPhysicalDeviceProtectedMemoryProperties protected_memory_props;
			protected_memory_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES;
			protected_memory_props.pNext = &point_clipping_props;

			// Missing: VkPhysicalDeviceProvokingVertexPropertiesEXT // VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_PROPERTIES_EXT

			VkPhysicalDevicePushDescriptorPropertiesKHR push_descriptor_props;
			push_descriptor_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
			push_descriptor_props.pNext = &protected_memory_props;

			VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_pipeline_props;
			ray_tracing_pipeline_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
			ray_tracing_pipeline_props.pNext = &push_descriptor_props;

			VkPhysicalDeviceRayTracingPropertiesNV ray_tracing_props_nv;
			ray_tracing_props_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
			ray_tracing_props_nv.pNext = &ray_tracing_pipeline_props;

			VkPhysicalDeviceRobustness2PropertiesEXT robustness_2_props;
			robustness_2_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT;
			robustness_2_props.pNext = &ray_tracing_props_nv;

			VkPhysicalDeviceSampleLocationsPropertiesEXT sample_locations_props;
			sample_locations_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT;
			sample_locations_props.pNext = &robustness_2_props;

			VkPhysicalDeviceSamplerFilterMinmaxProperties sampler_filter_minmax_props;
			sampler_filter_minmax_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES;
			sampler_filter_minmax_props.pNext = &sample_locations_props;

			VkPhysicalDeviceShaderCoreProperties2AMD shader_core_props_2_amd;
			shader_core_props_2_amd.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD;
			shader_core_props_2_amd.pNext = &sampler_filter_minmax_props;

			VkPhysicalDeviceShaderCorePropertiesAMD shader_core_props_amd;
			shader_core_props_amd.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD;
			shader_core_props_amd.pNext = &shader_core_props_2_amd;

			// Missing: VkPhysicalDeviceShaderIntegerDotProductProperties // VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_PROPERTIES

			VkPhysicalDeviceShaderSMBuiltinsPropertiesNV shader_sm_builtins_props_nv;
			shader_sm_builtins_props_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_NV;
			shader_sm_builtins_props_nv.pNext = &shader_core_props_amd;

			VkPhysicalDeviceShadingRateImagePropertiesNV shading_rate_image_props_nv;
			shading_rate_image_props_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_PROPERTIES_NV;
			shading_rate_image_props_nv.pNext = &shader_sm_builtins_props_nv;

			VkPhysicalDeviceSubgroupProperties subgroup_props;
			subgroup_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
			subgroup_props.pNext = &shading_rate_image_props_nv;

			VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroup_size_control_props;
			subgroup_size_control_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
			subgroup_size_control_props.pNext = &subgroup_props;

			// Missing: VkPhysicalDeviceSubpassShadingPropertiesHUAWEI // VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_SHADING_PROPERTIES_HUAWEI

			VkPhysicalDeviceTexelBufferAlignmentPropertiesEXT texel_buffer_alignment_props;
			texel_buffer_alignment_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES_EXT;
			texel_buffer_alignment_props.pNext = &subgroup_size_control_props;

			VkPhysicalDeviceTimelineSemaphoreProperties timeline_semaphore_props;
			timeline_semaphore_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES;
			timeline_semaphore_props.pNext = &texel_buffer_alignment_props;

			VkPhysicalDeviceTransformFeedbackPropertiesEXT transform_feedback_props;
			transform_feedback_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT;
			transform_feedback_props.pNext = &timeline_semaphore_props;

			VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT vertex_attribute_divisor_props;
			vertex_attribute_divisor_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT;
			vertex_attribute_divisor_props.pNext = &transform_feedback_props;

			VkPhysicalDeviceVulkan11Properties vulkan_1_1_props;
			vulkan_1_1_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
			vulkan_1_1_props.pNext = &vertex_attribute_divisor_props;

			VkPhysicalDeviceVulkan12Properties vulkan_1_2_props;
			vulkan_1_2_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
			vulkan_1_2_props.pNext = &vulkan_1_1_props;

			// Missing: VkPhysicalDeviceVulkan13Properties // VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES

			VkPhysicalDeviceProperties2 physical_device_props2;
			physical_device_props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			physical_device_props2.pNext = &vulkan_1_2_props;

			for (uint32_t i = 0; i != physical_device_cnt; ++i)
			{
				vkGetPhysicalDeviceProperties2(physical_devices[i], &physical_device_props2);

				// Print out VkPhysicalDeviceProperties
				{
					och::print(output_file,
						"VkPhysicalDeviceProperties:"
						"     maxImageDimension1D:                             {}\n"
						"     maxImageDimension2D:                             {}\n"
						"     maxImageDimension3D:                             {}\n"
						"     maxImageDimensionCube:                           {}\n"
						"     maxImageArrayLayers:                             {}\n"
						"     maxTexelBufferElements:                          {}\n"
						"     maxUniformBufferRange:                           {}\n"
						"     maxStorageBufferRange:                           {}\n"
						"     maxPushConstantsSize:                            {}\n"
						"     maxMemoryAllocationCount:                        {}\n"
						"     maxSamplerAllocationCount:                       {}\n"
						"     bufferImageGranularity:                          {}\n"
						"     sparseAddressSpaceSize:                          {}\n"
						"     maxBoundDescriptorSets:                          {}\n"
						"     maxPerStageDescriptorSamplers:                   {}\n"
						"     maxPerStageDescriptorUniformBuffers:             {}\n"
						"     maxPerStageDescriptorStorageBuffers:             {}\n"
						"     maxPerStageDescriptorSampledImages:              {}\n"
						"     maxPerStageDescriptorStorageImages:              {}\n"
						"     maxPerStageDescriptorInputAttachments:           {}\n"
						"     maxPerStageResources:                            {}\n"
						"     maxDescriptorSetSamplers:                        {}\n"
						"     maxDescriptorSetUniformBuffers:                  {}\n"
						"     maxDescriptorSetUniformBuffersDynamic:           {}\n"
						"     maxDescriptorSetStorageBuffers:                  {}\n"
						"     maxDescriptorSetStorageBuffersDynamic:           {}\n"
						"     maxDescriptorSetSampledImages:                   {}\n"
						"     maxDescriptorSetStorageImages:                   {}\n"
						"     maxDescriptorSetInputAttachments:                {}\n"
						"     maxVertexInputAttributes:                        {}\n"
						"     maxVertexInputBindings:                          {}\n"
						"     maxVertexInputAttributeOffset:                   {}\n"
						"     maxVertexInputBindingStride:                     {}\n"
						"     maxVertexOutputComponents:                       {}\n"
						"     maxTessellationGenerationLevel:                  {}\n"
						"     maxTessellationPatchSize:                        {}\n"
						"     maxTessellationControlPerVertexInputComponents:  {}\n"
						"     maxTessellationControlPerVertexOutputComponents: {}\n"
						"     maxTessellationControlPerPatchOutputComponents:  {}\n"
						"     maxTessellationControlTotalOutputComponents:     {}\n"
						"     maxTessellationEvaluationInputComponents:        {}\n"
						"     maxTessellationEvaluationOutputComponents:       {}\n"
						"     maxGeometryShaderInvocations:                    {}\n"
						"     maxGeometryInputComponents:                      {}\n"
						"     maxGeometryOutputComponents:                     {}\n"
						"     maxGeometryOutputVertices:                       {}\n"
						"     maxGeometryTotalOutputComponents:                {}\n"
						"     maxFragmentInputComponents:                      {}\n"
						"     maxFragmentOutputAttachments:                    {}\n"
						"     maxFragmentDualSrcAttachments:                   {}\n"
						"     maxFragmentCombinedOutputResources:              {}\n"
						"     maxComputeSharedMemorySize:                      {}\n"
						"     maxComputeWorkGroupCount[0]:                     {}\n"
						"     maxComputeWorkGroupCount[1]:                     {}\n"
						"     maxComputeWorkGroupCount[2]:                     {}\n"
						"     maxComputeWorkGroupInvocations:                  {}\n"
						"     maxComputeWorkGroupSize[0]:                      {}\n"
						"     maxComputeWorkGroupSize[1]:                      {}\n"
						"     maxComputeWorkGroupSize[2]:                      {}\n"
						"     subPixelPrecisionBits:                           {}\n"
						"     subTexelPrecisionBits:                           {}\n"
						"     mipmapPrecisionBits:                             {}\n"
						"     maxDrawIndexedIndexValue:                        {}\n"
						"     maxDrawIndirectCount:                            {}\n"
						"     maxSamplerLodBias:                               {}\n"
						"     maxSamplerAnisotropy:                            {}\n"
						"     maxViewports:                                    {}\n"
						"     maxViewportDimensions[0]:                        {}\n"
						"     maxViewportDimensions[1]:                        {}\n"
						"     viewportBoundsRange[0]:                          {}\n"
						"     viewportBoundsRange[1]:                          {}\n"
						"     viewportSubPixelBits:                            {}\n"
						"     minMemoryMapAlignment:                           {}\n"
						"     minTexelBufferOffsetAlignment:                   {}\n"
						"     minUniformBufferOffsetAlignment:                 {}\n"
						"     minStorageBufferOffsetAlignment:                 {}\n"
						"     minTexelOffset:                                  {}\n"
						"     maxTexelOffset:                                  {}\n"
						"     minTexelGatherOffset:                            {}\n"
						"     maxTexelGatherOffset:                            {}\n"
						"     minInterpolationOffset:                          {}\n"
						"     maxInterpolationOffset:                          {}\n"
						"     subPixelInterpolationOffsetBits:                 {}\n"
						"     maxFramebufferWidth:                             {}\n"
						"     maxFramebufferHeight:                            {}\n"
						"     maxFramebufferLayers:                            {}\n"
						"     framebufferColorSampleCounts:                    {}\n"
						"     framebufferDepthSampleCounts:                    {}\n"
						"     framebufferStencilSampleCounts:                  {}\n"
						"     framebufferNoAttachmentsSampleCounts:            {}\n"
						"     maxColorAttachments:                             {}\n"
						"     sampledImageColorSampleCounts:                   {}\n"
						"     sampledImageIntegerSampleCounts:                 {}\n"
						"     sampledImageDepthSampleCounts:                   {}\n"
						"     sampledImageStencilSampleCounts:                 {}\n"
						"     storageImageSampleCounts:                        {}\n"
						"     maxSampleMaskWords:                              {}\n"
						"     timestampComputeAndGraphics:                     {}\n"
						"     timestampPeriod:                                 {}\n"
						"     maxClipDistances:                                {}\n"
						"     maxCullDistances:                                {}\n"
						"     maxCombinedClipAndCullDistances:                 {}\n"
						"     discreteQueuePriorities:                         {}\n"
						"     pointSizeRange[0]:                               {}\n"
						"     pointSizeRange[1]:                               {}\n"
						"     lineWidthRange[0]:                               {}\n"
						"     lineWidthRange[1]:                               {}\n"
						"     pointSizeGranularity:                            {}\n"
						"     lineWidthGranularity:                            {}\n"
						"     strictLines:                                     {}\n"
						"     standardSampleLocations:                         {}\n"
						"     optimalBufferCopyOffsetAlignment:                {}\n"
						"     optimalBufferCopyRowPitchAlignment:              {}\n"
						"     nonCoherentAtomSize:                             {}\n",
						physical_device_props2.properties.limits.maxImageDimension1D,
						physical_device_props2.properties.limits.maxImageDimension2D,
						physical_device_props2.properties.limits.maxImageDimension3D,
						physical_device_props2.properties.limits.maxImageDimensionCube,
						physical_device_props2.properties.limits.maxImageArrayLayers,
						physical_device_props2.properties.limits.maxTexelBufferElements,
						physical_device_props2.properties.limits.maxUniformBufferRange,
						physical_device_props2.properties.limits.maxStorageBufferRange,
						physical_device_props2.properties.limits.maxPushConstantsSize,
						physical_device_props2.properties.limits.maxMemoryAllocationCount,
						physical_device_props2.properties.limits.maxSamplerAllocationCount,
						physical_device_props2.properties.limits.bufferImageGranularity,
						physical_device_props2.properties.limits.sparseAddressSpaceSize,
						physical_device_props2.properties.limits.maxBoundDescriptorSets,
						physical_device_props2.properties.limits.maxPerStageDescriptorSamplers,
						physical_device_props2.properties.limits.maxPerStageDescriptorUniformBuffers,
						physical_device_props2.properties.limits.maxPerStageDescriptorStorageBuffers,
						physical_device_props2.properties.limits.maxPerStageDescriptorSampledImages,
						physical_device_props2.properties.limits.maxPerStageDescriptorStorageImages,
						physical_device_props2.properties.limits.maxPerStageDescriptorInputAttachments,
						physical_device_props2.properties.limits.maxPerStageResources,
						physical_device_props2.properties.limits.maxDescriptorSetSamplers,
						physical_device_props2.properties.limits.maxDescriptorSetUniformBuffers,
						physical_device_props2.properties.limits.maxDescriptorSetUniformBuffersDynamic,
						physical_device_props2.properties.limits.maxDescriptorSetStorageBuffers,
						physical_device_props2.properties.limits.maxDescriptorSetStorageBuffersDynamic,
						physical_device_props2.properties.limits.maxDescriptorSetSampledImages,
						physical_device_props2.properties.limits.maxDescriptorSetStorageImages,
						physical_device_props2.properties.limits.maxDescriptorSetInputAttachments,
						physical_device_props2.properties.limits.maxVertexInputAttributes,
						physical_device_props2.properties.limits.maxVertexInputBindings,
						physical_device_props2.properties.limits.maxVertexInputAttributeOffset,
						physical_device_props2.properties.limits.maxVertexInputBindingStride,
						physical_device_props2.properties.limits.maxVertexOutputComponents,
						physical_device_props2.properties.limits.maxTessellationGenerationLevel,
						physical_device_props2.properties.limits.maxTessellationPatchSize,
						physical_device_props2.properties.limits.maxTessellationControlPerVertexInputComponents,
						physical_device_props2.properties.limits.maxTessellationControlPerVertexOutputComponents,
						physical_device_props2.properties.limits.maxTessellationControlPerPatchOutputComponents,
						physical_device_props2.properties.limits.maxTessellationControlTotalOutputComponents,
						physical_device_props2.properties.limits.maxTessellationEvaluationInputComponents,
						physical_device_props2.properties.limits.maxTessellationEvaluationOutputComponents,
						physical_device_props2.properties.limits.maxGeometryShaderInvocations,
						physical_device_props2.properties.limits.maxGeometryInputComponents,
						physical_device_props2.properties.limits.maxGeometryOutputComponents,
						physical_device_props2.properties.limits.maxGeometryOutputVertices,
						physical_device_props2.properties.limits.maxGeometryTotalOutputComponents,
						physical_device_props2.properties.limits.maxFragmentInputComponents,
						physical_device_props2.properties.limits.maxFragmentOutputAttachments,
						physical_device_props2.properties.limits.maxFragmentDualSrcAttachments,
						physical_device_props2.properties.limits.maxFragmentCombinedOutputResources,
						physical_device_props2.properties.limits.maxComputeSharedMemorySize,
						physical_device_props2.properties.limits.maxComputeWorkGroupCount[0],
						physical_device_props2.properties.limits.maxComputeWorkGroupCount[1],
						physical_device_props2.properties.limits.maxComputeWorkGroupCount[2],
						physical_device_props2.properties.limits.maxComputeWorkGroupInvocations,
						physical_device_props2.properties.limits.maxComputeWorkGroupSize[0],
						physical_device_props2.properties.limits.maxComputeWorkGroupSize[1],
						physical_device_props2.properties.limits.maxComputeWorkGroupSize[2],
						physical_device_props2.properties.limits.subPixelPrecisionBits,
						physical_device_props2.properties.limits.subTexelPrecisionBits,
						physical_device_props2.properties.limits.mipmapPrecisionBits,
						physical_device_props2.properties.limits.maxDrawIndexedIndexValue,
						physical_device_props2.properties.limits.maxDrawIndirectCount,
						physical_device_props2.properties.limits.maxSamplerLodBias,
						physical_device_props2.properties.limits.maxSamplerAnisotropy,
						physical_device_props2.properties.limits.maxViewports,
						physical_device_props2.properties.limits.maxViewportDimensions[0],
						physical_device_props2.properties.limits.maxViewportDimensions[1],
						physical_device_props2.properties.limits.viewportBoundsRange[0],
						physical_device_props2.properties.limits.viewportBoundsRange[1],
						physical_device_props2.properties.limits.viewportSubPixelBits,
						physical_device_props2.properties.limits.minMemoryMapAlignment,
						physical_device_props2.properties.limits.minTexelBufferOffsetAlignment,
						physical_device_props2.properties.limits.minUniformBufferOffsetAlignment,
						physical_device_props2.properties.limits.minStorageBufferOffsetAlignment,
						physical_device_props2.properties.limits.minTexelOffset,
						physical_device_props2.properties.limits.maxTexelOffset,
						physical_device_props2.properties.limits.minTexelGatherOffset,
						physical_device_props2.properties.limits.maxTexelGatherOffset,
						physical_device_props2.properties.limits.minInterpolationOffset,
						physical_device_props2.properties.limits.maxInterpolationOffset,
						physical_device_props2.properties.limits.subPixelInterpolationOffsetBits,
						physical_device_props2.properties.limits.maxFramebufferWidth,
						physical_device_props2.properties.limits.maxFramebufferHeight,
						physical_device_props2.properties.limits.maxFramebufferLayers,
						physical_device_props2.properties.limits.framebufferColorSampleCounts,
						physical_device_props2.properties.limits.framebufferDepthSampleCounts,
						physical_device_props2.properties.limits.framebufferStencilSampleCounts,
						physical_device_props2.properties.limits.framebufferNoAttachmentsSampleCounts,
						physical_device_props2.properties.limits.maxColorAttachments,
						physical_device_props2.properties.limits.sampledImageColorSampleCounts,
						physical_device_props2.properties.limits.sampledImageIntegerSampleCounts,
						physical_device_props2.properties.limits.sampledImageDepthSampleCounts,
						physical_device_props2.properties.limits.sampledImageStencilSampleCounts,
						physical_device_props2.properties.limits.storageImageSampleCounts,
						physical_device_props2.properties.limits.maxSampleMaskWords,
						physical_device_props2.properties.limits.timestampComputeAndGraphics,
						physical_device_props2.properties.limits.timestampPeriod,
						physical_device_props2.properties.limits.maxClipDistances,
						physical_device_props2.properties.limits.maxCullDistances,
						physical_device_props2.properties.limits.maxCombinedClipAndCullDistances,
						physical_device_props2.properties.limits.discreteQueuePriorities,
						physical_device_props2.properties.limits.pointSizeRange[0],
						physical_device_props2.properties.limits.pointSizeRange[1],
						physical_device_props2.properties.limits.lineWidthRange[0],
						physical_device_props2.properties.limits.lineWidthRange[1],
						physical_device_props2.properties.limits.pointSizeGranularity,
						physical_device_props2.properties.limits.lineWidthGranularity,
						physical_device_props2.properties.limits.strictLines,
						physical_device_props2.properties.limits.standardSampleLocations,
						physical_device_props2.properties.limits.optimalBufferCopyOffsetAlignment,
						physical_device_props2.properties.limits.optimalBufferCopyRowPitchAlignment,
						physical_device_props2.properties.limits.nonCoherentAtomSize
					);
				}
			}
		}
		else
		{
			VkPhysicalDeviceProperties physical_device_props;

			for (uint32_t i = 0; i != physical_device_cnt; ++i)
			{
				vkGetPhysicalDeviceProperties(physical_devices[i], &physical_device_props);

				// Print out VkPhysicalDeviceProperties
				{
					och::print(output_file,
						"VkPhysicalDeviceProperties:"
						"     maxImageDimension1D:                             {}\n"
						"     maxImageDimension2D:                             {}\n"
						"     maxImageDimension3D:                             {}\n"
						"     maxImageDimensionCube:                           {}\n"
						"     maxImageArrayLayers:                             {}\n"
						"     maxTexelBufferElements:                          {}\n"
						"     maxUniformBufferRange:                           {}\n"
						"     maxStorageBufferRange:                           {}\n"
						"     maxPushConstantsSize:                            {}\n"
						"     maxMemoryAllocationCount:                        {}\n"
						"     maxSamplerAllocationCount:                       {}\n"
						"     bufferImageGranularity:                          {}\n"
						"     sparseAddressSpaceSize:                          {}\n"
						"     maxBoundDescriptorSets:                          {}\n"
						"     maxPerStageDescriptorSamplers:                   {}\n"
						"     maxPerStageDescriptorUniformBuffers:             {}\n"
						"     maxPerStageDescriptorStorageBuffers:             {}\n"
						"     maxPerStageDescriptorSampledImages:              {}\n"
						"     maxPerStageDescriptorStorageImages:              {}\n"
						"     maxPerStageDescriptorInputAttachments:           {}\n"
						"     maxPerStageResources:                            {}\n"
						"     maxDescriptorSetSamplers:                        {}\n"
						"     maxDescriptorSetUniformBuffers:                  {}\n"
						"     maxDescriptorSetUniformBuffersDynamic:           {}\n"
						"     maxDescriptorSetStorageBuffers:                  {}\n"
						"     maxDescriptorSetStorageBuffersDynamic:           {}\n"
						"     maxDescriptorSetSampledImages:                   {}\n"
						"     maxDescriptorSetStorageImages:                   {}\n"
						"     maxDescriptorSetInputAttachments:                {}\n"
						"     maxVertexInputAttributes:                        {}\n"
						"     maxVertexInputBindings:                          {}\n"
						"     maxVertexInputAttributeOffset:                   {}\n"
						"     maxVertexInputBindingStride:                     {}\n"
						"     maxVertexOutputComponents:                       {}\n"
						"     maxTessellationGenerationLevel:                  {}\n"
						"     maxTessellationPatchSize:                        {}\n"
						"     maxTessellationControlPerVertexInputComponents:  {}\n"
						"     maxTessellationControlPerVertexOutputComponents: {}\n"
						"     maxTessellationControlPerPatchOutputComponents:  {}\n"
						"     maxTessellationControlTotalOutputComponents:     {}\n"
						"     maxTessellationEvaluationInputComponents:        {}\n"
						"     maxTessellationEvaluationOutputComponents:       {}\n"
						"     maxGeometryShaderInvocations:                    {}\n"
						"     maxGeometryInputComponents:                      {}\n"
						"     maxGeometryOutputComponents:                     {}\n"
						"     maxGeometryOutputVertices:                       {}\n"
						"     maxGeometryTotalOutputComponents:                {}\n"
						"     maxFragmentInputComponents:                      {}\n"
						"     maxFragmentOutputAttachments:                    {}\n"
						"     maxFragmentDualSrcAttachments:                   {}\n"
						"     maxFragmentCombinedOutputResources:              {}\n"
						"     maxComputeSharedMemorySize:                      {}\n"
						"     maxComputeWorkGroupCount[0]:                     {}\n"
						"     maxComputeWorkGroupCount[1]:                     {}\n"
						"     maxComputeWorkGroupCount[2]:                     {}\n"
						"     maxComputeWorkGroupInvocations:                  {}\n"
						"     maxComputeWorkGroupSize[0]:                      {}\n"
						"     maxComputeWorkGroupSize[1]:                      {}\n"
						"     maxComputeWorkGroupSize[2]:                      {}\n"
						"     subPixelPrecisionBits:                           {}\n"
						"     subTexelPrecisionBits:                           {}\n"
						"     mipmapPrecisionBits:                             {}\n"
						"     maxDrawIndexedIndexValue:                        {}\n"
						"     maxDrawIndirectCount:                            {}\n"
						"     maxSamplerLodBias:                               {}\n"
						"     maxSamplerAnisotropy:                            {}\n"
						"     maxViewports:                                    {}\n"
						"     maxViewportDimensions[0]:                        {}\n"
						"     maxViewportDimensions[1]:                        {}\n"
						"     viewportBoundsRange[0]:                          {}\n"
						"     viewportBoundsRange[1]:                          {}\n"
						"     viewportSubPixelBits:                            {}\n"
						"     minMemoryMapAlignment:                           {}\n"
						"     minTexelBufferOffsetAlignment:                   {}\n"
						"     minUniformBufferOffsetAlignment:                 {}\n"
						"     minStorageBufferOffsetAlignment:                 {}\n"
						"     minTexelOffset:                                  {}\n"
						"     maxTexelOffset:                                  {}\n"
						"     minTexelGatherOffset:                            {}\n"
						"     maxTexelGatherOffset:                            {}\n"
						"     minInterpolationOffset:                          {}\n"
						"     maxInterpolationOffset:                          {}\n"
						"     subPixelInterpolationOffsetBits:                 {}\n"
						"     maxFramebufferWidth:                             {}\n"
						"     maxFramebufferHeight:                            {}\n"
						"     maxFramebufferLayers:                            {}\n"
						"     framebufferColorSampleCounts:                    {}\n"
						"     framebufferDepthSampleCounts:                    {}\n"
						"     framebufferStencilSampleCounts:                  {}\n"
						"     framebufferNoAttachmentsSampleCounts:            {}\n"
						"     maxColorAttachments:                             {}\n"
						"     sampledImageColorSampleCounts:                   {}\n"
						"     sampledImageIntegerSampleCounts:                 {}\n"
						"     sampledImageDepthSampleCounts:                   {}\n"
						"     sampledImageStencilSampleCounts:                 {}\n"
						"     storageImageSampleCounts:                        {}\n"
						"     maxSampleMaskWords:                              {}\n"
						"     timestampComputeAndGraphics:                     {}\n"
						"     timestampPeriod:                                 {}\n"
						"     maxClipDistances:                                {}\n"
						"     maxCullDistances:                                {}\n"
						"     maxCombinedClipAndCullDistances:                 {}\n"
						"     discreteQueuePriorities:                         {}\n"
						"     pointSizeRange[0]:                               {}\n"
						"     pointSizeRange[1]:                               {}\n"
						"     lineWidthRange[0]:                               {}\n"
						"     lineWidthRange[1]:                               {}\n"
						"     pointSizeGranularity:                            {}\n"
						"     lineWidthGranularity:                            {}\n"
						"     strictLines:                                     {}\n"
						"     standardSampleLocations:                         {}\n"
						"     optimalBufferCopyOffsetAlignment:                {}\n"
						"     optimalBufferCopyRowPitchAlignment:              {}\n"
						"     nonCoherentAtomSize:                             {}\n",
						physical_device_props.limits.maxImageDimension1D,
						physical_device_props.limits.maxImageDimension2D,
						physical_device_props.limits.maxImageDimension3D,
						physical_device_props.limits.maxImageDimensionCube,
						physical_device_props.limits.maxImageArrayLayers,
						physical_device_props.limits.maxTexelBufferElements,
						physical_device_props.limits.maxUniformBufferRange,
						physical_device_props.limits.maxStorageBufferRange,
						physical_device_props.limits.maxPushConstantsSize,
						physical_device_props.limits.maxMemoryAllocationCount,
						physical_device_props.limits.maxSamplerAllocationCount,
						physical_device_props.limits.bufferImageGranularity,
						physical_device_props.limits.sparseAddressSpaceSize,
						physical_device_props.limits.maxBoundDescriptorSets,
						physical_device_props.limits.maxPerStageDescriptorSamplers,
						physical_device_props.limits.maxPerStageDescriptorUniformBuffers,
						physical_device_props.limits.maxPerStageDescriptorStorageBuffers,
						physical_device_props.limits.maxPerStageDescriptorSampledImages,
						physical_device_props.limits.maxPerStageDescriptorStorageImages,
						physical_device_props.limits.maxPerStageDescriptorInputAttachments,
						physical_device_props.limits.maxPerStageResources,
						physical_device_props.limits.maxDescriptorSetSamplers,
						physical_device_props.limits.maxDescriptorSetUniformBuffers,
						physical_device_props.limits.maxDescriptorSetUniformBuffersDynamic,
						physical_device_props.limits.maxDescriptorSetStorageBuffers,
						physical_device_props.limits.maxDescriptorSetStorageBuffersDynamic,
						physical_device_props.limits.maxDescriptorSetSampledImages,
						physical_device_props.limits.maxDescriptorSetStorageImages,
						physical_device_props.limits.maxDescriptorSetInputAttachments,
						physical_device_props.limits.maxVertexInputAttributes,
						physical_device_props.limits.maxVertexInputBindings,
						physical_device_props.limits.maxVertexInputAttributeOffset,
						physical_device_props.limits.maxVertexInputBindingStride,
						physical_device_props.limits.maxVertexOutputComponents,
						physical_device_props.limits.maxTessellationGenerationLevel,
						physical_device_props.limits.maxTessellationPatchSize,
						physical_device_props.limits.maxTessellationControlPerVertexInputComponents,
						physical_device_props.limits.maxTessellationControlPerVertexOutputComponents,
						physical_device_props.limits.maxTessellationControlPerPatchOutputComponents,
						physical_device_props.limits.maxTessellationControlTotalOutputComponents,
						physical_device_props.limits.maxTessellationEvaluationInputComponents,
						physical_device_props.limits.maxTessellationEvaluationOutputComponents,
						physical_device_props.limits.maxGeometryShaderInvocations,
						physical_device_props.limits.maxGeometryInputComponents,
						physical_device_props.limits.maxGeometryOutputComponents,
						physical_device_props.limits.maxGeometryOutputVertices,
						physical_device_props.limits.maxGeometryTotalOutputComponents,
						physical_device_props.limits.maxFragmentInputComponents,
						physical_device_props.limits.maxFragmentOutputAttachments,
						physical_device_props.limits.maxFragmentDualSrcAttachments,
						physical_device_props.limits.maxFragmentCombinedOutputResources,
						physical_device_props.limits.maxComputeSharedMemorySize,
						physical_device_props.limits.maxComputeWorkGroupCount[0],
						physical_device_props.limits.maxComputeWorkGroupCount[1],
						physical_device_props.limits.maxComputeWorkGroupCount[2],
						physical_device_props.limits.maxComputeWorkGroupInvocations,
						physical_device_props.limits.maxComputeWorkGroupSize[0],
						physical_device_props.limits.maxComputeWorkGroupSize[1],
						physical_device_props.limits.maxComputeWorkGroupSize[2],
						physical_device_props.limits.subPixelPrecisionBits,
						physical_device_props.limits.subTexelPrecisionBits,
						physical_device_props.limits.mipmapPrecisionBits,
						physical_device_props.limits.maxDrawIndexedIndexValue,
						physical_device_props.limits.maxDrawIndirectCount,
						physical_device_props.limits.maxSamplerLodBias,
						physical_device_props.limits.maxSamplerAnisotropy,
						physical_device_props.limits.maxViewports,
						physical_device_props.limits.maxViewportDimensions[0],
						physical_device_props.limits.maxViewportDimensions[1],
						physical_device_props.limits.viewportBoundsRange[0],
						physical_device_props.limits.viewportBoundsRange[1],
						physical_device_props.limits.viewportSubPixelBits,
						physical_device_props.limits.minMemoryMapAlignment,
						physical_device_props.limits.minTexelBufferOffsetAlignment,
						physical_device_props.limits.minUniformBufferOffsetAlignment,
						physical_device_props.limits.minStorageBufferOffsetAlignment,
						physical_device_props.limits.minTexelOffset,
						physical_device_props.limits.maxTexelOffset,
						physical_device_props.limits.minTexelGatherOffset,
						physical_device_props.limits.maxTexelGatherOffset,
						physical_device_props.limits.minInterpolationOffset,
						physical_device_props.limits.maxInterpolationOffset,
						physical_device_props.limits.subPixelInterpolationOffsetBits,
						physical_device_props.limits.maxFramebufferWidth,
						physical_device_props.limits.maxFramebufferHeight,
						physical_device_props.limits.maxFramebufferLayers,
						physical_device_props.limits.framebufferColorSampleCounts,
						physical_device_props.limits.framebufferDepthSampleCounts,
						physical_device_props.limits.framebufferStencilSampleCounts,
						physical_device_props.limits.framebufferNoAttachmentsSampleCounts,
						physical_device_props.limits.maxColorAttachments,
						physical_device_props.limits.sampledImageColorSampleCounts,
						physical_device_props.limits.sampledImageIntegerSampleCounts,
						physical_device_props.limits.sampledImageDepthSampleCounts,
						physical_device_props.limits.sampledImageStencilSampleCounts,
						physical_device_props.limits.storageImageSampleCounts,
						physical_device_props.limits.maxSampleMaskWords,
						physical_device_props.limits.timestampComputeAndGraphics,
						physical_device_props.limits.timestampPeriod,
						physical_device_props.limits.maxClipDistances,
						physical_device_props.limits.maxCullDistances,
						physical_device_props.limits.maxCombinedClipAndCullDistances,
						physical_device_props.limits.discreteQueuePriorities,
						physical_device_props.limits.pointSizeRange[0],
						physical_device_props.limits.pointSizeRange[1],
						physical_device_props.limits.lineWidthRange[0],
						physical_device_props.limits.lineWidthRange[1],
						physical_device_props.limits.pointSizeGranularity,
						physical_device_props.limits.lineWidthGranularity,
						physical_device_props.limits.strictLines,
						physical_device_props.limits.standardSampleLocations,
						physical_device_props.limits.optimalBufferCopyOffsetAlignment,
						physical_device_props.limits.optimalBufferCopyRowPitchAlignment,
						physical_device_props.limits.nonCoherentAtomSize
					);
				}
			}
		}

		if (argc > 2)
			check(och::close_file(output_file));

		vkDestroyInstance(instance, nullptr);

		return {};
	}
};

och::status run_gpu_info(int argc, const char** argv)
{
	gpu_info program{};

	och::status err = program.run(argc, argv);

	return err;
}
