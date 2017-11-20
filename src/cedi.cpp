
#define _USING_V110_SDK71_ 1
#include "windows.h"

#undef min
#undef max

#include <cstdio>

#include "lang_helpers.hpp"
#include "math.hpp"

#if RZ_COMP == RZ_COMP_GCC
	// gcc compiler bug with constexpr:
	// internal compiler error: in cxx_eval_constant_expression, at cp/constexpr.c:3503
	//  static Options opt;
	#define constexpr 
#else
	
#endif

#include "vector/vector.hpp"

typedef s32v2	iv2;
typedef s32v3	iv3;
typedef s32v4	iv4;
typedef fv2		v2;
typedef fv3		v3;
typedef fv4		v4;
typedef fm2		m2;
typedef fm3		m3;
typedef fm4		m4;

#include "gl.hpp"
#include "util.hpp"

struct Options {
	v3		col_background =				srgb(41,49,52);
	v3		col_text =						srgb(244,246,248);
	v3		col_text_highlighted =			srgb(20);
	v3		col_draw_whitespace =			col_text * 0.2f;
	v3		col_line_numbers =				col_text * 0.4f;
	v3		col_line_numbers_bar =			col_text * 0.2f;
	v3		col_cursor =					srgb(147,199,99);
	
	bool	draw_whitespace =				true;
	
	s32		tab_spaces =					4;
	
	f32		min_cursor_w_percent_of_char =	0 ? 0.25f : 1;
	f32		min_cursor_w_px =				6;
	
	f32		tex_buffer_margin =				4;
	
	f32		overscroll_fraction =			0.4f;
};

static Options opt;

static iv2 wnd_dim; // dimensions of windows window in px

static f64 t_draw_end =		QNANd; // DBG: invalid values for debugging
static f32 dt =				QNAN; // DBG: invalid values for debugging

static f32 avg_dt =			1.0f/60; // to get reasonable dt for first frame of smooth scrolling

static bool continuous_drawing = false;
static void set_continuous_drawing (bool state);

#include "font.hpp"

//
static font::Font			g_font; // one font for everything for now

typedef s64 buf_indx_t;

struct Text_Buffer { // A buffer (think file) that the editor can display, it contains lines of text
	
	typedef buf_indx_t indx_t;
	
	struct Line {
		std::vector<utf32>	text; // can contain U'\0' since we want to be able to handle files with null termintors in them
		
		f32		pos_y;
		std::vector<f32>	chars_x_px;
		
		u32 _count_newlines () {
			auto len = text.size();
			if (len == 0) return 0;
			
			utf32 a = text[len -1];
			if (a != '\n' && a != '\r') return 0;
			
			if (len == 1) return 1;
			
			utf32 b = text[len -2];
			if (b != '\n' && b != '\r') return 1;
			
			dbg_assert(a != b);
			return 2;
		}
		indx_t get_newlineless_len () {
			u32 newline_chars = _count_newlines();
			return text.size() -newline_chars;
		}
		
		indx_t get_max_cursor_c () {
			u32 newline_chars = _count_newlines();
			
			indx_t max_c = (indx_t)text.size();
			if (opt.draw_whitespace) {
				max_c -= newline_chars ? 1 : 0; // max cursor c is on the last newline char (or beyon the last character of the last line which does not have a newline char)
			} else {
				max_c -= newline_chars; // max cursor c is on the first newline char
			}
			
			return max_c;
		}
	};
	
	std::vector<Line> lines;
	
	struct Cursor {
		indx_t	l;
		indx_t	c; // char index the cursor is on (cursor appears on the left edge of the char it's on)
	};
	
	Cursor		cursor;
	Cursor		select_cursor;
	
	iv2			sub_wnd_dim;
	
	// scrolling
	indx_t		scroll;	// index of first line visible in text buffer window (from the top) (can overscroll, then this will be negative)
	
	// when smooth scrolling is enabled 'scroll' only defines the target to scroll to, 'smooth_scroll' is the actual scroll position
	f32			smooth_scroll;
	
