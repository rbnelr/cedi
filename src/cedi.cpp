
#define _USING_V110_SDK71_ 1
#include "windows.h"

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

#include "glad.c"
#include "GLFW/glfw3.h"

STATIC_ASSERT(sizeof(GLint) ==		sizeof(s32));
STATIC_ASSERT(sizeof(GLuint) ==		sizeof(u32));
STATIC_ASSERT(sizeof(GLsizei) ==	sizeof(u32));
STATIC_ASSERT(sizeof(GLsizeiptr) ==	sizeof(u64));

struct Rect {
	iv2 pos;
	iv2 dim;
};

//
static GLFWmonitor*	primary_monitor;
static GLFWwindow*	wnd;
static bool			fullscreen;
static Rect			_suggested_wnd_rect;
static Rect			_restore_wnd_rect;

static u32			frame_indx; // probably should only used for debug logic

static f64			t;
static f32			dt;

static iv2			wnd_dim;
static v2			wnd_dim_aspect;
static iv2			mcursor_pos;
static bool			cursor_in_wnd;
static s32			scrollwheel_diff;

//#include "buttons.hpp"

static void toggle_fullscreen ();

static std::basic_string<utf32> buffer = U"test Tst 123 あべし";
struct buf_cursor {
	u32	offs;
};
static buf_cursor cursor = { strlen(U"test Tst 123 あべし") -2 };

static void insert_char (utf32 c) {
	dbg_assert(cursor.offs <= buffer.size());
	buffer.insert( buffer.begin() +cursor.offs, (utf32)c );
	++cursor.offs;
}
static void delete_prev_char () { // backspace key
	if (cursor.offs != 0) {
		dbg_assert(cursor.offs <= buffer.size());
		buffer.erase( cursor.offs -1, 1 );
		--cursor.offs;
	}
}
static void delete_next_char () { // delete key
	if (cursor.offs != buffer.size()) {
		dbg_assert(cursor.offs < buffer.size());
		buffer.erase( cursor.offs, 1 );
	}
}

static void glfw_error_proc (int err, cstr msg) {
	fprintf(stderr, ANSI_COLOUR_CODE_RED "GLFW Error! 0x%x '%s'\n" ANSI_COLOUR_CODE_NC, err, msg);
}
static void glfw_scroll_proc (GLFWwindow* window, f64 xoffset, f64 yoffset) {
	scrollwheel_diff += (s32)floor(yoffset);
}
static void glfw_text_proc (GLFWwindow* window, ui codepoint) {
	insert_char(codepoint);
}
static void glfw_key_proc (GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_RELEASE) return;
	
	dbg_assert(action == GLFW_PRESS || action == GLFW_REPEAT);
	
	//cstr name = glfwGetKeyName(key, scancode);
	//printf("Button %s: %d\n", name ? name : "<null>", action);
	
	switch (key) {
		case GLFW_KEY_BACKSPACE: {
			delete_prev_char();
		} break;
		case GLFW_KEY_DELETE: {
			delete_next_char();
		} break;
		case GLFW_KEY_ENTER:
		case GLFW_KEY_KP_ENTER: {
			insert_char(U'\n');
		} break;
		
		case GLFW_KEY_F11: {
			toggle_fullscreen();
		} break;
		
		default: {
			// do nothing
		} break;
	}
}

static void setup_glfw () {
	glfwSetErrorCallback(glfw_error_proc);
	
	dbg_assert( glfwInit() );
	
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,	3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,	3);
	//glfwWindowHint(GLFW_OPENGL_PROFILE,			GLFW_OPENGL_CORE_PROFILE);
	
	primary_monitor = glfwGetPrimaryMonitor();
	
	glfwWindowHint(GLFW_VISIBLE,				0);
	wnd = glfwCreateWindow(1280, 720, u8"", NULL, NULL);
	dbg_assert(wnd);
	
	glfwGetWindowPos(wnd, &_suggested_wnd_rect.pos.x,&_suggested_wnd_rect.pos.y);
	glfwGetWindowSize(wnd, &_suggested_wnd_rect.dim.x,&_suggested_wnd_rect.dim.y);
	
	glfwSetKeyCallback(wnd, glfw_key_proc);
	glfwSetCharCallback(wnd, glfw_text_proc);
	glfwSetScrollCallback(wnd, glfw_scroll_proc);
	//glfwSetMouseButtonCallback(wnd, glfw_mousebutton_proc);
	
	glfwMakeContextCurrent(wnd);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	
}

