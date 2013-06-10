kernel void calc_wave(global float4 *position, global float *velocity, int width, int depth)
{
    int x_index = get_global_id(0);
    int z_index = get_global_id(1);
	int gid = z_index * get_global_size(0) + x_index;
    
    int x = gid % width;
    int z = gid / width;
    
    float a = position[z * width + max(x - 1, 0)        ].y;
    float d = position[z * width + min(x + 1, width - 1)].y;
    float s = position[max(z - 1, 0)         * width + x].y;
    float w = position[min(z + 1, depth - 1) * width + x].y;
    
    float pavg = (a + d + s + w) * 0.25f;
    float f = pavg - position[gid].y;
    
    velocity[gid] += -position[gid].y * 0.01f + f * 1.5f;
    velocity[gid] -= velocity[gid] * 0.01f;
    
    position[gid].y += velocity[gid];
    
    position[gid].y = mix(position[gid].y, pavg, 0.1f);
}

kernel void effect_wave(global float4 *position, global float *velocity, float x, float z)
{
    const float HEIGHT = -4.0f;
    
    int x_index = get_global_id(0);
    int z_index = get_global_id(1);
	int gid = z_index * get_global_size(0) + x_index;
    float4 p = position[gid];
    
    float dist = distance((float2)(p.x, p.z), (float2)(x, z));
    float factor = clamp(dist / 2.0f, 0.0f, 1.0f);
    float value = clamp(cos(factor * 3.141f) * 0.5f + 0.5f, 0.0f, 1.0f);
    position[gid] = (float4)(p.x, mix(HEIGHT, p.y, 1.0f - value), p.z, 1.0f);
}