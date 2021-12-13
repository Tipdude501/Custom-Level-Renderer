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

// Simple Vertex Shader
std::string vertexShaderPath = ShaderAsString("../VertexShader.hlsl");
const char* vertexShaderSource = vertexShaderPath.c_str();
// Simple Pixel Shader
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
	std::string levelFilePath = "../../Assets/Levels/TestLevel.txt";
	
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
	/*GW::MATH::GMATRIXF floorWorld;
	GW::MATH::GMATRIXF ceilingWorld;
	GW::MATH::GMATRIXF wallWorld1;
	GW::MATH::GMATRIXF wallWorld2;
	GW::MATH::GMATRIXF wallWorld3;
	GW::MATH::GMATRIXF wallWorld4;*/
	GW::MATH::GMATRIXF camera;
	GW::MATH::GMATRIXF view;
	GW::MATH::GMATRIXF projection;

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
	struct Vertex {
		float position[4];
		float color[4];

		Vertex(
			float x = 0, float y = 0, float z = 0, float w = 1,
			float r = 0, float g = 0, float b = 0, float a = 1) {
			position[0] = x;
			position[1] = y;
			position[2] = z;
			position[3] = w;
			color[0] = r;
			color[1] = g;
			color[2] = b;
			color[3] = a;
		}
	};

	struct ShaderVars {
		GW::MATH::GMATRIXF world;
		GW::MATH::GMATRIXF viewProjection;
	};


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

		// Init box grid matrices
		/*matrixProxy.IdentityF(floorWorld);
		matrixProxy.IdentityF(ceilingWorld);
		matrixProxy.IdentityF(wallWorld1);
		matrixProxy.IdentityF(wallWorld2);
		matrixProxy.IdentityF(wallWorld3);
		matrixProxy.IdentityF(wallWorld4);
		matrixProxy.RotateXGlobalF(floorWorld, 90.0f * TO_RADIANS, floorWorld);
		floorWorld.row4 = { 0, -0.5f, 0, 1 };
		matrixProxy.RotateXGlobalF(ceilingWorld, 90.0f * TO_RADIANS, ceilingWorld);
		ceilingWorld.row4 = { 0, 0.5f, 0, 1 };
		matrixProxy.RotateYGlobalF(wallWorld1, 90.0f * TO_RADIANS, wallWorld1);
		wallWorld1.row4 = { -0.5f, 0, 0, 1 };
		matrixProxy.RotateYGlobalF(wallWorld2, 90.0f * TO_RADIANS, wallWorld2);
		wallWorld2.row4 = { 0.5f, 0, 0, 1 };
		wallWorld3.row4 = { 0, 0, -0.5f, 1 };
		wallWorld4.row4 = { 0, 0, 0.5f, 1 };*/

		// Init camera and view
		GW::MATH::GVECTORF eye = { 0.5f, .2f, -0.5f };
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
			if (parser.meshCount > 1) 
			{
				//push back submeshes, starting at submesh 2
				for (size_t submeshIndex = 1; submeshIndex < parser.meshCount; submeshIndex++)
				{
					LevelData::UniqueMesh submesh = lvlData.uniqueMeshes[uniqueMeshIndex];
					submesh.name += "_submesh" + std::to_string(submeshIndex + 1);
					submesh.indexCount = parser.meshes[submeshIndex].drawInfo.indexCount;
					submesh.firstIndex = lvlData.indices.size();
					lvlData.uniqueMeshes.push_back(submesh);

					//push back indices per submesh
					int start = parser.meshes[submeshIndex].drawInfo.indexOffset;
					int end = parser.meshes[submeshIndex].drawInfo.indexOffset + parser.meshes[submeshIndex].drawInfo.indexCount;
					for (size_t i = start; i < end; i++)
						lvlData.indices.push_back(parser.indices[i]);
				}

				//then write the first submesh data to the original unique mesh spot
				lvlData.uniqueMeshes[uniqueMeshIndex].name += "_submesh1";
				lvlData.uniqueMeshes[uniqueMeshIndex].indexCount = parser.meshes[0].drawInfo.indexCount;
				lvlData.uniqueMeshes[uniqueMeshIndex].firstIndex = lvlData.indices.size();
				
				//push back indices for first submest
				int start = parser.meshes[0].drawInfo.indexOffset;
				int end = parser.meshes[0].drawInfo.indexOffset + parser.meshes[0].drawInfo.indexCount;
				for (size_t i = start; i < end; i++)
					lvlData.indices.push_back(parser.indices[i]);
				

				// Push back all vertices
				for (size_t j = 0; j < parser.vertexCount; j++)
					lvlData.vertices.push_back(parser.vertices[j]);
			}
			else 
			{
				lvlData.uniqueMeshes[uniqueMeshIndex].indexCount = parser.indexCount;
				lvlData.uniqueMeshes[uniqueMeshIndex].firstIndex = lvlData.indices.size();
				lvlData.uniqueMeshes[uniqueMeshIndex].materialIndex = lvlData.materials.size();
				for (size_t i = 0; i < parser.vertexCount; i++)
					lvlData.vertices.push_back(parser.vertices[i]);
				for (size_t i = 0; i < parser.indexCount; i++)
					lvlData.indices.push_back(parser.indices[i]);
			}

			/*lvlData.uniqueMeshes[i].indexCount = parser.indexCount;
			lvlData.uniqueMeshes[i].firstIndex = lvlData.indices.size();
			lvlData.uniqueMeshes[i].materialIndex = lvlData.materials.size();
			for (size_t j = 0; j < parser.vertexCount; j++)
				lvlData.vertices.push_back(parser.vertices[j]);
			for (size_t j = 0; j < parser.indexCount; j++)
				lvlData.indices.push_back(parser.indices[j]);*/
		}

		/***************** GEOMETRY BUFFER INITIALIZATION ******************/
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

		/*
		// Create Vertex Buffer
		float size = 25.0f;
		std::vector<Vertex> tempVerts;
		float span = 0.5f;
		float delta = (span * 2.0f) / size;
		for (float i = 0, j = span; i <= size; i++, j -= delta)
		{
			tempVerts.push_back(Vertex(span, j));
			tempVerts.push_back(Vertex(-span, j));
		}
		for (float i = 0, j = span; i <= size; i++, j -= delta)
		{
			tempVerts.push_back(Vertex(j, span));
			tempVerts.push_back(Vertex(j, -span));
		}

		Vertex verts[(25 * 4) + 4];
		std::copy(tempVerts.begin(), tempVerts.end(), verts);


		// Transfer triangle data to the vertex buffer. (staging would be prefered here)
		GvkHelper::create_buffer(physicalDevice, device, sizeof(verts),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertexHandle, &vertexData);
		GvkHelper::write_to_buffer(device, vertexData, verts, sizeof(verts));
		*/

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

		// Push constant
		VkPushConstantRange push_constant;
		push_constant.offset = 0;
		push_constant.size = sizeof(ShaderVars);
		push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		// Descriptor pipeline layout
		VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
		pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_create_info.setLayoutCount = 0;
		pipeline_layout_create_info.pSetLayouts = VK_NULL_HANDLE;
		pipeline_layout_create_info.pushConstantRangeCount = 1; 
		pipeline_layout_create_info.pPushConstantRanges = &push_constant;
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

		// Set shader vars
		ShaderVars sv;
		sv.world = GW::MATH::GIdentityMatrixF;
		matrixProxy.MultiplyMatrixF(view, projection, sv.viewProjection);
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShaderVars), &sv);

		// Draw
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexHandle, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indexHandle, offsets[0], VK_INDEX_TYPE_UINT32);
		for (size_t i = 0; i < lvlData.uniqueMeshes.size(); i++)
		{
			vkCmdDrawIndexed(commandBuffer, lvlData.uniqueMeshes[i].indexCount, 
				lvlData.uniqueMeshes[i].instanceCount, lvlData.uniqueMeshes[i].firstIndex, 
				0, 0);
		}
		/*vkCmdDrawIndexed(commandBuffer, lvlData.uniqueMeshes[0].indexCount,
					lvlData.uniqueMeshes[0].instanceCount, lvlData.uniqueMeshes[0].firstIndex,
					lvlData.uniqueMeshes[0].vertexOffset, 0);*/

		/*
		sv.world = floorWorld;
		matrixProxy.MultiplyMatrixF(view, projection, sv.viewProjection);

		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShaderVars), &sv);
		// now we can draw
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexHandle, offsets);
		vkCmdDraw(commandBuffer, (25 * 4) + 4, 1, 0, 0);

		sv.world = ceilingWorld;
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShaderVars), &sv);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexHandle, offsets);
		vkCmdDraw(commandBuffer, (25 * 4) + 4, 1, 0, 0);

		sv.world = wallWorld1;
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShaderVars), &sv);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexHandle, offsets);
		vkCmdDraw(commandBuffer, (25 * 4) + 4, 1, 0, 0);

		sv.world = wallWorld2;
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShaderVars), &sv);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexHandle, offsets);
		vkCmdDraw(commandBuffer, (25 * 4) + 4, 1, 0, 0);

		sv.world = wallWorld3;
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShaderVars), &sv);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexHandle, offsets);
		vkCmdDraw(commandBuffer, (25 * 4) + 4, 1, 0, 0);

		sv.world = wallWorld4;
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShaderVars), &sv);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexHandle, offsets);
		vkCmdDraw(commandBuffer, (25 * 4) + 4, 1, 0, 0);
		*/
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