static void toggle_fullscreen () {
	if (fullscreen) {
		glfwSetWindowMonitor(wnd, NULL, _restore_wnd_rect.pos.x,_restore_wnd_rect.pos.y,
				_restore_wnd_rect.dim.x,_restore_wnd_rect.dim.y, GLFW_DONT_CARE);
	} else {
		glfwGetWindowPos(wnd, &_restore_wnd_rect.pos.x,&_restore_wnd_rect.pos.y);
		glfwGetWindowSize(wnd, &_restore_wnd_rect.dim.x,&_restore_wnd_rect.dim.y);
		
		auto* r = glfwGetVideoMode(primary_monitor);
		glfwSetWindowMonitor(wnd, primary_monitor, 0,0, r->width,r->height, r->refreshRate);
	}
	fullscreen = !fullscreen;
	
	glfwSwapInterval(1); // seems like vsync needs to be set after switching to from the inital hidden window to a fullscreen one, or there will be no vsync
}
static void init_show_window (bool fullscreen, Rect rect=_suggested_wnd_rect) {
	::fullscreen = !fullscreen;
	
	_restore_wnd_rect = rect;
	
	toggle_fullscreen();
	
	glfwShowWindow(wnd);
}
static Rect get_monitor_rect () {
	auto* r = glfwGetVideoMode(primary_monitor);
	Rect ret;
	ret.dim = iv2(r->width,r->height);
	glfwGetMonitorPos(primary_monitor, &ret.pos.x,&ret.pos.y);
	return ret;
}

#if 0
#define GLSL_VERSION "#version 140\n"
#else
#define GLSL_VERSION "#version 330\n"
#endif

//
static GLuint vbo_init_and_upload_static (void* data, uptr data_size) {
	GLuint vbo;
	glGenBuffers(1, &vbo);
	
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, data_size, data, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	return vbo;
}
static bool shad_check_compile_status (GLuint shad) {
	GLint status;
	glGetShaderiv(shad, GL_COMPILE_STATUS, &status);
	bool error = status == GL_FALSE;
	
	s32 log_size; // including null terminator
	glGetShaderiv(shad, GL_INFO_LOG_LENGTH, &log_size);
	
	if (log_size > 1) { // 0=>no log  1=>empty log
		char* log_str = (char*)malloc(log_size);
		defer { free(log_str); };
		
		s32 written_len;
		glGetShaderInfoLog(shad, log_size, &written_len, log_str);
		dbg_assert(written_len == (log_size -1) && log_str[written_len] == '\0');
		
		if (error) {
			printf(	ANSI_COLOUR_CODE_RED
					"Shader compilation failed!\n"
					"  '%s'\n" ANSI_COLOUR_CODE_NC, log_str);
		} else {
			printf(	ANSI_COLOUR_CODE_YELLOW
					"Shader compilation info log:\n"
					"  '%s'\n" ANSI_COLOUR_CODE_NC, log_str);
		}
	}
	return !error;
}
static bool shad_check_link_status (GLuint prog) {
	GLint status;
	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	bool error = status == GL_FALSE;
	
	s32 log_size; // including null terminator
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_size);
	
	if (log_size > 1) { // 0=>no log  1=>empty log
		char* log_str = (char*)malloc(log_size);
		defer { free(log_str); };
		
		s32 written_len;
		glGetProgramInfoLog(prog, log_size, &written_len, log_str);
		dbg_assert(written_len == (log_size -1) && log_str[written_len] == '\0');
		
		if (error) {
			printf(	ANSI_COLOUR_CODE_RED
					"Shader linking failed!\n"
					"  '%s'\n" ANSI_COLOUR_CODE_NC, log_str);
		} else {
			printf(	ANSI_COLOUR_CODE_YELLOW
					"Shader linking info log:\n"
					"  '%s'\n" ANSI_COLOUR_CODE_NC, log_str);
		}
	}
	return !error;
}

struct Basic_Shader {
	GLuint		prog;
	
	cstr		vert_src;
	cstr		frag_src;
	
	Basic_Shader (cstr v, cstr f): vert_src{v}, frag_src{f} {}
	
	void compile () {
		prog = glCreateProgram();
		
		GLuint vert = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vert, 1, &vert_src, NULL);
		glCompileShader(vert);
		shad_check_compile_status(vert);
		glAttachShader(prog, vert);
		
		GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(frag, 1, &frag_src, NULL);
		glCompileShader(frag);
		shad_check_compile_status(frag);
		glAttachShader(prog, frag);
		
		glLinkProgram(prog);
		shad_check_link_status(prog);
		
		glDetachShader(prog, vert);
		glDeleteShader(vert);
		
		glDetachShader(prog, frag);
		glDeleteShader(frag);
	}
	void bind () {
		glUseProgram(prog);
	}
	
};