	bool smooth_scroll_update () {
		if ( abs((f32)scroll -smooth_scroll) < 0.01f ) {
			// already at destination
			smooth_scroll = (f32)scroll;
			set_continuous_drawing(false);
			return false;
		}
		
		bool started_smooth_scrolling = !continuous_drawing;
		if (started_smooth_scrolling) {
			dt = avg_dt; // reasonable dt for first frame of smooth scrolling, since actual dt is based on frames before continuous_drawing
		}
		set_continuous_drawing(true);
		
		#if 0
		f32 x0 = 5; // constant velocity
		f32 x1 = 1.0f; // velocity proportional to distance
		
		f32 error = ((f32)scroll -smooth_scroll);
		f32 vel = error * x1;
		vel += error > 0 ? +x0 : -x0;
		
		smooth_scroll += vel * dt;
		
		printf(">>> p %f v %f   dt %f ms\n", smooth_scroll, vel, dt * 1000);
		#else
		smooth_scroll = lerp(smooth_scroll, (f32)scroll, 0.4f);
		#endif
		
		avg_dt = lerp(avg_dt, dt, 0.1f);
		return started_smooth_scrolling;
	}
	
	indx_t get_max_visible_lines_count () {
		indx_t count = (indx_t)ceil( (f32)sub_wnd_dim.y / (f32)g_font.line_height );
		count = max(count, (indx_t)1);
		return count;
	}
	struct Line_Range {
		indx_t first, count;
	};
	Line_Range get_visible_line_range () {
		indx_t first = clamp(scroll, (indx_t)0, (indx_t)(lines.size() -1));
		
		indx_t count = get_max_visible_lines_count();
		if (scroll < 0) {
			count += scroll;
		} else if ((first +count) >= (indx_t)lines.size()) {
			count -= (first +count) -(indx_t)lines.size();
		}
		count = max(count, (indx_t)1);
		
		return {first, count};
	}
	
	//
	void move_cursor_left () {
		if (cursor.c > 0) {
			--cursor.c;
		} else {
			if (cursor.l > 0) {
				--cursor.l;
				cursor.c = lines[cursor.l].get_max_cursor_c();
			}
		}
		
		constrain_scroll_to_cursor();
	}
	void move_cursor_right () {
		if (cursor.c < lines[cursor.l].get_max_cursor_c()) {
			++cursor.c;
		} else {
			if (cursor.l < (indx_t)(lines.size() -1)) {
				++cursor.l;
				cursor.c = 0;
			}
		}
		
		constrain_scroll_to_cursor();
	}
	
	void move_cursor_up () {
		if (cursor.l > 0) {
			--cursor.l;
			cursor.c = min(lines[cursor.l].get_max_cursor_c(), cursor.c);
		}
		
		constrain_scroll_to_cursor();
	}
	void move_cursor_down () {
		if (cursor.l < (indx_t)(lines.size() -1)) {
			++cursor.l;
			cursor.c = min(lines[cursor.l].get_max_cursor_c(), cursor.c);
		}
		
		constrain_scroll_to_cursor();
	}
	
	void insert_char (utf32 c) {
		lines[cursor.l].text.insert(lines[cursor.l].text.begin() +cursor.c, c);
		++cursor.c;
		
		constrain_scroll_to_cursor();
	}
	void insert_tab () {
		insert_char(U'\t');
	}
	void insert_enter () {
		// insert line after current line
		auto& new_ = *lines.insert( lines.begin() +cursor.l +1, Line() );
		auto& cur = lines[cursor.l];
		
		{ // Move chars after cursor to new line
			auto b = cur.text.begin() +cursor.c;
			auto e = cur.text.end();
			
			new_.text.insert(new_.text.begin(), b, e); // paste text after cursor into new line
			cur.text.erase(b, e); // delete text after cursor from current line
		}
		// terminte current line with newline
		cur.text.push_back(U'\n');
		// move cursor to beginning of new line
		++cursor.l;
		cursor.c = 0;
		
		constrain_scroll_to_cursor();
	}
	
	void newline_delete_merge_lines (indx_t newline_l) {
		// marge two lines by deleting newline
		dbg_assert(newline_l < (indx_t)(lines.size() -1)); // cant merge last line with nothing
		
		auto& newl = lines[newline_l];
		auto& next = lines[newline_l +1];
		
		// delete newline-line newline char
		newl.text.erase(	newl.text.begin() +newl.get_newlineless_len(),
							newl.text.begin() +newl.text.size());
		
		// move cursor to end of newline-line (the place where we deleted the newline char)
		if (cursor.l != newline_l) --cursor.l;
		cursor.c = (indx_t)newl.text.size();
		
		// Merge line text
		newl.text.insert(newl.text.end(), next.text.begin(), next.text.end());
		
		lines.erase( lines.begin() +(newline_l +1), lines.begin() +(newline_l +2) );
	}
	
