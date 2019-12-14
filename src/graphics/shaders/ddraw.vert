R"(#version 300 es
layout(location = 0) in vec3 position;

layout (std140) uniform perFrame {
    lowp mat4 matViewProj;
	lowp vec3 cameraPos;
};

void main() {
    gl_Position = matViewProj * vec4(position, 1.0);
}

)"