#include "Pipeline.h"

#include "VulkanEngine.h"

Pipeline::Builder::Builder(VulkanEngine* engine) : m_engine(engine) {

	m_inputAssembly = {
		.topology = vk::PrimitiveTopology::eTriangleList
	};

	m_dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

	// this needs to explicitly be vk::PipelineViewportStateCreateInfo for some reason
	m_viewportState = vk::PipelineViewportStateCreateInfo {
		.viewportCount = 1,
		.scissorCount = 1,
	};

	m_rasterizer = {
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eBack,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = vk::False,
		.lineWidth = 1.0f,
	};

	m_multisampling = {
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
	};

	m_depthStencil = {
		.depthTestEnable = vk::True,
		.depthWriteEnable = vk::True,
		.depthCompareOp = vk::CompareOp::eLess,
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False
	};
}

Pipeline::Builder& Pipeline::Builder::setVertexInfo(const vk::VertexInputBindingDescription& bindings, const std::vector<vk::VertexInputAttributeDescription>& attributes) {

	m_bindings = bindings;
	m_attributes = attributes;
	m_vertexInputInfo = {
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindings,
		.vertexAttributeDescriptionCount = static_cast<u32>(attributes.size()),
		.pVertexAttributeDescriptions = attributes.data()
	};
	return *this;
}

Pipeline::Builder & Pipeline::Builder::addShaderStage(std::string path) {
	vk::ShaderStageFlagBits stage;
	if (path.ends_with(".vert.spv")) {
		stage = vk::ShaderStageFlagBits::eVertex;
	}
	else if (path.ends_with(".frag.spv")) {
		stage = vk::ShaderStageFlagBits::eFragment;
	}
	else {
		Logger::warn("Unrecognised shader stage: {}", path);
		stage = vk::ShaderStageFlagBits::eAll;
	}
	const std::vector<char> code = readFile(path);
	const vk::ShaderModuleCreateInfo createInfo = {
		.codeSize = code.size() * sizeof(char),
		.pCode = reinterpret_cast<const u32*>(code.data())
	};

	m_shaders.insert({stage, vk::raii::ShaderModule(m_engine->getDevice(), createInfo)});
	return *this;
}

Pipeline::Builder & Pipeline::Builder::addBinding(u32 set, u32 binding, vk::DescriptorType type, vk::ShaderStageFlagBits stage) {
	m_descriptorBindings.at(set).emplace_back(binding, type, 1, stage);
	return *this;
}

Pipeline::Builder & Pipeline::Builder::addDynamicState(const vk::DynamicState state) {
	m_dynamicStates.push_back(state);
	return *this;
}

Pipeline::Builder & Pipeline::Builder::setPolygonMode(const vk::PolygonMode polygonMode) {
	m_rasterizer.polygonMode = polygonMode;
	return *this;
}

Pipeline::Builder & Pipeline::Builder::setTopology(const vk::PrimitiveTopology topology) {
	m_inputAssembly.topology = topology;
	return *this;
}

Pipeline::Builder & Pipeline::Builder::setDepthCompareOp(const vk::CompareOp compareOp) {
	m_depthStencil.depthCompareOp = compareOp;
	return *this;
}

Pipeline::Builder & Pipeline::Builder::disableDepthTest() {
	m_depthStencil.depthTestEnable = vk::False;
	m_depthStencil.depthWriteEnable = vk::False;
	return *this;
}

Pipeline::Builder & Pipeline::Builder::setSamples(vk::SampleCountFlagBits samples) {
	m_multisampling.rasterizationSamples = samples;
	return *this;
}

Pipeline::Builder & Pipeline::Builder::addAttachment(const vk::Format format, const vk::PipelineColorBlendAttachmentState& attachment) {
	m_attachments.push_back(attachment);
	m_colourFormats.push_back(format);
	return *this;
}

Pipeline::Builder & Pipeline::Builder::addAttachment(const vk::Format format) {
	m_attachments.push_back({
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	});
	m_colourFormats.push_back(format);
	return *this;
}