	void delete_prev () {
		if (cursor.c > 0) {
			lines[cursor.l].text.erase( lines[cursor.l].text.begin() +(cursor.c -1) );
			--cursor.c;
		} else {
			if (cursor.l > 0) {
				// marge previous line with current line
				newline_delete_merge_lines(cursor.l -1);
			}
		}
		
		constrain_scroll_to_cursor();
	}
	void delete_next () {
		if (cursor.c < lines[cursor.l].get_newlineless_len()) {
			lines[cursor.l].text.erase( lines[cursor.l].text.begin() +cursor.c );
		} else {
			if (cursor.l < (indx_t)(lines.size() -1)) {
				// marge current line with next line
				newline_delete_merge_lines(cursor.l);
			}
		}
		
		constrain_scroll_to_cursor();
	}
	
	void open_file (cstr filename) {
		std::vector<byte> tmp;
		if (load_file_skip_bom(filename, &tmp, UTF8_BOM, arrlen(UTF8_BOM))) {
			init_from_str((utf8*)&tmp[0], tmp.size());
			printf("done.\n");
		} else {
			printf("Could not open file '%s'!\n", filename);
		}
	}
	
	void constrain_scroll_to_overscroll_setting () {
		auto count = get_max_visible_lines_count();
		
		indx_t ov = (indx_t)( lerp(count -1, (indx_t)1, opt.overscroll_fraction) +0.5f );
		
		scroll = clamp(scroll, 0 -max(count -1 -ov, (indx_t)0), (indx_t)(lines.size() -ov));
	}
	void constrain_scroll_to_buf () {
		indx_t ov = 1;
		
		auto count = get_max_visible_lines_count();
		scroll = clamp(scroll, 0 -max(count -1 -ov, (indx_t)0), (indx_t)(lines.size() -ov));
	}
	void constrain_scroll_to_cursor () {
		auto count = get_max_visible_lines_count();
		scroll = clamp(scroll, cursor.l -max(count -2, (indx_t)0), cursor.l);
	}
	void scroll_page_up () {
		scroll -= max(get_max_visible_lines_count() -2, (indx_t)1);
		
		constrain_scroll_to_overscroll_setting();
	}
	void scroll_page_down () {
		scroll += max(get_max_visible_lines_count() -2, (indx_t)1);
		
		constrain_scroll_to_overscroll_setting();
	}
	void mouse_scroll (s32 diff) {
		scroll -= diff;
		constrain_scroll_to_buf();
	}
	
	void resize_sub_wnd (iv2 dim) {
		sub_wnd_dim = dim;
		constrain_scroll_to_cursor();
	}
	
	//
	std::vector<VBO_Text::V> vbo_char_vert_data;
	
	void init () {
		cursor.l = 0;
		cursor.c = 0;
		
		scroll = 0;
		smooth_scroll = 0;
		
		//open_file("src/cedi.cpp");
		open_file("build.bat");
	}
	void init_from_str (utf8 const* str, u64 len) {
		
		lines.clear();
		lines.push_back({});
		
		auto* in = str;
		while (in != (str +len)) {
			utf32 c = utf8_to_utf32(&in);
			
			if (c == U'\n' && lines.size() % 2) lines.back().text.push_back( U'\r' );
			
			lines.back().text.push_back( c );
			
			if (c == U'\n' || c == U'\r') {
				if (in != (str +len) && (*in == '\n' || *in == '\r') && (utf32)*in != c) {
					lines.back().text.push_back( utf8_to_utf32(&in) );
				}
				
				lines.push_back({});
			}
		}
		
		cursor.l = 0;
		cursor.c = 0;
		
		scroll = 0;
		smooth_scroll = 0;
		
	}
	
	struct Cursor_Rect {
		v2 pos;
		v2 dim;
	};
	
