#version 460

layout(location = 0) out int FragColour;

layout(location = 0) flat in int id;

void main() {
    FragColour = id;
}