std::unique_ptr<Pipeline> Pipeline::Builder::create() {
	assert(m_shaders.contains(vk::ShaderStageFlagBits::eVertex));
	assert(m_shaders.contains(vk::ShaderStageFlagBits::eFragment));
	assert(!m_attachments.empty());
	assert(m_attachments.size() == m_colourFormats.size());
	assert(!m_attributes.empty());

	std::vector<vk::raii::DescriptorSetLayout> descriptorLayouts;
	std::vector<vk::DescriptorSetLayout> cLayouts;
	for (const auto & bindings : m_descriptorBindings) {
		vk::DescriptorSetLayoutCreateInfo createInfo = {
			.bindingCount = static_cast<u32>(bindings.size()),
			.pBindings = bindings.data()
		};
		descriptorLayouts.emplace_back(m_engine->getDevice(), createInfo);
		cLayouts.push_back(*descriptorLayouts.back());
	}

	const vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {
		.setLayoutCount = static_cast<u32>(cLayouts.size()),
		.pSetLayouts = cLayouts.data(),
	};

	vk::raii::PipelineLayout pipelineLayout(m_engine->getDevice(), pipelineLayoutInfo);

	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
	for (const auto& [stage, shader] : m_shaders) {
		shaderStages.push_back(vk::PipelineShaderStageCreateInfo{
			.stage = stage,
			.module = shader,
			.pName = "main",
		});
	}

	const vk::PipelineDynamicStateCreateInfo dynamicState = {
		.dynamicStateCount = static_cast<u32>(m_dynamicStates.size()),
		.pDynamicStates = m_dynamicStates.data()
	};

	vk::PipelineColorBlendStateCreateInfo colorBlending = {
		.logicOpEnable = vk::False,
		.attachmentCount = static_cast<u32>(m_attachments.size()),
		.pAttachments = m_attachments.data()
	};

	vk::StructureChain chain = {
		vk::GraphicsPipelineCreateInfo {
			.stageCount = static_cast<u32>(shaderStages.size()),
			.pStages = shaderStages.data(),
			.pVertexInputState = &m_vertexInputInfo,
			.pInputAssemblyState = &m_inputAssembly,
			.pViewportState = &m_viewportState,
			.pRasterizationState = &m_rasterizer,
			.pMultisampleState = &m_multisampling,
			.pDepthStencilState = &m_depthStencil,
			.pColorBlendState = &colorBlending,
			.pDynamicState = &dynamicState,
			.layout = pipelineLayout,
			.subpass = 0
		},
		vk::PipelineRenderingCreateInfo {
			.colorAttachmentCount = static_cast<u32>(m_colourFormats.size()),
			.pColorAttachmentFormats = m_colourFormats.data(),
			.depthAttachmentFormat = m_engine->getDepthFormat(),
		}
	};

	return std::make_unique<Pipeline>(
		m_engine,
		vk::raii::Pipeline(m_engine->getDevice(), nullptr, chain.get<vk::GraphicsPipelineCreateInfo>()),
		std::move(pipelineLayout),
		std::move(descriptorLayouts)
	);
}

Pipeline::Pipeline(VulkanEngine* engine, vk::raii::Pipeline pipeline, vk::raii::PipelineLayout layout, std::vector<vk::raii::DescriptorSetLayout> descriptorSetLayouts)
	: m_engine(engine)
	, m_pipeline(std::move(pipeline))
	, m_layout(std::move(layout))
	, m_descriptorLayouts(std::move(descriptorSetLayouts))
{}

vk::raii::DescriptorSet Pipeline::createDescriptorSet(const u32 set) const {
	 vk::DescriptorSetAllocateInfo allocInfo = {
		.descriptorPool = m_engine->getDescriptorPool(),
		.descriptorSetCount = 1,
		.pSetLayouts = &*m_descriptorLayouts.at(set)
	};
	return std::move(m_engine->getDevice().allocateDescriptorSets(allocInfo).front());
}

const vk::raii::Pipeline & Pipeline::getPipeline() const {
	return m_pipeline;
}

const vk::raii::PipelineLayout& Pipeline::getLayout() const {
	return m_layout;
}

const vk::raii::DescriptorSetLayout & Pipeline::getDescriptorLayout(u32 set) const {
	return m_descriptorLayouts.at(set);
}
