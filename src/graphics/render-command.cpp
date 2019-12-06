#include "render-command.h"

#include <spdlog/spdlog.h>
#include <debug_break/debug_break.h>
#include <sstream>
#include <fstream>
#include <stb_image/stb_image.h>

#include "graphics/gl-exception.h"
#include "graphics/constant-buffer.h"

RenderCommand::RenderCommand(SingletonComponents& scomps) : m_scomps(scomps)
{
}

RenderCommand::~RenderCommand() {
	for (auto pipeline : m_scomps.pipelines) {
		GLCall(glDeleteProgram(pipeline.programIndex));
	}

	for (auto cb : m_scomps.constantBuffers) {
		GLCall(glDeleteBuffers(1, &cb.bufferId));
	}
}

void RenderCommand::clear() const {
	GLCall(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

scomp::AttributeBuffer RenderCommand::createAttributeBuffer(const void* vertices, unsigned int count, unsigned int stride, scomp::AttributeBufferUsage usage, scomp::AttributeBufferType type) const {
	unsigned int id;
	GLCall(glGenBuffers(1, &id));
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, id));
	GLenum glUsage;

	switch (usage) {
	case scomp::AttributeBufferUsage::STATIC_DRAW: glUsage = GL_STATIC_DRAW; break;
	case scomp::AttributeBufferUsage::DYNAMIC_DRAW:  glUsage = GL_DYNAMIC_DRAW; break;
	default:
		debug_break();
		assert(false && "[createAttributeBuffer] Unknow usage");
	}

	GLCall(glBufferData(GL_ARRAY_BUFFER, stride * count, vertices, glUsage));

	scomp::AttributeBuffer buffer = {};
	buffer.bufferId = id;
	buffer.byteWidth = stride * count;
	buffer.count = count;
	buffer.stride = stride;
	buffer.type = type;
	buffer.usage = usage;

	return buffer;
}

scomp::VertexBuffer RenderCommand::createVertexBuffer(const VertexInputDescription& vib, scomp::AttributeBuffer* attributeBuffers) const {
	GLuint va;
	GLCall(glGenVertexArrays(1, &va));
	GLCall(glBindVertexArray(va));

	// Set layout
	unsigned int vbIndex = 0;
	unsigned int elementId = 0;
	for (const auto& element : vib) {
		GLCall(glBindBuffer(GL_ARRAY_BUFFER, attributeBuffers[elementId].bufferId));

		unsigned int iter = 1;
		if (element.type == ShaderDataType::Mat3)
			iter = 3;
		else if (element.type == ShaderDataType::Mat4)
			iter = 4;

		for (unsigned int i = 0; i < iter; i++) {
			GLCall(glEnableVertexAttribArray(vbIndex + i));
			GLCall(glVertexAttribPointer(
				vbIndex + i,
				element.getComponentCount() / iter,
				shaderDataTypeToOpenGLBaseType(element.type),
				element.normalized ? GL_TRUE : GL_FALSE,
				element.size,
				(const void*) ((element.size / iter) * i)
			));
			if (element.usage == BufferElementUsage::PerInstance) {
				GLCall(glVertexAttribDivisor(vbIndex + i, 1));
			}
		}
		elementId++;
		vbIndex += iter;
	}

	GLCall(glBindVertexArray(0));

	scomp::VertexBuffer vb = {};
	vb.vertexArrayId = va;
	vb.buffers.assign(attributeBuffers, attributeBuffers + vib.size());
	return vb;
}

scomp::IndexBuffer RenderCommand::createIndexBuffer(const void* indices, unsigned int count, scomp::IndexBuffer::dataType type) const {
	unsigned int id;
	GLCall(glGenBuffers(1, &id));
	GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id));
	GLCall(glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(unsigned int), indices, GL_STATIC_DRAW));

	scomp::IndexBuffer buffer = {};
	buffer.bufferId = id;
	buffer.count = count;
	buffer.type = type;

	return buffer;
}

