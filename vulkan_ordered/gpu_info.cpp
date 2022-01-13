#include "gpu_info.h"

#include "vulkan_base.h"

#include "och_fmt.h"

struct gpu_info
{
	vulkan_context context;

	och::status create(int argc, const char** argv)
	{
		och::iohandle output_file = och::get_stdout();

		if (argc > 2)
			check(och::open_file(output_file, argv[2], och::fio::access::write, och::fio::open::truncate, och::fio::open::normal));


		check(context.create("gpu info", 50, 50, 1));

		VkPhysicalDeviceProperties dev_props;
		vkGetPhysicalDeviceProperties(context.m_physical_device, &dev_props);

		och::print(output_file,
			"maxImageDimension1D:                             {}\n"
			"maxImageDimension2D:                             {}\n"
			"maxImageDimension3D:                             {}\n"
			"maxImageDimensionCube:                           {}\n"
			"maxImageArrayLayers:                             {}\n"
			"maxTexelBufferElements:                          {}\n"
			"maxUniformBufferRange:                           {}\n"
			"maxStorageBufferRange:                           {}\n"
			"maxPushConstantsSize:                            {}\n"
			"maxMemoryAllocationCount:                        {}\n"
			"maxSamplerAllocationCount:                       {}\n"
			"bufferImageGranularity:                          {}\n"
			"sparseAddressSpaceSize:                          {}\n"
			"maxBoundDescriptorSets:                          {}\n"
			"maxPerStageDescriptorSamplers:                   {}\n"
			"maxPerStageDescriptorUniformBuffers:             {}\n"
			"maxPerStageDescriptorStorageBuffers:             {}\n"
			"maxPerStageDescriptorSampledImages:              {}\n"
			"maxPerStageDescriptorStorageImages:              {}\n"
			"maxPerStageDescriptorInputAttachments:           {}\n"
			"maxPerStageResources:                            {}\n"
			"maxDescriptorSetSamplers:                        {}\n"
			"maxDescriptorSetUniformBuffers:                  {}\n"
			"maxDescriptorSetUniformBuffersDynamic:           {}\n"
			"maxDescriptorSetStorageBuffers:                  {}\n"
			"maxDescriptorSetStorageBuffersDynamic:           {}\n"
			"maxDescriptorSetSampledImages:                   {}\n"
			"maxDescriptorSetStorageImages:                   {}\n"
			"maxDescriptorSetInputAttachments:                {}\n"
			"maxVertexInputAttributes:                        {}\n"
			"maxVertexInputBindings:                          {}\n"
			"maxVertexInputAttributeOffset:                   {}\n"
			"maxVertexInputBindingStride:                     {}\n"
			"maxVertexOutputComponents:                       {}\n"
			"maxTessellationGenerationLevel:                  {}\n"
			"maxTessellationPatchSize:                        {}\n"
			"maxTessellationControlPerVertexInputComponents:  {}\n"
			"maxTessellationControlPerVertexOutputComponents: {}\n"
			"maxTessellationControlPerPatchOutputComponents:  {}\n"
			"maxTessellationControlTotalOutputComponents:     {}\n"
			"maxTessellationEvaluationInputComponents:        {}\n"
			"maxTessellationEvaluationOutputComponents:       {}\n"
			"maxGeometryShaderInvocations:                    {}\n"
			"maxGeometryInputComponents:                      {}\n"
			"maxGeometryOutputComponents:                     {}\n"
			"maxGeometryOutputVertices:                       {}\n"
			"maxGeometryTotalOutputComponents:                {}\n"
			"maxFragmentInputComponents:                      {}\n"
			"maxFragmentOutputAttachments:                    {}\n"
			"maxFragmentDualSrcAttachments:                   {}\n"
			"maxFragmentCombinedOutputResources:              {}\n"
			"maxComputeSharedMemorySize:                      {}\n"
			"maxComputeWorkGroupCount[0]:                     {}\n"
			"maxComputeWorkGroupCount[1]:                     {}\n"
			"maxComputeWorkGroupCount[2]:                     {}\n"
			"maxComputeWorkGroupInvocations:                  {}\n"
			"maxComputeWorkGroupSize[0]:                      {}\n"
			"maxComputeWorkGroupSize[1]:                      {}\n"
			"maxComputeWorkGroupSize[2]:                      {}\n"
			"subPixelPrecisionBits:                           {}\n"
			"subTexelPrecisionBits:                           {}\n"
			"mipmapPrecisionBits:                             {}\n"
			"maxDrawIndexedIndexValue:                        {}\n"
			"maxDrawIndirectCount:                            {}\n"
			"maxSamplerLodBias:                               {}\n"
			"maxSamplerAnisotropy:                            {}\n"
			"maxViewports:                                    {}\n"
			"maxViewportDimensions[0]:                        {}\n"
			"maxViewportDimensions[1]:                        {}\n"
			"viewportBoundsRange[0]:                          {}\n"
			"viewportBoundsRange[1]:                          {}\n"
			"viewportSubPixelBits:                            {}\n"
			"minMemoryMapAlignment:                           {}\n"
			"minTexelBufferOffsetAlignment:                   {}\n"
			"minUniformBufferOffsetAlignment:                 {}\n"
			"minStorageBufferOffsetAlignment:                 {}\n"
			"minTexelOffset:                                  {}\n"
			"maxTexelOffset:                                  {}\n"
			"minTexelGatherOffset:                            {}\n"
			"maxTexelGatherOffset:                            {}\n"
			"minInterpolationOffset:                          {}\n"
			"maxInterpolationOffset:                          {}\n"
			"subPixelInterpolationOffsetBits:                 {}\n"
			"maxFramebufferWidth:                             {}\n"
			"maxFramebufferHeight:                            {}\n"
			"maxFramebufferLayers:                            {}\n"
			"framebufferColorSampleCounts:                    {}\n"
			"framebufferDepthSampleCounts:                    {}\n"
			"framebufferStencilSampleCounts:                  {}\n"
			"framebufferNoAttachmentsSampleCounts:            {}\n"
			"maxColorAttachments:                             {}\n"
			"sampledImageColorSampleCounts:                   {}\n"
			"sampledImageIntegerSampleCounts:                 {}\n"
			"sampledImageDepthSampleCounts:                   {}\n"
			"sampledImageStencilSampleCounts:                 {}\n"
			"storageImageSampleCounts:                        {}\n"
			"maxSampleMaskWords:                              {}\n"
			"timestampComputeAndGraphics:                     {}\n"
			"timestampPeriod:                                 {}\n"
			"maxClipDistances:                                {}\n"
			"maxCullDistances:                                {}\n"
			"maxCombinedClipAndCullDistances:                 {}\n"
			"discreteQueuePriorities:                         {}\n"
			"pointSizeRange[0]:                               {}\n"
			"pointSizeRange[1]:                               {}\n"
			"lineWidthRange[0]:                               {}\n"
			"lineWidthRange[1]:                               {}\n"
			"pointSizeGranularity:                            {}\n"
			"lineWidthGranularity:                            {}\n"
			"strictLines:                                     {}\n"
			"standardSampleLocations:                         {}\n"
			"optimalBufferCopyOffsetAlignment:                {}\n"
			"optimalBufferCopyRowPitchAlignment:              {}\n"
			"nonCoherentAtomSize:                             {}\n",
			dev_props.limits.maxImageDimension1D,
			dev_props.limits.maxImageDimension2D,
			dev_props.limits.maxImageDimension3D,
			dev_props.limits.maxImageDimensionCube,
			dev_props.limits.maxImageArrayLayers,
			dev_props.limits.maxTexelBufferElements,
			dev_props.limits.maxUniformBufferRange,
			dev_props.limits.maxStorageBufferRange,
			dev_props.limits.maxPushConstantsSize,
			dev_props.limits.maxMemoryAllocationCount,
			dev_props.limits.maxSamplerAllocationCount,
			dev_props.limits.bufferImageGranularity,
			dev_props.limits.sparseAddressSpaceSize,
			dev_props.limits.maxBoundDescriptorSets,
			dev_props.limits.maxPerStageDescriptorSamplers,
			dev_props.limits.maxPerStageDescriptorUniformBuffers,
			dev_props.limits.maxPerStageDescriptorStorageBuffers,
			dev_props.limits.maxPerStageDescriptorSampledImages,
			dev_props.limits.maxPerStageDescriptorStorageImages,
			dev_props.limits.maxPerStageDescriptorInputAttachments,
			dev_props.limits.maxPerStageResources,
			dev_props.limits.maxDescriptorSetSamplers,
			dev_props.limits.maxDescriptorSetUniformBuffers,
			dev_props.limits.maxDescriptorSetUniformBuffersDynamic,
			dev_props.limits.maxDescriptorSetStorageBuffers,
			dev_props.limits.maxDescriptorSetStorageBuffersDynamic,
			dev_props.limits.maxDescriptorSetSampledImages,
			dev_props.limits.maxDescriptorSetStorageImages,
			dev_props.limits.maxDescriptorSetInputAttachments,
			dev_props.limits.maxVertexInputAttributes,
			dev_props.limits.maxVertexInputBindings,
			dev_props.limits.maxVertexInputAttributeOffset,
			dev_props.limits.maxVertexInputBindingStride,
			dev_props.limits.maxVertexOutputComponents,
			dev_props.limits.maxTessellationGenerationLevel,
			dev_props.limits.maxTessellationPatchSize,
			dev_props.limits.maxTessellationControlPerVertexInputComponents,
			dev_props.limits.maxTessellationControlPerVertexOutputComponents,
			dev_props.limits.maxTessellationControlPerPatchOutputComponents,
			dev_props.limits.maxTessellationControlTotalOutputComponents,
			dev_props.limits.maxTessellationEvaluationInputComponents,
			dev_props.limits.maxTessellationEvaluationOutputComponents,
			dev_props.limits.maxGeometryShaderInvocations,
			dev_props.limits.maxGeometryInputComponents,
			dev_props.limits.maxGeometryOutputComponents,
			dev_props.limits.maxGeometryOutputVertices,
			dev_props.limits.maxGeometryTotalOutputComponents,
			dev_props.limits.maxFragmentInputComponents,
			dev_props.limits.maxFragmentOutputAttachments,
			dev_props.limits.maxFragmentDualSrcAttachments,
			dev_props.limits.maxFragmentCombinedOutputResources,
			dev_props.limits.maxComputeSharedMemorySize,
			dev_props.limits.maxComputeWorkGroupCount[0],
			dev_props.limits.maxComputeWorkGroupCount[1],
			dev_props.limits.maxComputeWorkGroupCount[2],
			dev_props.limits.maxComputeWorkGroupInvocations,
			dev_props.limits.maxComputeWorkGroupSize[0],
			dev_props.limits.maxComputeWorkGroupSize[1],
			dev_props.limits.maxComputeWorkGroupSize[2],
			dev_props.limits.subPixelPrecisionBits,
			dev_props.limits.subTexelPrecisionBits,
			dev_props.limits.mipmapPrecisionBits,
			dev_props.limits.maxDrawIndexedIndexValue,
			dev_props.limits.maxDrawIndirectCount,
			dev_props.limits.maxSamplerLodBias,
			dev_props.limits.maxSamplerAnisotropy,
			dev_props.limits.maxViewports,
			dev_props.limits.maxViewportDimensions[0],
			dev_props.limits.maxViewportDimensions[1],
			dev_props.limits.viewportBoundsRange[0],
			dev_props.limits.viewportBoundsRange[1],
			dev_props.limits.viewportSubPixelBits,
			dev_props.limits.minMemoryMapAlignment,
			dev_props.limits.minTexelBufferOffsetAlignment,
			dev_props.limits.minUniformBufferOffsetAlignment,
			dev_props.limits.minStorageBufferOffsetAlignment,
			dev_props.limits.minTexelOffset,
			dev_props.limits.maxTexelOffset,
			dev_props.limits.minTexelGatherOffset,
			dev_props.limits.maxTexelGatherOffset,
			dev_props.limits.minInterpolationOffset,
			dev_props.limits.maxInterpolationOffset,
			dev_props.limits.subPixelInterpolationOffsetBits,
			dev_props.limits.maxFramebufferWidth,
			dev_props.limits.maxFramebufferHeight,
			dev_props.limits.maxFramebufferLayers,
			dev_props.limits.framebufferColorSampleCounts,
			dev_props.limits.framebufferDepthSampleCounts,
			dev_props.limits.framebufferStencilSampleCounts,
			dev_props.limits.framebufferNoAttachmentsSampleCounts,
			dev_props.limits.maxColorAttachments,
			dev_props.limits.sampledImageColorSampleCounts,
			dev_props.limits.sampledImageIntegerSampleCounts,
			dev_props.limits.sampledImageDepthSampleCounts,
			dev_props.limits.sampledImageStencilSampleCounts,
			dev_props.limits.storageImageSampleCounts,
			dev_props.limits.maxSampleMaskWords,
			dev_props.limits.timestampComputeAndGraphics,
			dev_props.limits.timestampPeriod,
			dev_props.limits.maxClipDistances,
			dev_props.limits.maxCullDistances,
			dev_props.limits.maxCombinedClipAndCullDistances,
			dev_props.limits.discreteQueuePriorities,
			dev_props.limits.pointSizeRange[0],
			dev_props.limits.pointSizeRange[1],
			dev_props.limits.lineWidthRange[0],
			dev_props.limits.lineWidthRange[1],
			dev_props.limits.pointSizeGranularity,
			dev_props.limits.lineWidthGranularity,
			dev_props.limits.strictLines,
			dev_props.limits.standardSampleLocations,
			dev_props.limits.optimalBufferCopyOffsetAlignment,
			dev_props.limits.optimalBufferCopyRowPitchAlignment,
			dev_props.limits.nonCoherentAtomSize
		);

		if (argc > 2)
			check(och::close_file(output_file));

		return {};
	}

	void destroy()
	{
		context.destroy();
	}
};

och::status run_gpu_info(int argc, const char** argv)
{
	gpu_info program{};

	och::status err = program.create(argc, argv);

	program.destroy();

	return err;
}
