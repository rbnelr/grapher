#include "common_app.hpp"
#include "equations.hpp"

struct LineDrawer {
	
	Shader* shad = g_shaders.compile("lines");

	struct Vertex {
		float3 pos;
		// float width
		float4 col;

		static void attrib (int idx) {
			ATTRIB(idx++, GL_FLOAT,3, Vertex, pos)
			ATTRIB(idx++, GL_FLOAT,4, Vertex, col)
		}
	};
	VertexBuffer vbo = vertex_buffer<Vertex>("LineDrawer");

	std::vector<Vertex> vertices;

	bool antialias = true;
	float line_w = 1.0f;

	void imgui () {
		ImGui::Checkbox("line antialias", &antialias);
		ImGui::DragFloat("line thickness", &line_w, 0.02f, 0);
	}

	void begin () {
		vertices.clear();
	}

	void draw_line (float3 const& a, float3 const& b, float4 const& col) {
		auto* v = push_back(vertices, 2);
		v[0].pos = a;
		v[0].col = col;
		v[1].pos = b;
		v[1].col = col;
	}

	void render (StateManager& state) {
		OGL_TRACE("LineDrawer");
		ZoneScoped

		glSetEnable(GL_LINE_SMOOTH, antialias);
		glLineWidth(line_w);

		glUseProgram(shad->prog);

		PipelineState s;
		s.depth_test = false;
		s.blend_enable = true;
		state.set(s);

		vbo.stream(vertices);
		glBindVertexArray(vbo.vao);
		glDrawArrays(GL_LINES, 0, (GLsizei)vertices.size());
	}
};


struct App : public IApp {
	App () {}
	virtual ~App () {}

	Camera2D cam = Camera2D(0, 20.0f);
	Flycam flycam = Flycam(float3(0,-7,6), float3(0,-deg(30),0), 100);
	bool cam_select = true;

	Equations equations;

	ogl::Renderer r;
	LineDrawer lines;

	float text_size = 24.0f;

	Shader* grid_shad = g_shaders.compile("grid");
	Vao dummy_vao = {"dummy_vao"};

	void imgui (Input& I) {
		ZoneScoped

		r.debug_draw.imgui();
		r.text.imgui();
		lines.imgui();

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

		cam.imgui();
		flycam.imgui();
		ImGui::Checkbox("cam_select", &cam_select);

		ImGui::DragFloat("text_size", &text_size, 0.05f, 0, 64);
		ImGui::DragFloat("equation_res", &eq_res_px, 0.02f);

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

		equations.imgui();

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
	}

