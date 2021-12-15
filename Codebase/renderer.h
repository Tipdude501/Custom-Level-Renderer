#include "shaderc/shaderc.h" // needed for compiling shaders at runtime
#ifdef _WIN32 // must use MT platform DLL libraries on windows
	#pragma comment(lib, "shaderc_combined.lib") 
#endif
#include <chrono>
#include <iostream>
#include <string>
#include <fstream>
#include "build/LevelData.h"
#include "build/h2bParser.h"

#define PI 3.14159265359f
#define TO_RADIANS PI / 180.0f

//Forward declarations
std::string ShaderAsString(const char* shaderFilePath);

// Shaders
std::string vertexShaderPath = ShaderAsString("../VertexShader.hlsl");
const char* vertexShaderSource = vertexShaderPath.c_str();
std::string pixelShaderPath = ShaderAsString("../PixelShader.hlsl");
const char* pixelShaderSource = pixelShaderPath.c_str();


// Creation, Rendering & Cleanup
class Renderer
{
	// Proxy handles
	GW::SYSTEM::GWindow win;
	GW::GRAPHICS::GVulkanSurface vlk;
	GW::CORE::GEventReceiver shutdown;

	// Level data
	LevelData lvlData;
	std::string levelFilePath = "../../Assets/Levels/TestLevel2.txt";
	
	// Model data
	H2B::Parser parser;

	// User Input
	GW::INPUT::GInput inputProxy;
	GW::INPUT::GController controllerProxy;
	float updatesPerSecond = 60;
	float cameraMoveSpeed = 0.18f;
	float lookSensitivity = 4.0f;

	// Matrices
	GW::MATH::GMatrix matrixProxy;
	GW::MATH::GVector vectorProxy;
	GW::MATH::GMATRIXF camera;
	GW::MATH::GMATRIXF view;
	GW::MATH::GMATRIXF projection;

	// Shader uniforms
	std::vector<VkBuffer> svHandle;
	std::vector<VkDeviceMemory> svData;
	VkDescriptorSetLayout svDescriptorLayout = nullptr;
	std::vector<VkDescriptorSet> svDescriptorSet;
	VkDescriptorPool descriptorPool = nullptr;
	unsigned int max_frames = 0;
	#define MAX_INSTANCE_PER_DRAW 1024
	struct ShaderVariables {
		int materialIndex;
		int textureIndex;
		int padding[14];
		GW::MATH::GMATRIXF viewProjection;
		GW::MATH::GMATRIXF matrices[MAX_INSTANCE_PER_DRAW];
	};
	ShaderVariables shaderVars;

	// Vulkan objects
	VkDevice device = nullptr;
	VkBuffer vertexHandle = nullptr;
	VkBuffer indexHandle = nullptr;
	VkDeviceMemory vertexData = nullptr;
	VkDeviceMemory indexData = nullptr;
	VkShaderModule vertexShader = nullptr;
	VkShaderModule pixelShader = nullptr;
	VkPipeline pipeline = nullptr;
	VkPipelineLayout pipelineLayout = nullptr;

public:
	//struct ShaderVars {
	//	GW::MATH::GMATRIXF world;
	//	GW::MATH::GMATRIXF viewProjection;
	//};

