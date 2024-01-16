#version 430

float hash12(vec2 p)
{
    vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise( in vec2 x )
{
    vec2 p = floor(x);
    vec2 w = fract(x);
    #if 1
    vec2 u = w*w*w*(w*(w*6.0-15.0)+10.0);
    #else
    vec2 u = w*w*(3.0-2.0*w);
    #endif

    float a = hash12(p+vec2(0,0));
    float b = hash12(p+vec2(1,0));
    float c = hash12(p+vec2(0,1));
    float d = hash12(p+vec2(1,1));

    return -1.0+2.0*(a + (b-a)*u.x + (c-a)*u.y + (a - b - c + d)*u.x*u.y);
}

float sdBox( vec3 p, float b )
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.f));
}

float sdTorus( vec3 p, vec2 t )
{
    vec2 q = vec2(length(p.xz)-t.x,p.y);
    return length(q)-t.y;
}

float map(vec3 pos)
{
    float d = pos.y - noise(pos.xz * 2.) * .3 + .0;
//    float d = sdBox(pos, .5) - .02;
//    float d2 = sdTorus(pos, vec2(.85,.2));
//    d = max(-d, d2);
    return d;
}

layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout (binding = 0, r8) writeonly uniform image3D outImage[5];
layout (binding = 0) uniform atomic_uint counter;
void main()
{
    const vec3 vertmap[] = {
        {0,0,0},{1,0,0},{0,1,0},{1,1,0},
        {0,0,1},{1,0,1},{0,1,1},{1,1,1},
    };

    vec3 res = vec3(imageSize(outImage[0]));
    ivec3 id = ivec3(gl_GlobalInvocationID.xyz);

    uint mask = 0;
    for (int i=0; i<8; i++)
    {
        float d = map((vec3(id)+vertmap[i])/res *2.-1.);
        mask |= int(d < 0) << i;
    }

    bool solid = mask != 0xff && mask != 0;
    imageStore(outImage[0], id, vec4( solid ));

    if (solid)
    {
        atomicCounterIncrement(counter);
    }
}