scomp::ConstantBuffer RenderCommand::createConstantBuffer(scomp::ConstantBufferIndex index, unsigned int byteWidth, void* data) const {
	std::string name = "";
	switch (index) {
		case scomp::PER_FRAME: name = "perFrame"; break;
		case scomp::PER_PHONG_MAT_CHANGE: name = "perPhongMatChange"; break;
		case scomp::PER_COOK_MAT_CHANGE: name = "perCookMatChange"; break;
		case scomp::PER_LIGHT_CHANGE: name = "perLightChange"; break;
		default:
			spdlog::error("[createConstantBuffer] unknown index {}", index);
		break;
	}
	
	unsigned int cbId = 0;
	GLCall(glGenBuffers(1, &cbId));
	GLCall(glBindBuffer(GL_UNIFORM_BUFFER, cbId));
	GLCall(glBufferData(GL_UNIFORM_BUFFER, byteWidth, data, GL_DYNAMIC_COPY));
	GLCall(glBindBuffer(GL_UNIFORM_BUFFER, 0));

	scomp::ConstantBuffer cb = {};
	cb.bufferId = cbId;
	cb.byteWidth = byteWidth; // TODO assert that it is a multiple of 16
	cb.name = name;

	// Save to singleton components
	m_scomps.constantBuffers.at(index) = cb;

	return cb;
}

comp::Pipeline RenderCommand::createPipeline(const char* VSfilePath, const char* FSfilePath, scomp::ConstantBufferIndex* cbIndices, unsigned int cbCount) const {
	// TODO generate hash and check hashmap to see if pipeline already exist

	// Compile vertex shader
	std::string shader = readTextFile(VSfilePath);
	const char* src = shader.c_str();
	unsigned int vsId = glCreateShader(GL_VERTEX_SHADER);
	GLCall(glShaderSource(vsId, 1, &src, nullptr));
	GLCall(glCompileShader(vsId));
	if (!hasShaderCompiled(vsId, GL_VERTEX_SHADER)) {
		spdlog::info("[Shader] {}", VSfilePath);
		spdlog::info("\n{}", shader);
		debug_break();
	}

	// Compile fragment shader
	shader = readTextFile(FSfilePath);
	src = shader.c_str();
	unsigned int fsId = glCreateShader(GL_FRAGMENT_SHADER);
	GLCall(glShaderSource(fsId, 1, &src, nullptr));
	GLCall(glCompileShader(fsId));
	if (!hasShaderCompiled(fsId, GL_FRAGMENT_SHADER)) {
		spdlog::info("[Shader] {}", FSfilePath);
		spdlog::info("\n{}", shader);
		debug_break();
	}

	// Compile pipeline
	unsigned int programId = glCreateProgram();
	GLCall(glAttachShader(programId, vsId));
	GLCall(glAttachShader(programId, fsId));
	GLCall(glLinkProgram(programId));
	GLCall(glDeleteShader(vsId));
	GLCall(glDeleteShader(fsId));
	
	// Check fs and vs link errors
	GLCall(glValidateProgram(programId));
	GLint isLinked = 0;
	GLCall(glGetProgramiv(programId, GL_LINK_STATUS, (int*)&isLinked));
	if (isLinked == GL_FALSE) {
		GLint maxLength = 0;
		glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &maxLength);

		// FIXME
		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(programId, maxLength, &maxLength, &infoLog[0]);
		glDeleteProgram(programId);

		spdlog::error("[createPipeline] Cannot link shader !");
		debug_break();
		assert(false);
	}

	// Link constant buffers
	scomp::Pipeline sPipeline = {};
	for (unsigned int i = 0; i < cbCount; i++) {
		scomp::ConstantBuffer& cb = m_scomps.constantBuffers.at(cbIndices[i]);
		unsigned int blockIndex = glGetUniformBlockIndex(programId, cb.name.c_str());
		GLCall(glUniformBlockBinding(programId, blockIndex, i));
		GLCall(glBindBufferBase(GL_UNIFORM_BUFFER, i, cb.bufferId));
		sPipeline.constantBufferIndices.push_back(cbIndices[i]);
	}
	
	// Save to singleton components
	sPipeline.programIndex = programId;
	m_scomps.pipelines.push_back(sPipeline);

	// Return result
	comp::Pipeline pipeline = {};
	pipeline.index = static_cast<unsigned int>(m_scomps.pipelines.size()) - 1;
	return pipeline;
}

void RenderCommand::bindVertexBuffer(const scomp::VertexBuffer& vb) const {
	GLCall(glBindVertexArray(vb.vertexArrayId));
}

void RenderCommand::bindIndexBuffer(const scomp::IndexBuffer& ib) const {
	GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib.bufferId));
}

