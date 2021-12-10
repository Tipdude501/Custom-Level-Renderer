// minimalistic code to draw a single triangle, this is not part of the API.
#include "shaderc/shaderc.h" // needed for compiling shaders at runtime
#ifdef _WIN32 // must use MT platform DLL libraries on windows
	#pragma comment(lib, "shaderc_combined.lib") 
#endif

#include <chrono>

#define PI 3.14159265359f
#define TO_RADIANS PI / 180.0f

//Forward declaration
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
	// proxy handles
	GW::SYSTEM::GWindow win;
	GW::GRAPHICS::GVulkanSurface vlk;
	GW::CORE::GEventReceiver shutdown;

	GW::INPUT::GInput inputProxy;
	GW::INPUT::GController controllerProxy;
	float timeScale = 60;						//updates per second
	float cameraSpeed = 0.18f;

	GW::MATH::GMATRIXF floorWorld;
	GW::MATH::GMATRIXF ceilingWorld;
	GW::MATH::GMATRIXF wallWorld1;
	GW::MATH::GMATRIXF wallWorld2;
	GW::MATH::GMATRIXF wallWorld3;
	GW::MATH::GMATRIXF wallWorld4;

	GW::MATH::GMatrix matrixProxy;
	GW::MATH::GVector vectorProxy;

	GW::MATH::GMATRIXF camera;
	GW::MATH::GMATRIXF view;

	GW::MATH::GMATRIXF projection;

	// what we need at a minimum to draw a triangle
	VkDevice device = nullptr;
	VkBuffer vertexHandle = nullptr;
	VkDeviceMemory vertexData = nullptr;
	VkShaderModule vertexShader = nullptr;
	VkShaderModule pixelShader = nullptr;
	// pipeline settings for drawing (also required)
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

		inputProxy.Create(win);
		controllerProxy.Create();

		matrixProxy.Create();
		vectorProxy.Create();

		//init all
		matrixProxy.IdentityF(floorWorld);
		matrixProxy.IdentityF(ceilingWorld);
		matrixProxy.IdentityF(wallWorld1);
		matrixProxy.IdentityF(wallWorld2);
		matrixProxy.IdentityF(wallWorld3);
		matrixProxy.IdentityF(wallWorld4);

		//floor
		matrixProxy.RotateXGlobalF(floorWorld, 90.0f * TO_RADIANS, floorWorld);
		floorWorld.row4 = { 0, -0.5f, 0, 1 };

		matrixProxy.RotateXGlobalF(ceilingWorld, 90.0f * TO_RADIANS, ceilingWorld);
		ceilingWorld.row4 = { 0, 0.5f, 0, 1 };

		matrixProxy.RotateYGlobalF(wallWorld1, 90.0f * TO_RADIANS, wallWorld1);
		wallWorld1.row4 = { -0.5f, 0, 0, 1 };

		matrixProxy.RotateYGlobalF(wallWorld2, 90.0f * TO_RADIANS, wallWorld2);
		wallWorld2.row4 = { 0.5f, 0, 0, 1 };

		wallWorld3.row4 = { 0, 0, -0.5f, 1 };
		wallWorld4.row4 = { 0, 0, 0.5f, 1 };


		GW::MATH::GVECTORF eye = { 0.5f, .2f, -0.5f };
		GW::MATH::GVECTORF at = { 0.0f, 0.0f, 0.0f };
		GW::MATH::GVECTORF up = { 0.0f, 1.0f, 0.0f };
		matrixProxy.LookAtLHF(eye, at, up, view);

		/***************** GEOMETRY INTIALIZATION ******************/
		// Grab the device & physical device so we can allocate some stuff
		VkPhysicalDevice physicalDevice = nullptr;
		vlk.GetDevice((void**)&device);
		vlk.GetPhysicalDevice((void**)&physicalDevice);


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
		shaderc_result_release(result); // done
		// Create Pixel Shader
		result = shaderc_compile_into_spv( // compile
			compiler, pixelShaderSource, strlen(pixelShaderSource),
			shaderc_fragment_shader, "main.frag", "main", options);
		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
			std::cout << "Pixel Shader Errors: " << shaderc_result_get_error_message(result) << std::endl;
		GvkHelper::create_shader_module(device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &pixelShader);
		shaderc_result_release(result); // done
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
		assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		assembly_create_info.primitiveRestartEnable = false;
		// Vertex Input State
		VkVertexInputBindingDescription vertex_binding_description = {};
		vertex_binding_description.binding = 0;
		vertex_binding_description.stride = sizeof(Vertex);
		vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		VkVertexInputAttributeDescription vertex_attribute_description[2] = {
			{ 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, position) },	//position
			{ 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color) }		//color
		};
		VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
		input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		input_vertex_info.vertexBindingDescriptionCount = 1;
		input_vertex_info.pVertexBindingDescriptions = &vertex_binding_description;
		input_vertex_info.vertexAttributeDescriptionCount = 2;
		input_vertex_info.pVertexAttributeDescriptions = vertex_attribute_description;
		// Viewport State (we still need to set this up even though we will overwrite the values)
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
		// Pipeline State... (FINALLY) 
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
			if (+shutdown.Find(GW::GRAPHICS::GVulkanSurface::Events::RELEASE_RESOURCES, true)) {
				CleanUp(); // unlike D3D we must be careful about destroy timing
			}
			});
	}
	void Render()
	{
		// grab the current Vulkan commandBuffer
		unsigned int currentBuffer;
		vlk.GetSwapchainCurrentImage(currentBuffer);
		VkCommandBuffer commandBuffer;
		vlk.GetCommandBuffer(currentBuffer, (void**)&commandBuffer);
		// what is the current client area dimensions?
		unsigned int width, height;
		win.GetClientWidth(width);
		win.GetClientHeight(height);
		// setup the pipeline's dynamic settings
		VkViewport viewport = {
			0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1
		};
		VkRect2D scissor = { {0, 0}, {width, height} };
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		float aspect;
		vlk.GetAspectRatio(aspect);
		matrixProxy.ProjectionVulkanLHF(65.0f * TO_RADIANS, aspect, 0.1f, 100.0f, projection);

		ShaderVars sv;
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
	}

	void UpdateCamera()
	{
		//Timer
		static std::chrono::system_clock::time_point prevTime = std::chrono::system_clock::now();
		std::chrono::duration<float> deltaTime = std::chrono::system_clock::now() - prevTime;
		if (deltaTime.count() >= 1.0f / timeScale) {
			prevTime += std::chrono::system_clock::now() - prevTime;
		}

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
			(d - a + lstickx)* deltaTime.count()* cameraSpeed,
			0,
			(w - s + lsticky) * deltaTime.count() * cameraSpeed };
		matrixProxy.TranslateLocalF(camera, displacement, camera);

		displacement = { 0, (spacebar - lshift + rtrigger - ltrigger) * deltaTime.count() * cameraSpeed, 0 };
		vectorProxy.AddVectorF(camera.row4, displacement, camera.row4);

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
			float pitch = (60.0f * TO_RADIANS * mouseY) / (screenHeight + rsticky * (-thumbSpeed));
			GW::MATH::GMATRIXF rotation;
			matrixProxy.RotationYawPitchRollF(0, pitch, 0, rotation);
			matrixProxy.MultiplyMatrixF(rotation, camera, camera);
			
			float yaw = 60.0f * TO_RADIANS * mouseX / screenWidth + rstickx * thumbSpeed;
			matrixProxy.RotateYGlobalF(camera, yaw, camera);
		}
		
		matrixProxy.InverseF(camera, view);
	}
private:
	void CleanUp()
	{
		// wait till everything has completed
		vkDeviceWaitIdle(device);
		// Release allocated buffers, shaders & pipeline
		vkDestroyBuffer(device, vertexHandle, nullptr);
		vkFreeMemory(device, vertexData, nullptr);
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
