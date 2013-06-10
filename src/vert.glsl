uniform float u_color_offset;
uniform float u_color_repeat_factor;

vec3 Hue( float hue )
{
	vec3 rgb = fract(hue + vec3(0.0,2.0/3.0,1.0/3.0));

	rgb = abs(rgb*2.0-1.0);
		
	return clamp(rgb*3.0-1.0,0.0,1.0);
}

vec3 HSVtoRGB( vec3 hsv )
{
	return ((Hue(hsv.x)-1.0)*hsv.y+1.0) * hsv.z;
}

varying vec4 v_color;

void main()
{
    v_color = vec4(HSVtoRGB(vec3(u_color_offset + gl_Vertex.y * u_color_repeat_factor, 1.0, 1.0)), 1.0);
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
