#include "common_app.hpp"
#include "equations.hpp"
#include <array>

struct AxisUnits {
	char const* name;
	std::string unit_str;
	float scale;
	bool stretch;
	bool log;
};
inline AxisUnits std_unit_modes[] = {
	{ "1"      , ""   , 1,          false },
	{ "Log"    , ""   , 1,           true },
	{ "Deg"    , "deg", RAD_TO_DEG, false }, // TODO: proper degrees mode needs trig special degree trig functions 
	{ "Pi"     , "pi" , PI,         false },
};

struct App : public IApp {
	App () {}
	virtual ~App () {}

	// Equations
	Equations equations;

	// Display
	Camera2D cam = Camera2D(0, 10.0f);
	Flycam flycam = Flycam(float3(0,-7,6), float3(0,-deg(30),0), 100);

	ogl::Renderer r;
	LineRenderer lines;

	LineRenderer::DrawCall              axis_lines;
	std::vector<LineRenderer::DrawCall> eq_lines;

	Shader* grid_shad = g_shaders.compile("grid");
	Vao dummy_vao = {"dummy_vao"};

	bool mode_3d = false;

	float text_size = 24.0f;

	float axis_line_w = 1.49f; // round to 1 for not-AA
	bool axis_line_aa = true;

	struct Axis {
		std::string name;
		float4      col;

		AxisUnits   custom_unit = { "Custom" , "", 1, false };

		int         sel_unit = 0;

		const char* display_name;
		AxisUnits*  units;

		//bool        base_2 = false;

		void imgui (const char* axis) {
			int std_count = ARRLEN(std_unit_modes);

			ImGui::PushID(axis);
			ImGui::PushItemWidth(70);

			ImGui::Text(axis);

			ImGui::SameLine();
			ImGui::ColorEdit3("###col", &col.x, ImGuiColorEditFlags_NoInputs);

			ImGui::SameLine();
			ImGui::InputText("###name", &name);

			display_name = name.empty() ? axis : name.c_str();

			for (int i=0; i<std_count; ++i) {
				ImGui::SameLine();
				ImGui::RadioButton(std_unit_modes[i].name, &sel_unit, i);
			}
			ImGui::SameLine();
			ImGui::RadioButton(custom_unit.name, &sel_unit, std_count);

			if (sel_unit == std_count) {
				ImGui::SameLine();
				ImGui::InputFloat("###scale", &custom_unit.scale, 0,0, "%g");
			}

			//ImGui::Checkbox("base_2", &base_2);

			units = sel_unit < std_count ? &std_unit_modes[sel_unit] : &custom_unit;

			ImGui::PopItemWidth();
			ImGui::PopID();
		}

	};
	Axis axes[2] = {
		{ "", float4(1.0f,0.1f,0.1f,1) },
		{ "", float4(0.1f,1.0f,0.1f,1) },
	};

	float min_tick_dist_px = 80;

	struct AxisTick {
		float offs;
		float size; // > 0.75f gets text label
	};
	typedef std::array<AxisTick, 21> AxisTicks;

