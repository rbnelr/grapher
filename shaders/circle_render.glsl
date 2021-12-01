#version 330
#include "common.glsl"

vs2fs vec2  vs_pos;
vs2fs float vs_size;
vs2fs vec4  vs_col;

#ifdef _VERTEX
	layout(location = 0) in vec2  mesh_pos;
	
	layout(location = 1) in vec3  inst_pos;
	layout(location = 2) in float inst_size; // in pixels
	layout(location = 3) in vec4  inst_col;
	
	void main () {
		vec4 pos_clip = view.world2clip * vec4(inst_pos, 1.0);
		
		vec2 pos = mesh_pos * (inst_size + 0.5); // add half a pixel for AA
		
		pos_clip.xy += pos * view.inv_viewport_size * 2.0;
		
		gl_Position = pos_clip;
		
		vs_pos  = pos;
		vs_size = inst_size / 2.0;
		vs_col  = inst_col;
	}
#endif
#ifdef _FRAGMENT
	out vec4 frag_col;
	void main () {
		float alpha = clamp(vs_size - length(vs_pos), 0.0, 1.0);
		
		frag_col = vec4(vs_col.rgb, vs_col.a * alpha);
	}
#endif