struct Unif_s32 {
	GLint loc;
	void set (s32 i) const {
		glUniform1i(loc, i);
	}
};
struct Unif_flt {
	GLint loc;
	void set (f32 f) const {
		glUniform1f(loc, f);
	}
};
struct Unif_fv2 {
	GLint loc;
	void set (fv2 v) const {
		glUniform2f(loc, v.x,v.y);
	}
};
struct Unif_fv3 {
	GLint loc;
	void set (fv3 cr v) const {
		glUniform3fv(loc, 1, &v.x);
	}
};
struct Unif_fm2 {
	GLint loc;
	void set (fm2 m) const {
		glUniformMatrix2fv(loc, 1, GL_FALSE, &m.arr[0][0]);
	}
};
struct Unif_fm4 {
	GLint loc;
	void set (fm4 m) const {
		glUniformMatrix4fv(loc, 1, GL_FALSE, &m.arr[0][0]);
	}
};

struct VBO_Pos_Col {
	GLuint	vbo;
	struct V {
		v2	pos;
		v4	col;
	};
	
	void init () {
		glGenBuffers(1, &vbo);
	}
	void upload (std::vector<V> data) {
		uptr data_size = data.size() * sizeof(V);
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, data_size, NULL, GL_STATIC_DRAW);
		glBufferData(GL_ARRAY_BUFFER, data_size, data.data(), GL_STATIC_DRAW);
		
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	void bind (Basic_Shader cr shad) {
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		
		GLint	pos =	glGetAttribLocation(shad.prog, "attrib_pos");
		GLint	col =	glGetAttribLocation(shad.prog, "attrib_col");
		
		dbg_assert(pos >= 0);
		dbg_assert(col >= 0);
		
		glEnableVertexAttribArray(pos);
		glVertexAttribPointer(pos,	2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V,pos));
		
		glEnableVertexAttribArray(col);
		glVertexAttribPointer(col,	4, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V,col));
		
	}
	
};
struct Shader_Px_Col : Basic_Shader {
	Shader_Px_Col (): Basic_Shader(
// Vertex shader
GLSL_VERSION R"_SHAD(
	in		vec2	attrib_pos; // px
	in		vec4	attrib_col;
	out		vec4	color;
	
	uniform vec2	wnd_dim;
	
	void main() {
		vec2 tmp = attrib_pos;
		tmp.y = wnd_dim.y -tmp.y;
		vec2 pos_clip = (tmp / wnd_dim) * 2 -1;
		
		gl_Position = vec4(pos_clip, 0.0, 1.0);
		color = attrib_col;
	}
)_SHAD",
// Fragment shader
GLSL_VERSION R"_SHAD(
	in		vec4	color;
	out		vec4	frag_col;
	
	void main() {
		frag_col = color;
	}
)_SHAD"
	) {}
	
	// uniforms
	Unif_fv2	wnd_dim;
	
	void init () {
		compile();
		
		wnd_dim.loc =		glGetUniformLocation(prog, "wnd_dim");
		dbg_assert(wnd_dim.loc != -1);
	}
};

//struct cursor_line  

struct VBO_Pos_Tex_Col {
	GLuint	vbo;
	struct V {
		v2	pos;
		v2	uv;
		v4	col;
	};
	
	void init () {
		glGenBuffers(1, &vbo);
	}
	void upload (std::vector<V> cr data) {
		uptr data_size = data.size() * sizeof(V);
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, data_size, NULL, GL_STATIC_DRAW);
		glBufferData(GL_ARRAY_BUFFER, data_size, data.data(), GL_STATIC_DRAW);
		
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	void bind (Basic_Shader cr shad) {
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		
		GLint	pos =	glGetAttribLocation(shad.prog, "attrib_pos");
		GLint	uv =	glGetAttribLocation(shad.prog, "attrib_uv");
		GLint	col =	glGetAttribLocation(shad.prog, "attrib_col");
		
		dbg_assert(pos >= 0);
		dbg_assert(uv >= 0);
		dbg_assert(col >= 0);
		
		glEnableVertexAttribArray(pos);
		glVertexAttribPointer(pos,	2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V,pos));
		
		glEnableVertexAttribArray(uv);
		glVertexAttribPointer(uv,	2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V,uv));
		
		glEnableVertexAttribArray(col);
		glVertexAttribPointer(col,	4, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V,col));
		
	}
	
};
#include "font.hpp"
struct Shader_Clip_Tex_Col : Basic_Shader {
	Shader_Clip_Tex_Col (): Basic_Shader(
// Vertex shader
GLSL_VERSION R"_SHAD(
	in		vec2	attrib_pos; // px
	in		vec2	attrib_uv;
	in		vec4	attrib_col;
	out		vec4	color;
	out		vec2	uv;
	
	uniform vec2	wnd_dim;
	
	void main() {
		vec2 tmp = attrib_pos;
		tmp.y = wnd_dim.y -tmp.y;
		vec2 pos_clip = (tmp / wnd_dim) * 2 -1;
		
		gl_Position =	vec4(pos_clip, 0.0, 1.0);
		uv =			attrib_uv;
		color =			attrib_col;
	}
)_SHAD",
// Fragment shader
GLSL_VERSION R"_SHAD(
	in		vec4	color;
	in		vec2	uv;
	uniform	sampler2D	tex;
	