	Renderer(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GVulkanSurface _vlk)
	{
		win = _win;
		vlk = _vlk;
		unsigned int width, height;
		win.GetClientWidth(width);
		win.GetClientHeight(height);

		// Create proxy's
		inputProxy.Create(win);
		controllerProxy.Create();
		matrixProxy.Create();
		vectorProxy.Create();

		// Init camera and view
		GW::MATH::GVECTORF eye = { 0.5f, 0.5f, -0.5f };
		GW::MATH::GVECTORF at = { 0.0f, 0.0f, 0.0f };
		GW::MATH::GVECTORF up = { 0.0f, 1.0f, 0.0f };
		matrixProxy.LookAtLHF(eye, at, up, view);

		/***************** LOAD LEVEL AND MODEL DATA ******************/
		// Load level data
		std::vector <std::string> filenames;
		std::vector<GW::MATH::GMATRIXF> matrices;
		if (!GetGameLevelData(filenames, matrices))
			std::cout << "Level Loading Error: \"" << levelFilePath << "\" did not open properly.\n";
		for (size_t i = 0; i < filenames.size(); i++)
		{
			lvlData.AddInstance(filenames[i], matrices[i]);
		}

		// Load model data
		size_t uniqueMeshCount = lvlData.uniqueMeshes.size();
		for (size_t uniqueMeshIndex = 0; uniqueMeshIndex < uniqueMeshCount; uniqueMeshIndex++)
		{
			// Parse h2b file
			std::string modelFilePath = "../../Assets/Models/" + lvlData.uniqueMeshes[uniqueMeshIndex].name + ".h2b";
			if (!parser.Parse(modelFilePath.c_str())) {
				std::cout << "Model Loading Error: \"" << modelFilePath << "\" did not open properly.\n";
				continue;
			}

			// Copy data over
			if (parser.meshCount > 1) //group by material if submeshes exist
			{
				//push back submeshes, starting at submesh 2
				for (size_t submeshIndex = 1; submeshIndex < parser.meshCount; submeshIndex++)
				{
					LevelData::UniqueMesh submesh = lvlData.uniqueMeshes[uniqueMeshIndex];
					submesh.name += "_submesh" + std::to_string(submeshIndex + 1);
					submesh.indexCount = parser.meshes[submeshIndex].drawInfo.indexCount;
					submesh.firstIndex = lvlData.indices.size();
					submesh.vertexOffset = lvlData.vertices.size();
					submesh.materialIndex = lvlData.materials.size();
					lvlData.uniqueMeshes.push_back(submesh);

					//push back indices per submesh
					int start = parser.meshes[submeshIndex].drawInfo.indexOffset;
					int end = parser.meshes[submeshIndex].drawInfo.indexOffset + parser.meshes[submeshIndex].drawInfo.indexCount;
					for (size_t i = start; i < end; i++)
						lvlData.indices.push_back(parser.indices[i]);
					
					//push back material per submesh
					int matIndex = parser.meshes[submeshIndex].materialIndex;
					lvlData.materials.push_back(parser.materials[matIndex]);
				}

				//then write the first submesh data to the original unique mesh spot
				lvlData.uniqueMeshes[uniqueMeshIndex].name += "_submesh1";
				lvlData.uniqueMeshes[uniqueMeshIndex].indexCount = parser.meshes[0].drawInfo.indexCount;
				lvlData.uniqueMeshes[uniqueMeshIndex].firstIndex = lvlData.indices.size();
				lvlData.uniqueMeshes[uniqueMeshIndex].vertexOffset = lvlData.vertices.size();
				lvlData.uniqueMeshes[uniqueMeshIndex].materialIndex = lvlData.materials.size();
				
				//push back indices for first submest
				int start = parser.meshes[0].drawInfo.indexOffset;
				int end = parser.meshes[0].drawInfo.indexOffset + parser.meshes[0].drawInfo.indexCount;
				for (size_t i = start; i < end; i++)
					lvlData.indices.push_back(parser.indices[i]);
				
				//push back material per submesh
				int matIndex = parser.meshes[0].materialIndex;
				lvlData.materials.push_back(parser.materials[matIndex]);

				// Push back all vertices
				for (size_t j = 0; j < parser.vertexCount; j++)
					lvlData.vertices.push_back(parser.vertices[j]);
			}
			else //otherwise load data into existing unique mesh
			{
				lvlData.uniqueMeshes[uniqueMeshIndex].indexCount = parser.indexCount;
				lvlData.uniqueMeshes[uniqueMeshIndex].firstIndex = lvlData.indices.size();
				lvlData.uniqueMeshes[uniqueMeshIndex].vertexOffset = lvlData.vertices.size();
				lvlData.uniqueMeshes[uniqueMeshIndex].materialIndex = lvlData.materials.size();
				for (size_t i = 0; i < parser.vertexCount; i++)
					lvlData.vertices.push_back(parser.vertices[i]);
				for (size_t i = 0; i < parser.indexCount; i++)
					lvlData.indices.push_back(parser.indices[i]);
				lvlData.materials.push_back(parser.materials[0]);
			}
		}

		/***************** BUFFER INITIALIZATION ******************/
		// Grab the device & physical device
		VkPhysicalDevice physicalDevice = nullptr;
		vlk.GetDevice((void**)&device);
		vlk.GetPhysicalDevice((void**)&physicalDevice);
		
		// Transfer vertices to vertex buffer
		GvkHelper::create_buffer(physicalDevice, device, lvlData.vertices.size() * sizeof(lvlData.vertices[0]),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertexHandle, &vertexData);
		GvkHelper::write_to_buffer(device, vertexData, lvlData.vertices.data(), lvlData.vertices.size() * sizeof(lvlData.vertices[0]));

		// Transfer indices to index buffer
		GvkHelper::create_buffer(physicalDevice, device, lvlData.indices.size() * sizeof(lvlData.indices[0]),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &indexHandle, &indexData);
		GvkHelper::write_to_buffer(device, indexData, lvlData.indices.data(), lvlData.indices.size() * sizeof(lvlData.indices[0]));


		// Create a storage buffer for per-draw call shader variables
		vlk.GetSwapchainImageCount(max_frames);
		svHandle.resize(max_frames);
		svData.resize(max_frames);
		for (size_t i = 0; i < max_frames; i++)
		{
			GvkHelper::create_buffer(physicalDevice, device, sizeof(ShaderVariables),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &svHandle[i], &svData[i]);
			GvkHelper::write_to_buffer(device, svData[i], &shaderVars, sizeof(ShaderVariables));
		}

		/***************** SHADER INTIALIZATION ******************/
		// Intialize runtime shader compiler HLSL -> SPIRV
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		shaderc_compile_options_t options = shaderc_compile_options_initialize();
		shaderc_compile_options_set_source_language(options, shaderc_source_language_hlsl);
		shaderc_compile_options_set_invert_y(options, false);
#ifndef NDEBUG
		shaderc_compile_options_set_generate_debug_info(options);
#endif
		
		// Create Vertex Shader
		shaderc_compilation_result_t result = shaderc_compile_into_spv( // compile
			compiler, vertexShaderSource, strlen(vertexShaderSource),
			shaderc_vertex_shader, "main.vert", "main", options);
		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
			std::cout << "Vertex Shader Errors: " << shaderc_result_get_error_message(result) << std::endl;
		GvkHelper::create_shader_module(device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &vertexShader);
		shaderc_result_release(result);
		
		// Create Pixel Shader
		result = shaderc_compile_into_spv( // compile
			compiler, pixelShaderSource, strlen(pixelShaderSource),
			shaderc_fragment_shader, "main.frag", "main", options);
		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
			std::cout << "Pixel Shader Errors: " << shaderc_result_get_error_message(result) << std::endl;
		GvkHelper::create_shader_module(device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &pixelShader);
		shaderc_result_release(result);
		
		// Free runtime shader compiler resources
		shaderc_compile_options_release(options);
		shaderc_compiler_release(compiler);

		/***************** PIPELINE INTIALIZATION ******************/
		// Create Pipeline & Layout (Thanks Tiny!)
		VkRenderPass renderPass;
		vlk.GetRenderPass((void**)&renderPass);
		VkPipelineShaderStageCreateInfo stage_create_info[2] = {};
		
		// Create Stage Info for Vertex Shader
		stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stage_create_info[0].module = vertexShader;
		stage_create_info[0].pName = "main";
		
		// Create Stage Info for Fragment Shader
		stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stage_create_info[1].module = pixelShader;
		stage_create_info[1].pName = "main";
		
		// Assembly State
		VkPipelineInputAssemblyStateCreateInfo assembly_create_info = {};
		assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		assembly_create_info.primitiveRestartEnable = false;
		
		// Vertex Input State
		VkVertexInputBindingDescription vertex_binding_description = {};
		vertex_binding_description.binding = 0;
		vertex_binding_description.stride = sizeof(H2B::VERTEX);
		vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		VkVertexInputAttributeDescription vertex_attribute_description[3] = {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(H2B::VERTEX, pos) },
			{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(H2B::VERTEX, uvw) },
			{ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(H2B::VERTEX, nrm) }
		};
		VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
		input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		input_vertex_info.vertexBindingDescriptionCount = 1;
		input_vertex_info.pVertexBindingDescriptions = &vertex_binding_description;
		input_vertex_info.vertexAttributeDescriptionCount = 3;
		input_vertex_info.pVertexAttributeDescriptions = vertex_attribute_description;
		