	void draw_background_grid () {
		OGL_TRACE("background_grid");
		ZoneScoped

		glUseProgram(grid_shad->prog);

		PipelineState s;
		s.depth_test = false;
		s.blend_enable = false;
		r.state.set(s);

		glBindVertexArray(dummy_vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	float4 world2screen (float3 const& pos, View3D const& view) {
		float4 clip = view.world2clip * float4(pos, 1);
		float4 ndc = clip / clip.w;

		return (ndc * 0.5f + 0.5f) * float4(view.viewport_size, 1,1);
	}
	float2 map_text (float3 const& pos, View3D const& view) {
		float4 screen = world2screen(pos, view);
		screen.y = view.viewport_size.y - screen.y;
		return (float2)screen;
	}


	float ticks_px = 3.0f;

	float eq_res_px = 1;

	float4 colX = float4(1.0f,0.1f,0.1f,1);
	float4 colY = float4(0.1f,1.0f,0.1f,1);
	float4 col_ticks_text = float4(0.8f,0.8f,0.8f,1);

	float4 eq_col = float4(0.95f,0.95f,0.95f,1);

	void draw_axes (View3D const& view) {
		ZoneScoped;

		float ticks_text_px = text_size * 0.66f;
		float axis_label_text_px = text_size * 1.25f;
		float2 ticks_text_padding = ticks_px * 1.5f;

		float2 px2world = view.frust_near_size / view.viewport_size;

		float2 view_size = view.frust_near_size * 0.5f;

		float x0 = view.cam_pos.x - view_size.x;
		float x1 = view.cam_pos.x + view_size.x;

		float y0 = view.cam_pos.y - view_size.y;
		float y1 = view.cam_pos.y + view_size.y;

		float2 line_pad = px2world * 10;

		// draw axes lines
		lines.draw_line(float3(x0-line_pad.x, 0,0), float3(x1+line_pad.x, 0,0), colX);
		lines.draw_line(float3( 0,y0-line_pad.y,0), float3( 0,y1+line_pad.y,0), colY);


		auto draw_axis_tick_text = [&] (float val, float x, float y, float2 const& align, int axis) {
			auto str = prints("%g", val);
			auto ptext = r.text.prepare_text(str, ticks_text_px, col_ticks_text);

			float2 pos_px = map_text(float3(x, y, 0), view);

			float2 padded_bounds = ptext.bounds + ticks_text_padding * 2.0f;
			float2 offset = pos_px + ticks_text_padding - padded_bounds * align;

			if      (axis == 1) offset.x = clamp(offset.x, ticks_text_padding.x, view.viewport_size.x - ptext.bounds.x - ticks_text_padding.x);
			else if (axis == 0) offset.y = clamp(offset.y, ticks_text_padding.y, view.viewport_size.y - ptext.bounds.y - ticks_text_padding.y);

			r.text.offset_glyphs(ptext.idx, ptext.len, offset);
		};

		draw_axis_tick_text(0, 0,0, float2(1,0), -1);

		float2 tick_sz = px2world * ticks_px;

		// draw X tick marks
		int x_mark0 = floori(x0), x_mark1 = ceili(x1);
		for (int x=x_mark0; x<=x_mark1; ++x) {
			if (x==0) continue;

			draw_axis_tick_text((float)x, (float)x, 0, float2(0.5f, 0), 0);

			lines.draw_line(float3((float)x, -tick_sz.y, 0), float3((float)x, +tick_sz.y, 0), colX);
		}
		// draw Y tick marks
		int y_mark0 = floori(y0), y_mark1 = ceili(y1);
		for (int y=y_mark0; y<=y_mark1; ++y) {
			if (y==0) continue;

			draw_axis_tick_text((float)y, 0, (float)y, float2(1, 0.5f), 1);

			lines.draw_line(float3(-tick_sz.x, (float)y, 0), float3(+tick_sz.x, (float) y,0), colY);
		}

		r.text.draw_text("x", axis_label_text_px, colX, map_text(float3(x1, 0, 0), view), float2(1,1), ticks_text_padding);
		r.text.draw_text("y", axis_label_text_px, colY, map_text(float3(0, y1, 0), view), float2(0,0), ticks_text_padding);

		{
			ZoneScopedN("draw equations");

			eq_res_px = max(eq_res_px, 1.0f / 8);

			float res = px2world.x * eq_res_px;

			int start = floori(x0 / res), end = ceili(x1 / res);
			
			Variables vars;
			auto eval = [&] (Equation& eq, float x, float* result) -> bool {
				vars.x = x;
				return eq.evaluate(vars, x, result);
			};

			for (auto& eq : equations.equations) {
				if (!eq.valid) continue;

				dbg_equation(eq);

				ZoneScopedN("draw equation");

				float prevt = (float)start * res;
				float prev;
				bool exec_valid = eval(eq, prevt, &prev);

				for (int i=start+1; exec_valid && i<=end; ++i) {
					float t = (float)i * res;
					float val;
					exec_valid = eval(eq, t, &val);

					if (!isnan(prev) && !isnan(val))
						lines.draw_line(float3(prevt, prev, 0), float3(t, val, 0), eq.col);

					prevt = t;
					prev = val;
				}
			}
		}
	}

	void render (Input& I, View3D const& view, int2 const& viewport_size) {
		ZoneScoped;

		r.begin(view, viewport_size);
		lines.begin();

		glClearColor(0.05f, 0.06f, 0.07f, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		draw_background_grid();

		draw_axes(view);

		//static float size = 20;
		//ImGui::SliderFloat("size", &size, 0, 70);
		//r.text.draw_text("Hello World!\n"
		//                 "f(x) = 5x^2 + 7x -9.3", map_text(float3(0,0,0), view), size);

		lines.render(r.state);
		r.text.render(r.state);
	}

	virtual void frame (Input& I) {
		ZoneScoped;
		imgui(I);

		View3D view;
		if (cam_select)
			view = cam.update(I, (float2)I.window_size);
		else
			view = flycam.update(I, (float2)I.window_size);
		
		render(I, view, I.window_size);
	}
};

inline IApp* make_app () { return new App(); }
int main () {
	return run_window(make_app, "Grapher");
}