void RenderCommand::bindPipeline(const comp::Pipeline& pipeline) const {
	scomp::Pipeline sPipeline = m_scomps.pipelines.at(pipeline.index);
	GLCall(glUseProgram(sPipeline.programIndex));
}

void RenderCommand::updateConstantBuffer(const scomp::ConstantBuffer& cb, void* data) const {
	GLCall(glBindBuffer(GL_UNIFORM_BUFFER, cb.bufferId));
	GLCall(glBufferSubData(GL_UNIFORM_BUFFER, 0, cb.byteWidth, data));
	GLCall(glBindBuffer(GL_UNIFORM_BUFFER, 0));
}

void RenderCommand::updateAttributeBuffer(const scomp::AttributeBuffer& buffer, void* data, unsigned int dataByteWidth) const {
	assert(dataByteWidth <= buffer.byteWidth && "New attribute buffer data exceed the size of the allocated buffer");
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, buffer.bufferId));
	GLCall(glBufferSubData(GL_ARRAY_BUFFER, 0, dataByteWidth, data));
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

void RenderCommand::drawIndexed(unsigned int count, scomp::IndexBuffer::dataType type) const {
	GLCall(glDrawElements(GL_TRIANGLES, count, indexBufferDataTypeToOpenGLBaseType(type), (void*) 0));
}

void RenderCommand::drawIndexedInstances(unsigned int indexCount, scomp::IndexBuffer::dataType type, unsigned int drawCount) const {
	GLCall(glDrawElementsInstanced(GL_TRIANGLES, indexCount, indexBufferDataTypeToOpenGLBaseType(type), (void*)0, drawCount));
}

bool RenderCommand::hasShaderCompiled(unsigned int shaderId, unsigned int shaderType) const {
	int result;
	glGetShaderiv(shaderId, GL_COMPILE_STATUS, &result);

	if (result == GL_FALSE) {
		int length;
		glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &length);
		char* message = (char*) alloca(length * sizeof(char));
		glGetShaderInfoLog(shaderId, length, &length, message);
		auto const typeString = [shaderType]() {
			switch (shaderType) {
			case GL_VERTEX_SHADER: return "vertex";
			case GL_FRAGMENT_SHADER: return "fragment";
			default: return "unknown type";
			}
		}();

		spdlog::error("[Shader] Failed to compile {} shader", typeString);
		spdlog::error("[Shader] {}", message);
		GLCall(glDeleteShader(shaderId));
		return false;
	}

	return true;
}

GLenum RenderCommand::shaderDataTypeToOpenGLBaseType(ShaderDataType type) const {
	switch (type) {
	case ShaderDataType::Float:    return GL_FLOAT;
	case ShaderDataType::Float2:   return GL_FLOAT;
	case ShaderDataType::Float3:   return GL_FLOAT;
	case ShaderDataType::Float4:   return GL_FLOAT;
	case ShaderDataType::Mat3:     return GL_FLOAT;
	case ShaderDataType::Mat4:     return GL_FLOAT;
	case ShaderDataType::Int:      return GL_INT;
	case ShaderDataType::Int2:     return GL_INT;
	case ShaderDataType::Int3:     return GL_INT;
	case ShaderDataType::Int4:     return GL_INT;
	case ShaderDataType::Bool:     return GL_BOOL;
	case ShaderDataType::None:	   break;
	}

	assert(false && "Unknown ShaderDataType!");
	return 0;
}

GLenum RenderCommand::indexBufferDataTypeToOpenGLBaseType(scomp::IndexBuffer::dataType type) const {
	switch (type) {
	case scomp::IndexBuffer::dataType::UNSIGNED_BYTE : return GL_UNSIGNED_BYTE;
	case scomp::IndexBuffer::dataType::UNSIGNED_SHORT : return GL_UNSIGNED_SHORT; 
	case scomp::IndexBuffer::dataType::UNSIGNED_INT : return GL_UNSIGNED_INT;
	default:	break;
	}

	assert(false && "Unknown indexBufferDataType!");
	return 0;
}

std::string RenderCommand::readTextFile(const char* filePath) const {
	std::ifstream fileStream(filePath);
	if (!fileStream) {
		spdlog::error("[readTextFile] Cannot load file : {}", filePath);
		debug_break();
		assert(false);
	}

	std::stringstream shaderStream;
	shaderStream << fileStream.rdbuf();
	fileStream.close();
	return shaderStream.str();
}