	Cursor_Rect generate_layout () {
		
		vbo_char_vert_data.clear();
		
		auto vis_lines = get_visible_line_range();
		
		f32	pos_x_px;
		//f32	pos_y_px = g_font.ascent_plus_gap +opt.tex_buffer_margin +(f32)((s64)g_font.line_height * -scroll);
		f32	pos_y_px = g_font.ascent_plus_gap +opt.tex_buffer_margin +((f32)g_font.line_height * -smooth_scroll);
		
		for (indx_t line_i=0; line_i<(indx_t)lines.size(); ++line_i) {
			auto& l = lines[line_i];
			
			pos_x_px = g_font.border_left +opt.tex_buffer_margin;
			
			auto emit_glyph = [&] (utf32 c, v3 col) {
				
				if (line_i == vis_lines.first)
					col *= v3(1,0,0);
				if (line_i == (vis_lines.first +vis_lines.count -1))
					col *= v3(0,0,1);
				
				pos_x_px = g_font.emit_glyph(&vbo_char_vert_data, pos_x_px,pos_y_px, c, v4(col,1));
			};
			
			{ // emit line numbers
				u32 digit_count = 0; // max needed digits to diplay line numbers
				{
					dbg_assert(lines.size() > 0);
					indx_t num = (indx_t)lines.size() -1; // max needed number to diplay line numbers
					while (num != 0) {
						num /= 10;
						++digit_count;
					}
					digit_count = max(digit_count, (u32)1);
				}
				
				u32 num = line_i;
				utf32 buf[32];
				u32 num_len = 0;
				for (; num_len<digit_count; ++num_len) {
					if (num_len > 0 && num == 0) break;
					buf[num_len] = num % 10;
					num /= 10;
				}
				num_len = max(num_len, (u32)1);
				
				for (u32 i=digit_count; i!=0;) { --i;
					emit_glyph(i < num_len ? U'0' +buf[i] : U' ', opt.col_line_numbers);
				}
				emit_glyph(U'|', opt.col_line_numbers_bar);
			}
			
			l.pos_y = pos_y_px;
			l.chars_x_px.clear();
			
			indx_t tab_char_i=0;
			
			auto draw_escaped_char = [&] (utf32 c) {
				auto tmp = pos_x_px;
				emit_glyph(U'\\', opt.col_draw_whitespace);
				pos_x_px = lerp(tmp, pos_x_px, 0.6f); // squash \ and c closer together to make it seem like 1 glyph
				
				emit_glyph(c, opt.col_draw_whitespace);
				++tab_char_i;
			};
			
			for (indx_t char_i=0; char_i<(indx_t)l.text.size(); ++char_i) {
				utf32 c = l.text[ char_i ];
				
				l.chars_x_px.push_back(pos_x_px);
				
				switch (c) {
					case U'\t': {
						indx_t spaces_needed = opt.tab_spaces -(tab_char_i % opt.tab_spaces);
						
						for (indx_t j=0; j<spaces_needed; ++j) {
							auto c = U' ';
							if (opt.draw_whitespace) {
								c = j<spaces_needed-1 ? U'—' : U'→';
							}
							
							emit_glyph(c, opt.col_draw_whitespace);
							
							++tab_char_i;
						}
						
					} break;
					
					case U'\n': {
						if (opt.draw_whitespace) draw_escaped_char(U'n');
					} break;
					case U'\r': {
						if (opt.draw_whitespace) draw_escaped_char(U'r');
					} break;
					case U'\0': {
						draw_escaped_char(U'0');
					} break;
					
					case U' ': {
						if (opt.draw_whitespace) {
							emit_glyph(U'·', opt.col_draw_whitespace);
						} else {
							emit_glyph(c, opt.col_text);
						}
						
						++tab_char_i;
					} break;
					
					default: {
						emit_glyph(c, opt.col_text);
						++tab_char_i;
					} break;
				}
			}
			
			l.chars_x_px.push_back(pos_x_px); // push char pos for imaginary last character, to be able to determine width of last char on line
			
			pos_y_px += g_font.line_height;
			
		}
		
		//
		f32 x = lines[cursor.l].chars_x_px[ cursor.c ];
		
		f32 w = 0;
		if (cursor.c < (indx_t)(lines[cursor.l].chars_x_px.size() -1)) {
			w = lines[cursor.l].chars_x_px[ cursor.c +1 ] -x; // could be imaginary last character
		}
		// w might end up zero because either the final few chars on the line are invisible (newline because draw_whitespace is off) or is not a character (end of file)
		w *= opt.min_cursor_w_percent_of_char;
		
		w = max(w, opt.min_cursor_w_px);
		
		return {	v2(x -g_font.border_left, lines[cursor.l].pos_y -g_font.line_height +g_font.descent_plus_gap),
					v2(w, g_font.line_height) };
	}
	
};

Text_Buffer g_buf; // init to zero/null

// input events
static void move_cursor_left () {		g_buf.move_cursor_left();	}
static void move_cursor_right () {		g_buf.move_cursor_right();	}
static void move_cursor_up () {			g_buf.move_cursor_up();		}
static void move_cursor_down () {		g_buf.move_cursor_down();	}
static void scroll_page_up () {			g_buf.scroll_page_up();		}
static void scroll_page_down () {		g_buf.scroll_page_down();	}
static void mouse_scroll (s32 diff) {	g_buf.mouse_scroll(diff);	}

static void insert_char (utf32 c) {		g_buf.insert_char(c);		}
static void insert_tab () {				g_buf.insert_tab();			}
static void insert_enter () {			g_buf.insert_enter();		}
static void delete_prev () {			g_buf.delete_prev();		}
static void delete_next () {			g_buf.delete_next();		}