		// Viewport State 
		VkViewport viewport = {
			0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1
		};
		VkRect2D scissor = { {0, 0}, {width, height} };
		VkPipelineViewportStateCreateInfo viewport_create_info = {};
		viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_create_info.viewportCount = 1;
		viewport_create_info.pViewports = &viewport;
		viewport_create_info.scissorCount = 1;
		viewport_create_info.pScissors = &scissor;
		
		// Rasterizer State
		VkPipelineRasterizationStateCreateInfo rasterization_create_info = {};
		rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_create_info.rasterizerDiscardEnable = VK_FALSE;
		rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization_create_info.lineWidth = 1.0f;
		rasterization_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterization_create_info.depthClampEnable = VK_FALSE;
		rasterization_create_info.depthBiasEnable = VK_FALSE;
		rasterization_create_info.depthBiasClamp = 0.0f;
		rasterization_create_info.depthBiasConstantFactor = 0.0f;
		rasterization_create_info.depthBiasSlopeFactor = 0.0f;
		
		// Multisampling State
		VkPipelineMultisampleStateCreateInfo multisample_create_info = {};
		multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_create_info.sampleShadingEnable = VK_FALSE;
		multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisample_create_info.minSampleShading = 1.0f;
		multisample_create_info.pSampleMask = VK_NULL_HANDLE;
		multisample_create_info.alphaToCoverageEnable = VK_FALSE;
		multisample_create_info.alphaToOneEnable = VK_FALSE;
		
