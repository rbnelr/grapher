#version 330
#include "common.glsl"

vs2fs vec2 vs_uv;
#ifdef _VERTEX
	void main () {
		gl_Position = vec4(vec2(gl_VertexID & 1, gl_VertexID >> 1) * 4.0 - 1.0, 0.0, 1.0);
		vs_uv = vec2(vec2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0);
	}
#endif

#ifdef _FRAGMENT
	uniform float grid_size = 1;
	
	uniform vec3 base_col = vec3(0.01, 0.01, 0.02);
	
	out vec4 frag_col;
	void main () {
		vec4 clip = vec4(vs_uv * 2.0 - 1.0, 0,1);
		vec4 world = view.clip2world * clip;
		
		ivec2 xy = ivec2(floor(world / grid_size));
		
		float fac  = ((xy.x ^ xy.y) & 1) == 0 ? 1.0 : 0.85;
		
		frag_col = vec4(base_col * fac, 1);
	}
#endif