static void open_file (cstr filename) {	g_buf.open_file(filename);	}

static void resize_wnd (iv2 dim) {
	wnd_dim = dim;
	g_buf.resize_sub_wnd(dim);
}

static void draw (cstr reason);
static void init ();

#include "glfw_engine.hpp"


////
static Shader_Text					shad_text;
static Shader_Fullscreen_Tex_Copy	shad_text_copy;
static Shader_Cursor_Pass			shad_cursor_pass;

static VBO_Cursor_Pass		vbo_cursor;

typedef VBO_Cursor_Pass::V Vertex;

static RGBA_Framebuffer		fb_text;

static void init  () {
	g_font.init("consola.ttf");
	
	shad_text			.init();
	shad_text_copy		.init();
	shad_cursor_pass	.init();
	
	vbo_cursor			.init();
	
	fb_text				.init();
	
	g_buf.init();
	
	{ // show window
		auto mr = get_monitor_rect();
		#if 0
		s32 border_top =		31;			// window title
		s32 border_bottom =		8 +29;		// window bottom border +taskbar
		s32 border_right =		8;			// window right border
		s32 h = mr.dim.y -border_top -border_bottom;
		Rect r = {mr.pos +iv2(mr.dim.x,border_top) -iv2(h +border_right, 0), h};
		#else
		s32 border_top =		31;			// window title
		s32 border_bottom =		8 +29;		// window bottom border +taskbar
		s32 border_left =		8;			// window right border
		s32 h = mr.dim.y -border_top -border_bottom;
		Rect r = {mr.pos +iv2(border_left,border_top), h};
		#endif
		init_show_window(false, r);
		
		glfwSetWindowTitle(wnd, u8"cedi");
	}
	
	draw("init()");
}

static void draw (cstr reason) { // DBG: reason we drew a new frame
	
	f64 t_draw_start = glfwGetTime();
	
	printf("draw [%14s] dt %.1f ms\n", reason, dt * 1000);
	
	bool started_smooth_scrolling = g_buf.smooth_scroll_update();
	
	auto cursor_rect = g_buf.generate_layout();
	
	{ // text pass
		fb_text.bind_and_clear(wnd_dim, v4(0));
		
		shad_text.bind();
		shad_text.wnd_dim.set( (v2)wnd_dim );
		shad_text.bind_texture(g_font.tex);
		
		g_font.draw_emitted_glyphs(shad_text, &g_buf.vbo_char_vert_data);
	}
	
	{ // draw cursor
		bind_backbuffer(wnd_dim);
		clear_framebuffer(v4(opt.col_background,0));
		
		//
		shad_text_copy.bind();
		shad_text_copy.bind_fb(fb_text);
		shad_text_copy.wnd_dim.set( (v2)wnd_dim );
		
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		
		glDrawArrays(GL_TRIANGLES, 0, 6);
		
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		//
		shad_cursor_pass.bind();
		shad_cursor_pass.bind_fb(fb_text);
		shad_cursor_pass.wnd_dim.set( (v2)wnd_dim );
		shad_cursor_pass.col_background.set( opt.col_background );
		shad_cursor_pass.col_highlighted.set( opt.col_text_highlighted );
		
		{
			auto& r = cursor_rect;
			
			std::initializer_list<Vertex> data = {
				{ r.pos +r.dim * v2(1,0), v4(opt.col_cursor,1) },
				{ r.pos +r.dim * v2(1,1), v4(opt.col_cursor,1) },
				{ r.pos +r.dim * v2(0,0), v4(opt.col_cursor,1) },
				{ r.pos +r.dim * v2(0,0), v4(opt.col_cursor,1) },
				{ r.pos +r.dim * v2(1,1), v4(opt.col_cursor,1) },
				{ r.pos +r.dim * v2(0,1), v4(opt.col_cursor,1) },
			};
			
			vbo_cursor.upload(data);
			vbo_cursor.bind(shad_cursor_pass);
			
			glDrawArrays(GL_TRIANGLES, 0, data.size());
		}
	}
	
	platform_present_frame();
	
	{
		f64 now = glfwGetTime();
		if (started_smooth_scrolling) {
			dt = now -t_draw_start; // use this for 2nd frame of smooth scrolling, since old t_draw_end was before continuous_drawing
		} else {
			dt = now -t_draw_end;
		}
		//printf(">>>>>> now %f t %f -> dt %f ms\n", now, t, dt * 1000);
		t_draw_end = now;
	}
}
