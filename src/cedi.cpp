
#define _USING_V110_SDK71_ 1
#include "windows.h"

#undef min
#undef max

#include <cstdio>

#include "lang_helpers.hpp"
#include "math.hpp"
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
	
	f32		min_cursor_w_percent_of_char =	1 ? 0.25f : 1;
	f32		min_cursor_w_px =				4;
	
	f32		tex_buffer_margin =				4;
};

static Options opt;

static void init ();
static void frame ();

#include "glfw_engine.hpp"
#include "font.hpp"

//
static font::Font			g_font; // one font for everything for now

struct Text_Buffer { // A buffer (think file) that the editor can display, it contains lines of text
	struct Line {
		Line*	next;
		Line*	prev;
		
		std::vector<utf32>	text; // can contain U'\0' since we want to be able to handle files with null termintors in them
		std::vector<v2>		chars_pos_px; // seperate y pos for each char to support line wrap (one line in the file gets visually split into multiple lines if it would cross outside the buffer window)
	};
	
	Line*	first_line;
	
	struct Cursor {
		Line*	lp; // line the cursor is in (use this for logic)
		s32		l; // mainly use this var for debggung and diplaying to the user
		
		s32		c; // char index the cursor is on (cursor appears on the left edge of the char it's on)
		
		//bool	is_horiz_sticking;
		//f32		stick_c;
	};
	
	Cursor	cursor;
	Cursor	select_cursor; // select_cursor.lp == nullptr -> not in selecting state
	
	std::vector<VBO_Text::V> vbo_char_vert_data;
	
	void _dbg_init_from_str (utf32 const* str, u32 len) {
		
		first_line = new Line;
		first_line->prev = nullptr;
		
		Line* cur_line = first_line;
		
		auto* in = str;
		while (in != (str +len)) {
			utf32 c = *in++;
			
			cur_line->text.push_back( c );
			
			if (c == U'\n' || c == U'\r') {
				if ((*in == U'\n' || *in == U'\r') && *in != c) {
					cur_line->text.push_back( *in++ );
				}
				
				Line* next_line = new Line;
				
				cur_line->next = next_line;
				next_line->prev = cur_line;
				
				cur_line = next_line;
			}
		}
		
		cur_line->next = nullptr; // cur_line == last line
		
		cursor.lp = first_line;
	}
	
	struct _Cursor_Rect {
		v2 pos;
		v2 dim;
	};
	
	_Cursor_Rect cursor_rect;
	
