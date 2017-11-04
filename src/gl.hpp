#include "glad.c"
#include "GLFW/glfw3.h"

STATIC_ASSERT(sizeof(GLint) ==		sizeof(s32));
STATIC_ASSERT(sizeof(GLuint) ==		sizeof(u32));
STATIC_ASSERT(sizeof(GLsizei) ==	sizeof(u32));
STATIC_ASSERT(sizeof(GLsizeiptr) ==	sizeof(u64));

#if 1
#define GLSL_VERSION "#version 140\n"
#else
#define GLSL_VERSION "#version 330\n"
#endif

struct Texture {
	GLuint	gl;
	u8*		data;
	u32		w;
	u32		h;
	
	void alloc (u32 w, u32 h) {
		glGenTextures(1, &gl);
		this->w = w;
		this->h = h;
		data = (u8*)::malloc(w*h*sizeof(u8));
	}
	
	u8* operator[] (u32 row) { return &data[row*w]; }
	
	void inplace_vertical_flip () {
		u8* row_a = (*this)[0];
		u8* row_b = (*this)[h -1];
		for (u32 y=0; y<(h/2); ++y) {
			dbg_assert(row_a < row_b);
			for (u32 x=0; x<w; ++x) {
				u8 tmp = row_a[x];
				row_a[x] = row_b[x];
				row_b[x] = tmp;
			}
			row_a += w;
			row_b -= w;
		}
	}
};

static void bind_backbuffer (iv2 res) {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	glViewport(0, 0, res.x, res.y);
	
}
static void clear_framebuffer (v4 clear_col) {
	glClearColor(clear_col.x, clear_col.y, clear_col.z, clear_col.w);
	glClear(GL_COLOR_BUFFER_BIT);
}

struct RGBA_Framebuffer {
	GLuint	fb;
	GLuint	tex;
	
	void init () {
		glGenFramebuffers(1,	&fb);
		glGenTextures(1,		&tex);
		
		glBindFramebuffer(GL_FRAMEBUFFER, fb);
		
		{
			glBindTexture(GL_TEXTURE_2D, tex);
			
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
		}
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	
	void bind_and_clear (iv2 res, v4 clear_col) {
		glBindFramebuffer(GL_FRAMEBUFFER, fb);
		
		glBindTexture(GL_TEXTURE_2D, tex);
		
		// GL_RGBA32F
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, res.x,res.y,
				0, GL_RGBA, GL_FLOAT, NULL);
		
		glTexParameteri(GL_TEXTURE_2D,	GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,	GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		
		glTexParameteri(GL_TEXTURE_2D,	GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D,	GL_TEXTURE_MAX_LEVEL, 0);
		
		glBindTexture(GL_TEXTURE_2D, 0);
		
		glViewport(0, 0, res.x, res.y);
		
		clear_framebuffer(clear_col);
		
	}
};

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
struct Unif_fv4 {
	GLint loc;
	void set (fv4 cr v) const {
		glUniform4fv(loc, 1, &v.x);
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

struct VBO_Text {
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
struct Shader_Text : Basic_Shader {
	Shader_Text (): Basic_Shader(
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
		dbg_assert(wnd_dim.loc >= 0);
		
		auto tex = 			glGetUniformLocation(prog, "tex");
		dbg_assert(tex >= 0);
		glUniform1i(tex, 0);
	}
	void bind_texture (Texture tex) {
		glActiveTexture(GL_TEXTURE0 +0);
		glBindTexture(GL_TEXTURE_2D, tex.gl);
	}
};

struct Shader_Fullscreen_Tex_Copy : Basic_Shader {
	Shader_Fullscreen_Tex_Copy (): Basic_Shader(
// Vertex shader
GLSL_VERSION R"_SHAD(
	void main() {
		switch (gl_VertexID) {
			case 0: gl_Position = vec4(-1,-1, 0,1); break;
			case 1: gl_Position = vec4(+1,-1, 0,1); break;
			case 2: gl_Position = vec4(-1,+1, 0,1); break;
			case 3: gl_Position = vec4(-1,+1, 0,1); break;
			case 4: gl_Position = vec4(+1,-1, 0,1); break;
			case 5: gl_Position = vec4(+1,+1, 0,1); break;
		}
	}
)_SHAD",
// Fragment shader
GLSL_VERSION R"_SHAD(
	out		vec4	frag_col;
	
	uniform vec2	wnd_dim;
	
	uniform sampler2D 	tex;
	
	void main() {
		frag_col = texture(tex, gl_FragCoord.xy / wnd_dim);
	}
)_SHAD"
	) {}
	
	// uniforms
	Unif_fv2	wnd_dim;
	
	void init () {
		compile();
		
		wnd_dim.loc =			glGetUniformLocation(prog, "wnd_dim");
		dbg_assert(wnd_dim.loc >= 0);
		
		auto tex =		glGetUniformLocation(prog, "tex");
		dbg_assert(tex >= 0);
		glUniform1i(tex, 0);
	}
	void bind_fb (RGBA_Framebuffer cr fb) {
		glActiveTexture(GL_TEXTURE0 +0);
		glBindTexture(GL_TEXTURE_2D, fb.tex);
	}
	
};

struct VBO_Cursor_Pass {
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
struct Shader_Cursor_Pass : Basic_Shader {
	Shader_Cursor_Pass (): Basic_Shader(
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
	in		vec2	uv;
	in		vec4	color;
	out		vec4	frag_col;
	
	uniform vec2	wnd_dim;
	uniform vec3	col_background;
	uniform vec3	col_highlighted;
	
	uniform sampler2D 	fb_text;
	
	void main() {
		vec3 col = col_background;
		
		col *= 1 -color.a;
		col += color.rgb * color.a;
		
		vec4 text_col = texture(fb_text, gl_FragCoord.xy / wnd_dim);
		text_col.a = pow(text_col.a, 1.0/3);
		
		col *= 1 -text_col.a;
		col += col_highlighted*text_col.a;
		
		frag_col = vec4(col, 1);
	}
)_SHAD"
	) {}
	
	// uniforms
	Unif_fv2	wnd_dim;
	Unif_fv3	col_background;
	Unif_fv3	col_highlighted;
	
	void init () {
		compile();
		
		wnd_dim.loc =			glGetUniformLocation(prog, "wnd_dim");
		dbg_assert(wnd_dim.loc >= 0);
		
		col_background.loc =	glGetUniformLocation(prog, "col_background");
		dbg_assert(col_background.loc >= 0);
		
		col_highlighted.loc =	glGetUniformLocation(prog, "col_highlighted");
		dbg_assert(col_highlighted.loc >= 0);
		
		auto fb_text =		glGetUniformLocation(prog, "fb_text");
		dbg_assert(fb_text >= 0);
		glUniform1i(fb_text, 0);
	}
	void bind_fb (RGBA_Framebuffer cr fb) {
		glActiveTexture(GL_TEXTURE0 +0);
		glBindTexture(GL_TEXTURE_2D, fb.tex);
	}
	
};