	out		vec4	frag_col;
	
	void main() {
		frag_col = color * vec4(1,1,1, texture(tex, uv).r);
	}
)_SHAD"
	) {}
	
	// uniforms
	Unif_fv2	wnd_dim;
	
	void init () {
		compile();
		
		wnd_dim.loc =		glGetUniformLocation(prog, "wnd_dim");
		dbg_assert(wnd_dim.loc != -1);
		
		auto tex = glGetUniformLocation(prog, "tex");
		dbg_assert(tex != -1);
		glUniform1i(tex, 0);
	}
	void bind_texture (Texture tex) {
		glActiveTexture(GL_TEXTURE0 +0);
		glBindTexture(GL_TEXTURE_2D, tex.gl);
	}
};

using font::Font;

//
static constexpr cstr	PROJECT_NAME =		u8"asteroids";
static cstr				game_name =			u8"「アステロイド」ー【ASTEROIDS】";

static f32				running_avg_fps;

static std::basic_string<utf8>		dbg_name_and_fps;
static std::basic_string<utf8>		wnd_title;

static Font				g_font;

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
	
	g_font.init("consola.ttf"); // fixed font for now
	
	shad_tex.init();
	shad_px_col.init();
	vbo_px_col.init();
	
}

static void frame () {
	
	if (frame_indx != 0) {
		f32 alpha = 0.025f;
		running_avg_fps = running_avg_fps*(1.0f -alpha) +(1.0f/dt)*alpha;
	}
	{
		prints(&wnd_title, "%s  ~%.1f fps  %.3f ms", PROJECT_NAME, running_avg_fps, dt*1000);
		glfwSetWindowTitle(wnd, wnd_title.c_str());
	}
	
	v4 background_col = v4( srgb(41,49,52), 1 );
	
	glViewport(0, 0, wnd_dim.x, wnd_dim.y);
	
	glClearColor(background_col.x, background_col.y, background_col.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	
	
	{
		shad_tex.bind();
		shad_tex.wnd_dim.set( (v2)wnd_dim );
		shad_tex.bind_texture(g_font.tex);
		
		g_font.draw_buffer(shad_tex, buffer);
	}
	
	{
		shad_px_col.bind();
		shad_px_col.wnd_dim.set( (v2)wnd_dim );
		
		v4 col = v4(1,1,1,1);
		
		std::initializer_list<Vertex> data = {
			{ v2(50.5f, 50.5f), col },
			{ v2(200.5f, 51.5f), col },
		};
		
		vbo_px_col.upload(data);
		vbo_px_col.bind(shad_px_col);
		
		glDrawArrays(GL_LINES, 0, data.size());
	}
	
}

int main (int argc, char** argv) {
	
	setup_glfw();
	
	glEnable(GL_FRAMEBUFFER_SRGB);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
	{ // for ogl 3.3 and up (i'm not using vaos)
		GLuint vao;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
	}
	
	init();
	
	bool	dragging = false;
	v2		dragging_grab_pos_world;
	
	dt = 0;
	u64	initial_ts = glfwGetTimerValue();
	u64	prev_frame_end = initial_ts;
	
	for (frame_indx=0;; ++frame_indx) {
		t = (f64)(prev_frame_end -initial_ts) / glfwGetTimerFrequency();
		
		scrollwheel_diff = 0;
		
		glfwPollEvents();
		
		if (glfwWindowShouldClose(wnd)) break;
		
		{
			glfwGetFramebufferSize(wnd, &wnd_dim.x, &wnd_dim.y);
			dbg_assert(wnd_dim.x > 0 && wnd_dim.y > 0);
			
			v2 tmp = (v2)wnd_dim;
			wnd_dim_aspect = tmp / v2(tmp.y, tmp.x);
		}
		
		{
			dv2 pos;
			glfwGetCursorPos(wnd, &pos.x, &pos.y);
			mcursor_pos = iv2( (s32)floor(pos.x), (s32)floor(pos.y) );
		}
		
		frame();
		
		glfwSwapBuffers(wnd);
		
		{
			u64 now = glfwGetTimerValue();
			dt = (f32)(now -prev_frame_end) / (f32)glfwGetTimerFrequency();
			prev_frame_end = now;
		}
	}
	
	glfwDestroyWindow(wnd);
	glfwTerminate();
	
	return 0;
}