		// Depth-Stencil State
		VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = {};
		depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_create_info.depthTestEnable = VK_TRUE;
		depth_stencil_create_info.depthWriteEnable = VK_TRUE;
		depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
		depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_create_info.minDepthBounds = 0.0f;
		depth_stencil_create_info.maxDepthBounds = 1.0f;
		depth_stencil_create_info.stencilTestEnable = VK_FALSE;
		
		// Color Blending Attachment & State
		VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
		color_blend_attachment_state.colorWriteMask = 0xF;
		color_blend_attachment_state.blendEnable = VK_FALSE;
		color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
		color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
		color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
		color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
		VkPipelineColorBlendStateCreateInfo color_blend_create_info = {};
		color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blend_create_info.logicOpEnable = VK_FALSE;
		color_blend_create_info.logicOp = VK_LOGIC_OP_COPY;
		color_blend_create_info.attachmentCount = 1;
		color_blend_create_info.pAttachments = &color_blend_attachment_state;
		color_blend_create_info.blendConstants[0] = 0.0f;
		color_blend_create_info.blendConstants[1] = 0.0f;
		color_blend_create_info.blendConstants[2] = 0.0f;
		color_blend_create_info.blendConstants[3] = 0.0f;
		
		// Dynamic State 
		VkDynamicState dynamic_state[2] = {
			// By setting these we do not need to re-create the pipeline on Resize
			VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
		dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_create_info.dynamicStateCount = 2;
		dynamic_create_info.pDynamicStates = dynamic_state;

		// Instance Uniform Buffer descriptor layout
		VkDescriptorSetLayoutBinding svDescriptorLayoutBinding = {};
		svDescriptorLayoutBinding.binding = 0;
		svDescriptorLayoutBinding.descriptorCount = 1;
		svDescriptorLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		svDescriptorLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		svDescriptorLayoutBinding.pImmutableSamplers = nullptr;
		VkDescriptorSetLayoutCreateInfo svDescriptorCreateInfo = {};
		svDescriptorCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		svDescriptorCreateInfo.flags = 0;
		svDescriptorCreateInfo.bindingCount = 1;
		svDescriptorCreateInfo.pBindings = &svDescriptorLayoutBinding;
		svDescriptorCreateInfo.pNext = nullptr;
		VkResult r = vkCreateDescriptorSetLayout(device, &svDescriptorCreateInfo,
			nullptr, &svDescriptorLayout);

		// Descriptor Pool
		VkDescriptorPoolCreateInfo svDescriptorpool_create_info = {};
		svDescriptorpool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		VkDescriptorPoolSize descriptorpool_size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, max_frames };
		/*VkDescriptorPoolSize descriptorpool_size[2] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, matrixBuffer.size() }, //instance uniform
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }			//material storage
																		//texture storage
		};*/
		svDescriptorpool_create_info.poolSizeCount = 1;
		svDescriptorpool_create_info.pPoolSizes = &descriptorpool_size;
		svDescriptorpool_create_info.maxSets = max_frames;
		svDescriptorpool_create_info.flags = 0;
		svDescriptorpool_create_info.pNext = nullptr;
		vkCreateDescriptorPool(device, &svDescriptorpool_create_info, nullptr, &descriptorPool);

