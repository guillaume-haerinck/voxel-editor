#version 300 es
layout(location = 0) out lowp vec3 color;

in FSInput {
	lowp vec3 myValue;
} fin;

void main() {
	color = fin.myValue;
}