	float get_tick_step (Axis& a, float px2world, AxisTicks& ticks) {
		ticks.fill({});

		float units = min_tick_dist_px * px2world; // world units per <min_tick_dist_px>

		units /= a.units->scale;
		
		if (a.units->log) {
			units *= 0.75f; // use smaller spacing because log space can be confusing, so want more number labels

			float scale = powf(2.0f, ceil(log2f(units)));
			
			if (scale >= 1.0f) {
				return scale;
			} else {

				if (scale >= 0.5f) {
					ticks[0] = { log10f(0.2f), 0.75f };
					ticks[1] = { log10f(0.3f), 0.75f };
					ticks[2] = { log10f(0.4f), 0.75f };
					ticks[3] = { log10f(0.5f), 1.00f };
					ticks[4] = { log10f(0.6f), 0.75f };
					ticks[5] = { log10f(0.7f), 0.75f };
					ticks[6] = { log10f(0.8f), 0.75f };
					ticks[7] = { log10f(0.9f), 0.75f };
				} else if (scale >= 0.25f) {
					ticks[0] = { log10f(0.2f), 1.00f };
					ticks[1] = { log10f(0.3f), 0.75f };
					ticks[2] = { log10f(0.4f), 0.75f };
					ticks[3] = { log10f(0.5f), 1.00f };
					ticks[4] = { log10f(0.6f), 0.75f };
					ticks[5] = { log10f(0.7f), 0.75f };
					ticks[6] = { log10f(0.8f), 0.75f };
					ticks[7] = { log10f(0.9f), 0.75f };
				} else {
					ticks[0] = { log10f(0.2f), 1.00f };
					ticks[1] = { log10f(0.3f), 1.00f };
					ticks[2] = { log10f(0.4f), 1.00f };
					ticks[3] = { log10f(0.5f), 1.00f };
					ticks[4] = { log10f(0.6f), 1.00f };
					ticks[5] = { log10f(0.7f), 1.00f };
					ticks[6] = { log10f(0.8f), 1.00f };
					ticks[7] = { log10f(0.9f), 1.00f };
				}

				return 1.0f;
			}

		} else {
			//if (a.base_2) {
			//	float scale = powf(2.0f, ceil(log2f(units)));
			//	
			//	ticks[0] = { 0.25f, 0.50f };
			//	ticks[1] = { 0.50f, 0.75f };
			//	ticks[2] = { 0.75f, 0.50f };
			//
			//	return scale * a.units->scale;
			//}
			//else {
				float scale = powf(10.0f, ceil(log10f(units))); // rounded up to next ..., 0.1, 1, 10, 100, ...
				float fract = units / scale; // remaining factor  0.3 -> scale=1 factor=0.3    25 -> scale=10 factor=2.5 etc.

				if      (fract <= 0.2f)   {
					fract = 0.2f;

					ticks[0] = { 0.05f / fract, 0.50f };
					ticks[1] = { 0.10f / fract, 0.75f };
					ticks[2] = { 0.15f / fract, 0.50f };
				}
				else if (fract <= 0.5f)   {
					fract = 0.5f;

					ticks[0] = { 0.10f / fract, 0.50f };
					ticks[1] = { 0.20f / fract, 0.50f };
					ticks[2] = { 0.30f / fract, 0.50f };
					ticks[3] = { 0.40f / fract, 0.50f };
				}
				else  /* fract <= 1.0f */ {
					fract = 1.0f; 
					
					ticks[0] = { 0.25f / fract, 0.50f };
					ticks[1] = { 0.50f / fract, 0.75f };
					ticks[2] = { 0.75f / fract, 0.50f };
				}

				return fract * scale * a.units->scale;
			//}
		}
	}

