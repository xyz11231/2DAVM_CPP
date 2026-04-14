#version 300 es
precision highp float;
precision highp sampler2D;
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTexture;
void main() {
    FragColor = texture(uTexture, vUV);
}
