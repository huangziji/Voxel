#version 300 es
precision mediump float;

uniform sampler2D iChannel0;
out vec4 fragColor;
void main(void)
{
    ivec2 res = textureSize(iChannel0, 0);
    vec2 uv = gl_FragCoord.xy/vec2(res);
    fragColor = texture(iChannel0, uv);
}