	void imgui (Input& I) {
		ZoneScoped

		r.debug_draw.imgui();
		r.text.imgui();

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

		ImGui::Checkbox("3D", &mode_3d);

		ImGui::Spacing();
		axes[0].imgui("X");
		axes[1].imgui("Y");
		ImGui::Spacing();

		if (ImGui::TreeNode("Graphics")) {
			ImGui::DragFloat("text_size", &text_size, 0.05f, 0, 64);
			ImGui::DragFloat("equation_res", &eq_res_px, 0.02f);

			ImGui::Checkbox("axis_line_antialis", &axis_line_aa);
			ImGui::SliderFloat("axis_line_thickness", &axis_line_w, 0.5f, 8);

			cam.imgui();
			flycam.imgui();

			ImGui::TreePop();
		}

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

		equations.imgui();

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
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

	float eq_res_px = 1;

	float ticks_px = 5.0f;

	float4 col_ticks_text = float4(0.8f,0.8f,0.8f,1);

	float4 eq_col = float4(0.95f,0.95f,0.95f,1);

	// 
	float2 px2world;
	float2 tick_step;

	AxisTicks ticks_x;
	AxisTicks ticks_y;

	float x0, x1, y0, y1;

	void adjust_matrices (View3D& view) {
		//static float2 stretch = 1;
		//
		//ImGui::DragFloat("stretch x", &stretch.x, 0.05f);
		//ImGui::DragFloat("stretch y", &stretch.y, 0.05f);
		//
		//float4x4 stretch2world = (float4x4)scale(float3(1.0f/stretch, 1));
		//float4x4 world2stretch = (float4x4)scale(float3(stretch, 1));
		//
		//view.world2cam = view.world2cam * stretch2world;
		//view.cam2world = world2stretch * view.cam2world;
		//
		//view.world2clip = view.world2clip * stretch2world;
		//view.clip2world = world2stretch * view.clip2world;
		//
		//view.frust_near_size *= stretch;
		//view.cam_pos *= float3(stretch, 1);
	}
	void precompute (View3D const& view) {
		px2world = view.frust_near_size / view.viewport_size;

		tick_step.x = get_tick_step(axes[0], px2world.x, ticks_x);
		tick_step.y = get_tick_step(axes[1], px2world.y, ticks_y);
		
		float2 view_size = view.frust_near_size * 0.5f;

		x0 = view.cam_pos.x - view_size.x;
		x1 = view.cam_pos.x + view_size.x;

		y0 = view.cam_pos.y - view_size.y;
		y1 = view.cam_pos.y + view_size.y;
	}

	void draw_background_grid (View3D const& view) {
		OGL_TRACE("background_grid");
		ZoneScoped

		glUseProgram(grid_shad->prog);

		PipelineState s;
		s.depth_test = false;
		s.blend_enable = false;
		r.state.set(s);

		grid_shad->set_uniform("grid_size", tick_step);

		glBindVertexArray(dummy_vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	void draw_axes (View3D const& view) {
		ZoneScoped;

		axis_lines = lines.begin_draw(axis_line_w, axis_line_aa);
		auto& line_vc = axis_lines.vertex_count;

		{ // draw axes lines
			float2 line_pad = px2world * 10;

			line_vc += lines.draw_line(float3(x0-line_pad.x, 0,0), float3(x1+line_pad.x, 0,0), axes[0].col);
			line_vc += lines.draw_line(float3( 0,y0-line_pad.y,0), float3( 0,y1+line_pad.y,0), axes[1].col);
		}
		
		float ticks_text_px = text_size * 0.66f;
		float axis_label_text_px = text_size * 1.25f;
		float2 ticks_text_padding = ticks_px * 1.5f;

		auto draw_axis_tick_text = [&] (View3D const& view, float val, float unit_scale, float x, float y, float2 const& align, int axis, const char* unit) {
			val = val * unit_scale;
			if (axes[1].units->log)
				val = powf(10.0f, val);

			auto str = prints("%g%s", val, unit);
			auto ptext = r.text.prepare_text(str, ticks_text_px, col_ticks_text);

			float2 pos_px = map_text(float3(x, y, 0), view);

			float2 padded_bounds = ptext.bounds + ticks_text_padding * 2.0f;
			float2 offset = pos_px + ticks_text_padding - padded_bounds * align;

			if      (axis == 1) offset.x = clamp(offset.x, ticks_text_padding.x, view.viewport_size.x - ptext.bounds.x - ticks_text_padding.x);
			else if (axis == 0) offset.y = clamp(offset.y, ticks_text_padding.y, view.viewport_size.y - ptext.bounds.y - ticks_text_padding.y);

			r.text.offset_glyphs(ptext.idx, ptext.len, offset);
		};

		draw_axis_tick_text(view, 0, 1, 0,0, float2(1,0), -1, "");

		{ // draw tick marks
			float2 tick_sz = ticks_px * px2world;

			const char* unit_str_x = axes[0].units->unit_str.c_str();
			const char* unit_str_y = axes[1].units->unit_str.c_str();

			// unit_str=""   with scale=1    -> 0  1  2  3  4  5  6  7    
			// unit_str="pi" with scale=3.14 -> 0pi       1pi       2pi   
			// unit_str=""   with scale=3.14 -> 0         3.14      6.28  
			float text_unit_scale_x = axes[0].units->unit_str.empty() ? 1.0f : 1.0f / axes[0].units->scale;
			float text_unit_scale_y = axes[1].units->unit_str.empty() ? 1.0f : 1.0f / axes[1].units->scale;

			{ // X
				int start = floori(x0/tick_step.x);
				int   end =  ceili(x1/tick_step.x);
				for (int i=start; i<=end; ++i) {
					float x = (float)i * tick_step.x;

					if (i!=0) {
						draw_axis_tick_text(view, x, text_unit_scale_x, x, 0, float2(0.5f, 0), 1, unit_str_x);

						line_vc += lines.draw_line(float3(x, -tick_sz.y, 0), float3(x, +tick_sz.y, 0), axes[0].col);
					}

					for (int j = 0; j < (int)ticks_x.size() && ticks_x[j].offs != 0; ++j) {
						float xm = x + ticks_x[j].offs * tick_step.x;

						if (ticks_x[j].size >= 1.0f)
							draw_axis_tick_text(view, xm, text_unit_scale_x, xm, 0, float2(0.5f, 0), 1, unit_str_x);

						float sz = tick_sz.y * ticks_x[j].size * 0.8f;
						line_vc += lines.draw_line(float3(xm, -sz, 0), float3(xm, +sz, 0), axes[0].col);
					}
				}
			}

			{ // Y
				int start = floori(y0/tick_step.y);
				int   end =  ceili(y1/tick_step.y);
				for (int i=start; i<=end; ++i) {
					float y = (float)i * tick_step.y;

					if (i!=0) {
						draw_axis_tick_text(view, y, text_unit_scale_y, 0, y, float2(1, 0.5f), 1, unit_str_y);

						line_vc += lines.draw_line(float3(-tick_sz.x, y, 0), float3(+tick_sz.x, y, 0), axes[1].col);
					}

					for (int j = 0; j < (int)ticks_y.size() && ticks_y[j].offs != 0; ++j) {
						float ym = y + ticks_y[j].offs * tick_step.y;

						if (ticks_y[j].size >= 1.0f)
							draw_axis_tick_text(view, ym, text_unit_scale_y, 0, ym, float2(1, 0.5f), 1, unit_str_y);

						float sz = tick_sz.x * ticks_y[j].size * 0.8f;
						line_vc += lines.draw_line(float3(-sz, ym, 0), float3(+sz, ym, 0), axes[1].col);
					}
				}
			}
		}

		r.text.draw_text(axes[0].display_name, axis_label_text_px, axes[0].col,
			map_text(float3(x1, 0, 0), view), float2(1,1), ticks_text_padding);
		r.text.draw_text(axes[1].display_name, axis_label_text_px, axes[1].col,
			map_text(float3(0, y1, 0), view), float2(0,0), ticks_text_padding);
	}

	void draw_equations (View3D const& view) {
		ZoneScoped;

		eq_res_px = max(eq_res_px, 1.0f / 8);

		float res = px2world.x * eq_res_px;

		int start = floori(x0 / res), end = ceili(x1 / res);

		Variables vars;
		auto eval = [&] (Equation& eq, float x, float* result) -> bool {
			vars.x = x;
			return eq.evaluate(vars, x, result);
		};

		bool dbg = ImGui::TreeNode("Debug Equations");

		eq_lines.resize(equations.equations.size());

		for (int eqi=0; eqi<(int)equations.equations.size(); ++eqi) {
			auto& eq = equations.equations[eqi];
			if (!eq.valid || !eq.enable) continue;

			eq_lines[eqi] = lines.begin_draw(eq.line_w);

			if (dbg) dbg_equation(eq);

			ZoneScopedN("draw equation");

			float prevt = (float)start * res;
			float prev;
			bool exec_valid = eval(eq, prevt, &prev);

			for (int i=start+1; exec_valid && i<=end; ++i) {
				float t = (float)i * res;
				float val;
				exec_valid = eval(eq, t, &val);

				if (axes[1].units->log)
					val = log10f(val);

				if (!isnan(prev) && !isnan(val))
					eq_lines[eqi].vertex_count += lines.draw_line(float3(prevt, prev, 0), float3(t, val, 0), eq.col);

				prevt = t;
				prev = val;
			}
		}

		if (dbg) ImGui::TreePop();
	}

	void render (Input& I, View3D const& view, int2 const& viewport_size) {
		ZoneScoped;

		r.begin(view, viewport_size);
		lines.begin();

		glClearColor(0.05f, 0.06f, 0.07f, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		precompute(view);

		draw_background_grid(view);
		draw_axes(view);
		draw_equations(view);

		//static float size = 20;
		//ImGui::SliderFloat("size", &size, 0, 70);
		//r.text.draw_text("Hello World!\n"
		//                 "f(x) = 5x^2 + 7x -9.3", map_text(float3(0,0,0), view), size);

		lines.upload_vertices();
		lines.render(r.state, axis_lines);
		lines.render(r.state, eq_lines.data(), (int)eq_lines.size());

		r.text.render(r.state);
	}

	virtual void frame (Input& I) {
		ZoneScoped;
		imgui(I);

		View3D view;
		if (!mode_3d) {
			view = cam.update(I, (float2)I.window_size);
			adjust_matrices(view);
		} else {
			view = flycam.update(I, (float2)I.window_size);
		}	
		render(I, view, I.window_size);
	}
};

inline IApp* make_app () { return new App(); }
int main () {
	return run_window(make_app, "Grapher");
}
