#version 330
#include "common.glsl"

/* // Didn't really seem to draw antialised lines, and since shader debuggers are nonexistent I can't fix it easily
#ifdef _VERTEX
	layout(location = 0) in vec3  pos;
	layout(location = 1) in float width;
	layout(location = 2) in vec4  col;
	
	out vs_Out {
		float width;
		vec4 col;
	} vs;
	
	out float vs_width;
	out vec4 vs_col;
	
	void main () {
		gl_Position = view.world2clip * vec4(pos, 1.0);
		
		vs.width = 10.0;
		vs.col   = col;
	}
#endif

#ifdef _GEOMETRY
	layout (lines) in;
	layout (triangle_strip, max_vertices = 4) out;
	
	in vs_Out {
		float width;
		vec4 col;
	} vs[];
	
	out vec4 geom_col;
	out vec2 geom_uv;
	out float geom_wh;
	
	void vertex (vec4 pos, vec4 col, vec2 normal, float wh, vec2 uv) {
		pos.xy *= view.viewport_size;
		pos.xy += normal;
		pos.xy *= view.inv_viewport_size;
		
		gl_Position = pos;
		geom_col = col;
		geom_uv = uv;
		geom_wh = wh;
		EmitVertex();
	}
	
	void main () {
		vec4 pos0 = gl_in[0].gl_Position;
		vec4 pos1 = gl_in[1].gl_Position;
		
		vec2 dir = pos1.xy - pos0.xy;
		float len = length(dir);
		
		vec2 normal = len != 0.0 ? vec2(-dir.y, dir.x) / len : vec2(0.0);
		
		float wh0 = vs[0].width * 0.5 + 0.5;
		float wh1 = vs[1].width * 0.5 + 0.5;
		
		vec2 side0 = normal * wh0;
		vec2 side1 = normal * wh1;
		
		vertex(pos0, vs[0].col, -side0, wh0, vec2(-wh0, 0.0));
		vertex(pos0, vs[0].col, +side0, wh0, vec2(+wh0, 0.0));
		vertex(pos1, vs[1].col, -side1, wh1, vec2(-wh1, len));
		vertex(pos1, vs[1].col, +side1, wh1, vec2(+wh1, len));
		
		EndPrimitive();
	}
#endif

#ifdef _FRAGMENT
	in vec4 geom_col;
	in vec2 geom_uv;
	in float geom_wh;
	
	out vec4 frag_col;
	void main () {
		float alpha = clamp(geom_wh - abs(geom_uv.x), 0.0, 1.0);
		
		frag_col = vec4(vec3(1.0 - alpha), 1.0);
		//frag_col = vec4(geom_col.rgb, geom_col.a * alpha);
	}
#endif
*/

vs2fs vec4 vs_col;

#ifdef _VERTEX
	layout(location = 0) in vec3  pos;
	layout(location = 1) in vec4  col;
	
	void main () {
		gl_Position = view.world2clip * vec4(pos, 1.0);
		vs_col   = col;
	}
#endif
#ifdef _FRAGMENT
	out vec4 frag_col;
	void main () {
		frag_col = vs_col;
	}
#endif