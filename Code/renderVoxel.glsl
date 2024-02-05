#version 430

mat3 setCamera(in vec3 ro, in vec3 ta, float cr)
{
    vec3 cw = normalize(ta-ro);
    vec3 cp = vec3(sin(cr), cos(cr), 0.0);
    vec3 cu = normalize(cross(cw, cp));
    vec3 cv = cross(cu, cw);
    return mat3(cu, cv, cw);
}

vec2 boxIntersection( in vec3 ro, in vec3 rd, vec3 boxSize, out vec3 outNormal )
{
    vec3 m = 1.0/rd;
    vec3 n = m*ro;
    vec3 k = abs(m)*boxSize;
    vec3 t1 = -n - k;
    vec3 t2 = -n + k;
    float tN = max( max( t1.x, t1.y ), t1.z );
    float tF = min( min( t2.x, t2.y ), t2.z );
    if( tN>tF || tF<0.0) return vec2(-1.0);
    outNormal = (tN>0.0) ? step(vec3(tN),t1)
                         : step(t2,vec3(tF));
    outNormal *= -sign(rd);
    return vec2( tN, tF );
}

layout (binding = 1) uniform sampler3D iChannel1;
vec4 castRay(in vec3 ro, in vec3 rd, vec3 nor)
{
    int lvl = 4;
    int voxelSize = 1<<lvl;
    ivec3 res = textureSize(iChannel1, 0);
    ro -= nor*.0001;

    vec3 ird = 1./rd;
    vec3 srd = sign(rd);

    ivec3 id = ivec3(floor(ro));
        id -= id%voxelSize;
    vec3 sd = (vec3(id)-ro)*ird + float(voxelSize)*max(srd,0.0)*ird;

    bool hit = false;
    int steps = 0;
    vec3 mask = abs(nor);
    float tF = 0.;

    for (;;)
    {
        vec3 sp = (vec3(id)+.5)/vec3(res);
        float val = textureLod( iChannel1, sp, float(lvl) ).r;
        hit = val != 0;

        if (hit)
        {
            if (lvl > 0)
            {
                lvl--;
                voxelSize = 1<<lvl;
                id = ivec3(floor(ro + rd*tF + mask*srd*.0001));
                id -= id%voxelSize;
                sd = ( (vec3(id)-ro) + float(voxelSize)*max(srd,0.0) )*ird;
                continue;
            }
            else
            {
                break;
            }
        }

        steps += 1;
        tF = min(min(sd.x, sd.y), sd.z);
        mask = step(sd.xyz, min(sd.yzx, sd.zxy));
        sd += mask * srd * ird * float(voxelSize);
        id += ivec3(mask * srd) * voxelSize;

        if ( any(greaterThanEqual(id, res)) ||
             any(lessThan(id, ivec3(0))) )
            break;
    }

    return vec4(distance(ro, id), mask*float(hit));
}

layout (local_size_x = 16, local_size_y = 9) in;
layout (binding = 0, rgba8) writeonly uniform image2D outImage;
layout (location = 0) uniform float iTime;
void main(void)
{
    vec2 fragCoord = vec2(gl_GlobalInvocationID.xy);
    vec2 resolution = vec2(imageSize(outImage));
    vec2 uv = (2.0*fragCoord - resolution.xy)/resolution.y;

    float t = iTime;
    vec3 ta = vec3(0);
    vec3 ro = ta + vec3(cos(t),.3,sin(t)) * 1.;
    mat3 ca = setCamera(ro, ta, 0.0);
    vec3 rd = ca * normalize(vec3(uv, 1.2));

    vec3 col = vec3(0);
    vec3 res = vec3(textureSize(iChannel1, 0));
#if 0
    vec4 ret = castRay(ro+res*.5, rd, vec3(0));
//    col += ret.x/150.;
    col += dot(ret.yzw, vec3(.5,.7,.9))*.7;

    col += 0.3 * mix(vec3(1,0,0), vec3(0,1,0), floor(log2(ret.x))/6.);
    col = pow(col, vec3(2.4545));

#else
    vec3 nor;
    vec2 tt = boxIntersection(ro, rd, vec3(.5), nor);
    if (tt.y > tt.x)
    {
        vec3 pos = ro + rd*max(tt.x, 0.); // start with ro if ro is inside of box
        vec4 ret = castRay(( pos+.5 )*res, rd, nor);

//        col += ret.x/150.;
        col += dot(ret.yzw, vec3(.5,.7,.9))*.7;
    }
#endif

    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(col, 1));
}