		// Instance Uniform Buffer descriptor set
		VkDescriptorSetAllocateInfo svDescriptorset_allocate_info = {};
		svDescriptorset_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		svDescriptorset_allocate_info.descriptorSetCount = 1;
		svDescriptorset_allocate_info.pSetLayouts = &svDescriptorLayout;
		svDescriptorset_allocate_info.descriptorPool = descriptorPool;
		svDescriptorset_allocate_info.pNext = nullptr;
		svDescriptorSet.resize(max_frames);
		for (int i = 0; i < max_frames; ++i)
		{
			vkAllocateDescriptorSets(device, &svDescriptorset_allocate_info, &svDescriptorSet[i]);
		}

		VkWriteDescriptorSet svWrite_descriptorset = {};
		svWrite_descriptorset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		svWrite_descriptorset.descriptorCount = 1;
		svWrite_descriptorset.dstArrayElement = 0;
		svWrite_descriptorset.dstBinding = 0;
		svWrite_descriptorset.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		for (int i = 0; i < max_frames; ++i) {
			svWrite_descriptorset.dstSet = svDescriptorSet[i];
			VkDescriptorBufferInfo dbinfo = { svHandle[i], 0, VK_WHOLE_SIZE };
			svWrite_descriptorset.pBufferInfo = &dbinfo;
			vkUpdateDescriptorSets(device, 1, &svWrite_descriptorset, 0, nullptr);
		}

		//// Push constant
		//VkPushConstantRange push_constant;
		//push_constant.offset = 0;
		//push_constant.size = sizeof(ShaderVars);
		//push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		// Descriptor pipeline layout
		VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
		pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_create_info.setLayoutCount = 1;
		/*VkDescriptorSetLayout layouts[2] = { vertexDescriptorLayout, pixelDescriptorLayout };
		pipeline_layout_create_info.pSetLayouts = layouts;*/
		pipeline_layout_create_info.pSetLayouts = &svDescriptorLayout;
		pipeline_layout_create_info.pushConstantRangeCount = 0;
		pipeline_layout_create_info.pPushConstantRanges = nullptr;
		vkCreatePipelineLayout(device, &pipeline_layout_create_info,
			nullptr, &pipelineLayout);
		