	void gen_chars_pos_and_vbo_data () {
		
		vbo_char_vert_data.clear();
		
		f32	pos_x_px;
		f32	pos_y_px = g_font.ascent_plus_gap +opt.tex_buffer_margin;
		
		auto* cur_line = first_line;
		u32	line_i=0;
		do {
			pos_x_px = g_font.border_left +opt.tex_buffer_margin;
			
			auto emit_glyph = [&] (utf32 c, v3 col) {
				pos_x_px = g_font.emit_glyph(&vbo_char_vert_data, pos_x_px,pos_y_px, c, v4(col,1));
			};
			
			{ // emit line numbers
				emit_glyph(U'0' +(line_i / 10) % 10, opt.col_line_numbers);
				emit_glyph(U'0' +(line_i /  1) % 10, opt.col_line_numbers);
				emit_glyph(U'|', opt.col_line_numbers_bar);
			}
			
			cur_line->chars_pos_px.clear();
			
			u32 tab_char_i=0;
			
			auto draw_escaped_char = [&] (utf32 c) {
				auto tmp = pos_x_px;
				emit_glyph(U'\\', opt.col_draw_whitespace);
				pos_x_px = lerp(tmp, pos_x_px, 0.6f); // squash \ and c closer together to make it seem like 1 glyph
				
				emit_glyph(c, opt.col_draw_whitespace);
				++tab_char_i;
			};
			
			for (s32 char_i=0; char_i<cur_line->text.size(); ++char_i) {
				utf32 c = cur_line->text[ char_i ];
				
				cur_line->chars_pos_px.push_back( v2(pos_x_px, pos_y_px) );
				
				switch (c) {
					case U'\t': {
						s32 spaces_needed = opt.tab_spaces -(tab_char_i % opt.tab_spaces);
						
						for (s32 j=0; j<spaces_needed; ++j) {
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
			
			cur_line->chars_pos_px.push_back( v2(pos_x_px, pos_y_px) ); // push char pos for imaginary last character, to be able to determine width of last char on line
			
			//
			cur_line = cur_line->next;
			
			pos_y_px += g_font.line_height;
			
			++line_i;
		} while (cur_line);
		
		v2 pos = cursor.lp->chars_pos_px[ cursor.c ];
		
		f32 w = 0;
		if (cursor.c < (cursor.lp->chars_pos_px.size() -1)) {
			w = cursor.lp->chars_pos_px[ cursor.c +1 ].x -pos.x; // could be imaginary last character
		}
		// w might end up zero because either the final few chars on the line are invisible (newline because draw_whitespace is off) or is not a character (end of file)
		w *= opt.min_cursor_w_percent_of_char;
		w = max(w, opt.min_cursor_w_px);
		
		cursor_rect = {	v2(pos.x -g_font.border_left, pos.y -g_font.line_height +g_font.descent_plus_gap),
						v2(w, g_font.line_height) };
	}
	
	static s32 max_cursor_pos_on_line (Line const* l) {
		// on each line except the last there always has to be at least a newline char
		// on last line there is no newline char -> we can place the cursor on the imaginary last character
		s32 ret = l->text.size();
		if (l->next) {
			dbg_assert(ret > 0);
			ret -= 1;
		}
		return ret; 
	}
	
	void move_cursor_left () {
		if (cursor.c > 0) {
			--cursor.c;
		} else {
			if (cursor.lp->prev) {
				dbg_assert(cursor.l > 0);
				--cursor.l;
				cursor.lp = cursor.lp->prev;
				cursor.c = max_cursor_pos_on_line(cursor.lp);
			}
		}
	}
	void move_cursor_right () {
		if (cursor.c < max_cursor_pos_on_line(cursor.lp)) {
			++cursor.c;
		} else {
			if (cursor.lp->next) {
				++cursor.l;
				cursor.lp = cursor.lp->next;
				cursor.c = 0;
			}
		}
	}
	
	static s32 nearest_char_to_cur_char_x (Line* dst, f32 x) {
		dbg_assert(dst->chars_pos_px.size() >= 1);
		
		f32 nearest_dist = +INF;
		s32 nearest_i;
		
		s32 i=0;
		for (; i<dst->chars_pos_px.size(); ++i) {
			f32 dist = abs(dst->chars_pos_px[i].x -x);
			if (dist < nearest_dist) {
				nearest_dist = dist;
				nearest_i = i;
			}
		}
		
		return nearest_i;
	}
	
	void move_cursor_up () {
		Line* cur_line = cursor.lp;
		Line* dst_line = cursor.lp->prev;
		if (dst_line) {
			--cursor.l;
			cursor.c = min( max_cursor_pos_on_line(dst_line),
					nearest_char_to_cur_char_x(dst_line, cur_line->chars_pos_px[cursor.c].x) );
			cursor.lp = dst_line;
		}
	}
	void move_cursor_down () {
		Line* cur_line = cursor.lp;
		Line* dst_line = cursor.lp->next;
		if (dst_line) {
			++cursor.l;
			cursor.c = min( max_cursor_pos_on_line(dst_line),
					nearest_char_to_cur_char_x(dst_line, cur_line->chars_pos_px[cursor.c].x) );
			cursor.lp = dst_line;
		}
	}
};

Text_Buffer g_buf; // init to zero/null

static void move_cursor_left () {	g_buf.move_cursor_left();	}
static void move_cursor_right () {	g_buf.move_cursor_right();	}
static void move_cursor_up () {		g_buf.move_cursor_up();		}
static void move_cursor_down () {	g_buf.move_cursor_down();	}

#define TEST U"test Tst\n123\nかきくけこ　ゴゴごご\nあべし\nABeShi\n"
#define C_PROG \
	U"\n" \
	U"#include <stdio.h>\n" \
	U"\n" \
	U"int main (int argc, char** argv) {\n" \
	U"	\n" \
	U"	printf(\"Hello World!\\n\");\n" \
	U"	\n" \
	U"	return 0;\n" \
	U"}\n"
#define C_PROG_2 \
	U"\n" \
	U"#include <stdio.h>\r\n" \
	U"int main (int argc, char** argv) {\n" \
	U"\0	printf(\"Hello World!\\n\");\r\n" \
	U" 	return 0;\n" \
	U"}\r" \
	U"\0blah"
#define TEST2 U"static void insert_char (utf32 c) {\n"

static constexpr cstr		APP_NAME =		u8"cedi";

static f32					running_avg_fps;

static std::basic_string<utf8>		wnd_title;

static Shader_Text					shad_text;
static Shader_Fullscreen_Tex_Copy	shad_text_copy;
static Shader_Cursor_Pass			shad_cursor_pass;

static VBO_Cursor_Pass		vbo_cursor;

typedef VBO_Cursor_Pass::V Vertex;

static RGBA_Framebuffer		fb_text;

static void init  () {
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
	
	running_avg_fps = 60; // assume 60 fps initially
	
	g_font.init("consola.ttf");
	
	shad_text			.init();
	shad_text_copy		.init();
	shad_cursor_pass	.init();
	
	vbo_cursor			.init();
	
	fb_text				.init();
	
	{
		utf32 str[] = C_PROG;
		g_buf._dbg_init_from_str( str, arrlen(str) -1 );
	}
	
	g_buf.cursor.l = 3;
	g_buf.cursor.lp = g_buf.first_line->next->next->next;
}

static void frame () {
	
	if (frame_indx != 0) {
		f32 alpha = 0.025f;
		running_avg_fps = running_avg_fps*(1.0f -alpha) +(1.0f/dt)*alpha;
	}
	{
		prints(&wnd_title, "%s  ~%.1f fps  %.3f ms", APP_NAME, running_avg_fps, dt*1000);
		glfwSetWindowTitle(wnd, wnd_title.c_str());
	}
	
	{
		g_buf.gen_chars_pos_and_vbo_data();
		
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
				auto& r = g_buf.cursor_rect;
				
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
		
	}
	
}
