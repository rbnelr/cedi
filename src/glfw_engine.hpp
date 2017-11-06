
static void init ();
static void refresh ();

static void move_cursor_left ();
static void move_cursor_right ();
static void move_cursor_up ();
static void move_cursor_down ();

static void insert_char (utf32 c);
static void insert_tab ();
static void insert_newline ();
static void delete_prev_char ();
static void delete_next_char ();


struct Rect {
	iv2 pos;
	iv2 dim;
};

static GLFWmonitor*	primary_monitor;
static GLFWwindow*	wnd;
static bool			fullscreen;
static Rect			_suggested_wnd_rect;
static Rect			_restore_wnd_rect;

static iv2			wnd_dim;
static v2			wnd_dim_aspect;
static iv2			mcursor_pos;
static bool			mcursor_in_wnd;

static void platform_get_frame_input () {
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
}

static s32 _scrollwheel_diff = 0;
static void glfw_scroll_proc (GLFWwindow* window, f64 xoffset, f64 yoffset) {
	_scrollwheel_diff += (s32)floor(yoffset);
}

static s32 get_scrollwheel_diff () {
	auto ret = _scrollwheel_diff;
	_scrollwheel_diff = 0;
	return _scrollwheel_diff;
}

static bool			resizing_tab_spaces; // needed state for CTRL+T+(+/-) control

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
	
	glfwSwapInterval(0); // seems like vsync needs to be set after switching to from the inital hidden window to a fullscreen one, or there will be no vsync
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

static void glfw_error_proc (int err, cstr msg) {
	fprintf(stderr, ANSI_COLOUR_CODE_RED "GLFW Error! 0x%x '%s'\n" ANSI_COLOUR_CODE_NC, err, msg);
}

static void glfw_text_proc (GLFWwindow* window, ui codepoint) {
	//printf("glfw_text_proc: '%c' [%x]\n", codepoint, codepoint);
	insert_char(codepoint);
}
static void glfw_key_proc (GLFWwindow* window, int key, int scancode, int action, int mods) {
	dbg_assert(action == GLFW_PRESS || action == GLFW_REPEAT || action == GLFW_RELEASE);
	
	//cstr name = glfwGetKeyName(key, scancode);
	//printf("Button %s: %d\n", name ? name : "<null>", action);
	
	s32 generic_incdec = 0;
	
	if (action != GLFW_RELEASE) { // pressed or repeated ...
		
		switch (key) {
			case GLFW_KEY_BACKSPACE: {
				delete_prev_char();
			} break;
			case GLFW_KEY_DELETE: {
				delete_next_char();
			} break;
			case GLFW_KEY_ENTER:
			case GLFW_KEY_KP_ENTER: {
				insert_newline();
			} break;
			case GLFW_KEY_TAB: {
				insert_tab();
			} break;
			
			case GLFW_KEY_LEFT: {
				move_cursor_left();
			} break;
			case GLFW_KEY_RIGHT: {
				move_cursor_right();
			} break;
			case GLFW_KEY_UP: {
				move_cursor_up();
			} break;
			case GLFW_KEY_DOWN: {
				move_cursor_down();
			} break;
			
			// generic decrease
			case GLFW_KEY_MINUS:
			case GLFW_KEY_KP_SUBTRACT : {
				generic_incdec = -1;
			} break;
			// generic increase
			case GLFW_KEY_EQUAL: // pseudo '+' key, this would be the '+' key when shift is pressed
			case GLFW_KEY_KP_ADD: {
				generic_incdec = +1;
			} break;
		}
	}
	
	if (action == GLFW_PRESS) { // pressed ...
		
		switch (key) {
			case GLFW_KEY_F11: { // ... key
				toggle_fullscreen();
			} break;
		}
		
		if (mods & GLFW_MOD_ALT) { // ... ALT+key
			switch (key) {
				case GLFW_KEY_N: {
					opt.draw_whitespace = !opt.draw_whitespace;
				} break;
			}
		}
	}
	
	if (action != GLFW_REPEAT) { // pressed or released ...
		
		if (mods & GLFW_MOD_ALT) { // ... ALT+key
			switch (key) {
				case GLFW_KEY_T : {
					resizing_tab_spaces = action == GLFW_PRESS;
				} break;
			}
		}
	}
	
	if (resizing_tab_spaces) {
		opt.tab_spaces = max(opt.tab_spaces +generic_incdec, 1);
	}
	
}

static void glfw_refresh (GLFWwindow* wnd) {
	refresh();
}

static void setup_glfw () {
	glfwSetErrorCallback(glfw_error_proc);
	
	dbg_assert( glfwInit() );
	
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,	3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,	1);
	//glfwWindowHint(GLFW_OPENGL_PROFILE,			GLFW_OPENGL_CORE_PROFILE);
	
	primary_monitor = glfwGetPrimaryMonitor();
	
	glfwWindowHint(GLFW_VISIBLE,				0);
	wnd = glfwCreateWindow(1280, 720, u8"", NULL, NULL);
	dbg_assert(wnd);
	
	glfwGetWindowPos(wnd, &_suggested_wnd_rect.pos.x,&_suggested_wnd_rect.pos.y);
	glfwGetWindowSize(wnd, &_suggested_wnd_rect.dim.x,&_suggested_wnd_rect.dim.y);
	
	glfwSetWindowRefreshCallback(wnd, glfw_refresh);
	
	glfwSetKeyCallback(wnd, glfw_key_proc);
	glfwSetCharCallback(wnd, glfw_text_proc);
	glfwSetScrollCallback(wnd, glfw_scroll_proc);
	//glfwSetMouseButtonCallback(wnd, glfw_mousebutton_proc);
	
	glfwMakeContextCurrent(wnd);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	
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
	
	do {
		glfwWaitEvents();
	} while (!glfwWindowShouldClose(wnd));
	
	glfwDestroyWindow(wnd);
	glfwTerminate();
	
	return 0;
}

static void platform_present_frame () {
	glfwSwapBuffers(wnd);
}