		// Pipeline State
		VkGraphicsPipelineCreateInfo pipeline_create_info = {};
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = stage_create_info;
		pipeline_create_info.pInputAssemblyState = &assembly_create_info;
		pipeline_create_info.pVertexInputState = &input_vertex_info;
		pipeline_create_info.pViewportState = &viewport_create_info;
		pipeline_create_info.pRasterizationState = &rasterization_create_info;
		pipeline_create_info.pMultisampleState = &multisample_create_info;
		pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
		pipeline_create_info.pColorBlendState = &color_blend_create_info;
		pipeline_create_info.pDynamicState = &dynamic_create_info;
		pipeline_create_info.layout = pipelineLayout;
		pipeline_create_info.renderPass = renderPass;
		pipeline_create_info.subpass = 0;
		pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
		vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
			&pipeline_create_info, nullptr, &pipeline);

		/***************** CLEANUP / SHUTDOWN ******************/
		// GVulkanSurface will inform us when to release any allocated resources
		shutdown.Create(vlk, [&]() {
			if (+shutdown.Find(GW::GRAPHICS::GVulkanSurface::Events::RELEASE_RESOURCES, true)) 
				CleanUp();
		});
	}
	
	void Render()
	{
		// Grab the current Vulkan commandBuffer
		unsigned int currentBuffer;
		vlk.GetSwapchainCurrentImage(currentBuffer);
		VkCommandBuffer commandBuffer;
		vlk.GetCommandBuffer(currentBuffer, (void**)&commandBuffer);
		
		// Setup the pipeline's dynamic settings
		unsigned int width, height;
		win.GetClientWidth(width);
		win.GetClientHeight(height);
		VkViewport viewport = {
			0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1
		};
		VkRect2D scissor = { {0, 0}, {width, height} };
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		// Build projection matrix
		float aspect;
		vlk.GetAspectRatio(aspect);
		matrixProxy.ProjectionVulkanLHF(65.0f * TO_RADIANS, aspect, 0.1f, 100.0f, projection);

		//// Set shader vars
		//ShaderVars sv;
		//matrixProxy.MultiplyMatrixF(view, projection, sv.viewProjection);
		matrixProxy.MultiplyMatrixF(view, projection, shaderVars.viewProjection);
		
		// Draw
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexHandle, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indexHandle, offsets[0], VK_INDEX_TYPE_UINT32);
		for (size_t i = 0; i < lvlData.uniqueMeshes.size(); i++)
		{	
			for (size_t j = 0; j < lvlData.uniqueMeshes[j].matrices.size(); j++)
			{
				shaderVars.matrices[j] = lvlData.uniqueMeshes[i].matrices[j];
			}
			//shaderVars.matrices[0] = lvlData.uniqueMeshes[i].matrices[0];
			//shaderVars.world = lvlData.uniqueMeshes[i].matrices[0];
			GvkHelper::write_to_buffer(device, svData[currentBuffer], &shaderVars, sizeof(ShaderVariables));
			
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout, 0, 1, &svDescriptorSet[currentBuffer], 0, nullptr);

			vkCmdDrawIndexed(commandBuffer, lvlData.uniqueMeshes[i].indexCount, 
				lvlData.uniqueMeshes[i].instanceCount, lvlData.uniqueMeshes[i].firstIndex, 
				lvlData.uniqueMeshes[i].vertexOffset, 0);
		}
	}

	// Call before Render. Updates the view matrix based on user input.
	void UpdateCamera()
	{
		// Timer
		static std::chrono::system_clock::time_point prevTime = std::chrono::system_clock::now();
		std::chrono::duration<float> deltaTime = std::chrono::system_clock::now() - prevTime;
		if (deltaTime.count() >= 1.0f / updatesPerSecond) {
			prevTime += std::chrono::system_clock::now() - prevTime;
		}

		// Move camera
		matrixProxy.InverseF(view, camera);
		GW::MATH::GVECTORF displacement;
		float spacebar = 0;
		float lshift = 0;
		float rtrigger = 0;
		float ltrigger = 0;
		float w = 0;
		float s = 0;
		float lsticky = 0;
		float d = 0;
		float a = 0;
		float lstickx = 0;
		inputProxy.GetState(G_KEY_SPACE, spacebar);
		inputProxy.GetState(G_KEY_LEFTSHIFT, lshift);
		inputProxy.GetState(G_RIGHT_TRIGGER_AXIS, rtrigger);
		inputProxy.GetState(G_LEFT_TRIGGER_AXIS, ltrigger);
		inputProxy.GetState(G_KEY_W, w);
		inputProxy.GetState(G_KEY_S, s);
		inputProxy.GetState(G_LY_AXIS, lsticky);
		inputProxy.GetState(G_KEY_D, d);
		inputProxy.GetState(G_KEY_A, a);
		inputProxy.GetState(G_LX_AXIS, lstickx);

		displacement = { 
			(d - a + lstickx)* deltaTime.count()* cameraMoveSpeed,
			0,
			(w - s + lsticky) * deltaTime.count() * cameraMoveSpeed };
		matrixProxy.TranslateLocalF(camera, displacement, camera);

		displacement = { 0, (spacebar - lshift + rtrigger - ltrigger) * deltaTime.count() * cameraMoveSpeed, 0 };
		vectorProxy.AddVectorF(camera.row4, displacement, camera.row4);

		// Rotate camera
		float mouseX = 0;
		float mouseY = 0;
		float rsticky = 0;
		float rstickx = 0;
		unsigned int screenHeight = 0;
		unsigned int screenWidth = 0;
		GW::GReturn result = inputProxy.GetMouseDelta(mouseX, mouseY);
		inputProxy.GetState(G_RY_AXIS, rsticky);
		inputProxy.GetState(G_RX_AXIS, rstickx);
		win.GetWidth(screenWidth);
		win.GetHeight(screenHeight);

		if (G_PASS(result) && result != GW::GReturn::REDUNDANT) 
		{
			float thumbSpeed = PI * deltaTime.count();
			float pitch = (60.0f * TO_RADIANS * mouseY * lookSensitivity) / (screenHeight + rsticky * (-thumbSpeed));
			GW::MATH::GMATRIXF rotation;
			matrixProxy.RotationYawPitchRollF(0, pitch, 0, rotation);
			matrixProxy.MultiplyMatrixF(rotation, camera, camera);
			
			float yaw = 60.0f * TO_RADIANS * mouseX * lookSensitivity / screenWidth + rstickx * thumbSpeed;
			matrixProxy.RotateYGlobalF(camera, yaw, camera);
		}
		
		// Apply to view matrix
		matrixProxy.InverseF(camera, view);
	}

	// Loads model + transform level data from gameLevelFile
	bool GetGameLevelData(std::vector<std::string>& _filenames, std::vector<GW::MATH::GMATRIXF>& _matrices) 
	{
		std::string line;
		std::ifstream file(levelFilePath, std::ios::in);
		
		// Failed to open
		if (!file.is_open())
			return false;
		
		// Read file
		while (std::getline(file, line)) {
			if (line.compare("MESH") == 0) {
				// Get name
				std::getline(file, line);
				//trim off extra characters
				auto index = line.find(".");
				if (index != std::string::npos)
					line = line.substr(0, index);
				//push back name
				_filenames.push_back(line);

				// Get matrix
				GW::MATH::GMATRIXF m;
				std::string row1, row2, row3, row4;
				std::getline(file, row1);
				std::getline(file, row2);
				std::getline(file, row3);
				std::getline(file, row4);
				//row1
				std::string sub = row1.substr(row1.find("(") + 1, row1.length() - 1);
				m.row1.data[0] = std::atof(sub.c_str());
				sub = sub.substr(sub.find(",") + 1, sub.length() - 1);
				m.row1.data[1] = std::atof(sub.c_str());
				sub = sub.substr(sub.find(",") + 1, sub.length() - 1);
				m.row1.data[2] = std::atof(sub.c_str());
				sub = sub.substr(sub.find(",") + 1, sub.length() - 1);
				m.row1.data[3] = std::atof(sub.c_str());
				//row2
				sub = row2.substr(row2.find("(") + 1, row2.length() - 1);
				m.row2.data[0] = std::atof(sub.c_str());
				sub = sub.substr(sub.find(",") + 1, sub.length() - 1);
				m.row2.data[1] = std::atof(sub.c_str());
				sub = sub.substr(sub.find(",") + 1, sub.length() - 1);
				m.row2.data[2] = std::atof(sub.c_str());
				sub = sub.substr(sub.find(",") + 1, sub.length() - 1);
				m.row2.data[3] = std::atof(sub.c_str());
				//row3
				sub = row3.substr(row3.find("(") + 1, row3.length() - 1);
				m.row3.data[0] = std::atof(sub.c_str());
				sub = sub.substr(sub.find(",") + 1, sub.length() - 1);
				m.row3.data[1] = std::atof(sub.c_str());
				sub = sub.substr(sub.find(",") + 1, sub.length() - 1);
				m.row3.data[2] = std::atof(sub.c_str());
				sub = sub.substr(sub.find(",") + 1, sub.length() - 1);
				m.row3.data[3] = std::atof(sub.c_str());
				//row4
				sub = row4.substr(row4.find("(") + 1, row4.length() - 1);
				m.row4.data[0] = std::atof(sub.c_str());
				sub = sub.substr(sub.find(",") + 1, sub.length() - 1);
				m.row4.data[1] = std::atof(sub.c_str());
				sub = sub.substr(sub.find(",") + 1, sub.length() - 1);
				m.row4.data[2] = std::atof(sub.c_str());
				sub = sub.substr(sub.find(",") + 1, sub.length() - 1);
				m.row4.data[3] = std::atof(sub.c_str());
				//push back matrix
				_matrices.push_back(m);
			}
		}
		file.close();
		_filenames.shrink_to_fit();
		_matrices.shrink_to_fit();

		// File read successfully
		return true;
	}

private:
	void CleanUp()
	{
		vkDeviceWaitIdle(device);

		vkDestroyBuffer(device, vertexHandle, nullptr);
		vkFreeMemory(device, vertexData, nullptr);
		vkDestroyBuffer(device, indexHandle, nullptr);
		vkFreeMemory(device, indexData, nullptr);

		vkDestroyShaderModule(device, vertexShader, nullptr);
		vkDestroyShaderModule(device, pixelShader, nullptr);

		for (size_t i = 0; i < max_frames; i++)
		{
			vkDestroyBuffer(device, svHandle[i], nullptr);
			vkFreeMemory(device, svData[i], nullptr);
		}
		svHandle.clear();
		svData.clear();

		vkDestroyDescriptorSetLayout(device, svDescriptorLayout, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
	}
};

// Load a shader file as a string of characters.
std::string ShaderAsString(const char* shaderFilePath) {
	std::string output;
	unsigned int stringLength = 0;
	GW::SYSTEM::GFile file; file.Create();
	file.GetFileSize(shaderFilePath, stringLength);
	if (stringLength && +file.OpenBinaryRead(shaderFilePath)) {
		output.resize(stringLength);
		file.Read(&output[0], stringLength);
	}
	return output;
}
