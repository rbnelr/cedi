
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
	v3		col_text =						1;
	v3		col_draw_whitespace =			col_text * 0.1f;
	v3		col_line_numbers =				col_text * 0.25f;
	v3		col_line_numbers_bar =			col_text * 0.1f;
	v3		col_cursor =					srgb(147,199,99);
	
	bool	draw_whitespace =				true;
	
	s32		tab_spaces =					4;
	
	f32		min_cursor_w_percent_of_char =	0 ? 0.25f : 1;
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
	
	std::vector<VBO_Pos_Tex_Col::V> vbo_char_vert_data;
	
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
			
			u32 char_i=0;
			
			auto draw_escaped_char = [&] (utf32 c) {
				if (opt.draw_whitespace) {
					auto tmp = pos_x_px;
					emit_glyph(U'\\', opt.col_draw_whitespace);
					pos_x_px = lerp(tmp, pos_x_px, 0.6f); // squash \ and c closer together to make it seem like 1 glyph
					
					emit_glyph(c, opt.col_draw_whitespace);
					++char_i;
				}
			};
			
			for (utf32 c : cur_line->text) {
				
				cur_line->chars_pos_px.push_back( v2(pos_x_px, pos_y_px) );
				
				switch (c) {
					case U'\t': {
						s32 spaces_needed = opt.tab_spaces -(char_i % opt.tab_spaces);
						
						for (s32 j=0; j<spaces_needed; ++j) {
							auto c = U' ';
							if (opt.draw_whitespace) {
								c = j<spaces_needed-1 ? U'—' : U'→';
							}
							
							emit_glyph(c, opt.col_draw_whitespace);
							
							++char_i;
						}
						
					} break;
					
					case U'\n': {
						draw_escaped_char(U'n');
					} break;
					case U'\r': {
						draw_escaped_char(U'r');
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
						
						++char_i;
					} break;
					
					default: {
						emit_glyph(c, opt.col_text);
						++char_i;
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
	
	//static s32 nearest_char_to_cur_char_x (Line* dst, Line*) {
	//	
	//}
	//cursor.c = nearest_char_to_cur_char_x(cursor.lp->prev, cursor.lp, cursor.c);
	//cursor.lp = cursor.lp->prev;
	
	void move_cursor_up () {
		if (cursor.lp->prev) {
			--cursor.l;
			cursor.lp = cursor.lp->prev;
			cursor.c = min(cursor.c, max_cursor_pos_on_line(cursor.lp));
		}
	}
	void move_cursor_down () {
		if (cursor.lp->next) {
			++cursor.l;
			cursor.lp = cursor.lp->next;
			cursor.c = min(cursor.c, max_cursor_pos_on_line(cursor.lp));
		}
	}
};

Text_Buffer g_buf; // init to zero/null

static void move_cursor_left () {	g_buf.move_cursor_left();	}
static void move_cursor_right () {	g_buf.move_cursor_right();	}
static void move_cursor_up () {		g_buf.move_cursor_up();		}
static void move_cursor_down () {	g_buf.move_cursor_down();	}

#define TEST U"test Tst\n123\nかきくけこ　ゴゴごご\nあべし\nABeShi\n"
#define C_PROG_ \
	U"\n" \
	U"#include <stdio.h>\n" \
	U"\n" \
	U"int main (int argc, char** argv) {\n" \
	U"	\n" \
	U"	printf(\"Hello World!\\n\");\n" \
	U"	\n" \
	U"	return 0;\n" \
	U"}\n"
#define C_PROG \
	U"\n" \
	U"#include <stdio.h>\r\n" \
	U"int main (int argc, char** argv) {\n" \
	U"\0	printf(\"Hello World!\\n\");\r\n" \
	U" 	return 0;\n" \
	U"}\r"
#define TEST2 U"static void insert_char (utf32 c) {\n"

static constexpr cstr		APP_NAME =		u8"cedi";

static f32					running_avg_fps;

static std::basic_string<utf8>		wnd_title;

static Shader_Clip_Tex_Col	shad_tex;
static Shader_Px_Col		shad_px_col;

static VBO_Pos_Col			vbo_px_col;

typedef VBO_Pos_Col::V Vertex;

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
	
	shad_tex.init();
	shad_px_col.init();
	vbo_px_col.init();
	
	{
		utf32 str[] = C_PROG;
		g_buf._dbg_init_from_str( str, arrlen(str) );
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
	
	glViewport(0, 0, wnd_dim.x, wnd_dim.y);
	
	glClearColor(opt.col_background.x, opt.col_background.y, opt.col_background.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	
	{
		g_buf.gen_chars_pos_and_vbo_data();
		
		// draw cursor line
		shad_px_col.bind();
		shad_px_col.wnd_dim.set( (v2)wnd_dim );
		
		{
			auto& r = g_buf.cursor_rect;
			
			std::initializer_list<Vertex> data = {
				{ r.pos +r.dim * v2(1,0), v4(opt.col_cursor, 1) },
				{ r.pos +r.dim * v2(1,1), v4(opt.col_cursor, 1) },
				{ r.pos +r.dim * v2(0,0), v4(opt.col_cursor, 1) },
				{ r.pos +r.dim * v2(0,0), v4(opt.col_cursor, 1) },
				{ r.pos +r.dim * v2(1,1), v4(opt.col_cursor, 1) },
				{ r.pos +r.dim * v2(0,1), v4(opt.col_cursor, 1) },
			};
			
			vbo_px_col.upload(data);
			vbo_px_col.bind(shad_px_col);
			
			glDrawArrays(GL_TRIANGLES, 0, data.size());
		}
		
		// draw buffer text
		shad_tex.bind();
		shad_tex.wnd_dim.set( (v2)wnd_dim );
		shad_tex.bind_texture(g_font.tex);
		
		g_font.draw_emitted_glyphs(shad_tex, &g_buf.vbo_char_vert_data);
		
	}
	
}
