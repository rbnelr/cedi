
static void init ();
static void resize_wnd (iv2 dim);
static void draw ();

static void move_cursor_left ();
static void move_cursor_right ();
static void move_cursor_up ();
static void move_cursor_down ();
static void pageup_scroll ();
static void pagedown_scroll ();
static void mouse_scroll (s32 offs);

static void insert_char (utf32 c);
static void insert_tab ();
static void insert_newline ();
static void delete_prev_char ();
static void delete_next_char ();

static void open_file (cstr filename);

//
static f32 dt = 0;
static bool continuous_drawing = false;

static void set_continuous_drawing (bool state) {
	if (continuous_drawing != state) {
		
		if (state) {
			// started continuous_drawing
			dt = 1.0f / 60; // update first frame of smooth scrolling with 60 fps, since there is no real dt to calculate here
		}
		
		printf(">> %s smooth scrolling\n", state ? "started":"stopped");
	}
	continuous_drawing = state;
}

static void draw_if_not_continuous_drawing () {
	if (!continuous_drawing) draw();
}


struct Rect {
	iv2 pos;
	iv2 dim;
};

static GLFWmonitor*	primary_monitor;
static GLFWwindow*	wnd;
static bool			fullscreen;
static Rect			_suggested_wnd_rect;
static Rect			_restore_wnd_rect;

static void glfw_resize (GLFWwindow* window, int width, int height) {
	resize_wnd( max(iv2(width,height), iv2(1)) );
}

static void glfw_scroll_proc (GLFWwindow* window, f64 xoffset, f64 yoffset) {
	mouse_scroll( (s32)floor(yoffset) );
	draw_if_not_continuous_drawing();
}

static bool			_resizing_tab_spaces; // needed state for CTRL+T+(+/-) control

static char _filename_buf[512];

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
	draw_if_not_continuous_drawing();
}
static void glfw_key_proc (GLFWwindow* window, int key, int scancode, int action, int mods) {
	dbg_assert(action == GLFW_PRESS || action == GLFW_REPEAT || action == GLFW_RELEASE);
	
	bool update = false;
	
	//cstr name = glfwGetKeyName(key, scancode);
	//printf("Button %s: %d\n", name ? name : "<null>", action);
	
	s32 generic_incdec = 0;
	
	if (action != GLFW_RELEASE) { // pressed or repeated ...
		
		switch (key) {
			case GLFW_KEY_BACKSPACE: {
				delete_prev_char();
				update = true;
			} break;
			case GLFW_KEY_DELETE: {
				delete_next_char();
				update = true;
			} break;
			case GLFW_KEY_ENTER:
			case GLFW_KEY_KP_ENTER: {
				insert_newline();
				update = true;
			} break;
			case GLFW_KEY_TAB: {
				insert_tab();
				update = true;
			} break;
			
			case GLFW_KEY_LEFT: {
				move_cursor_left();
				update = true;
			} break;
			case GLFW_KEY_RIGHT: {
				move_cursor_right();
				update = true;
			} break;
			case GLFW_KEY_UP: {
				move_cursor_up();
				update = true;
			} break;
			case GLFW_KEY_DOWN: {
				move_cursor_down();
				update = true;
			} break;
			
			case GLFW_KEY_PAGE_UP: {
				pageup_scroll();
				update = true;
			} break;
			case GLFW_KEY_PAGE_DOWN: {
				pagedown_scroll();
				update = true;
			} break;
			
			// generic decrease
			case GLFW_KEY_MINUS:
			case GLFW_KEY_KP_SUBTRACT : {
				generic_incdec = -1;
				update = true;
			} break;
			// generic increase
			case GLFW_KEY_EQUAL: // pseudo '+' key, this would be the '+' key when shift is pressed
			case GLFW_KEY_KP_ADD: {
				generic_incdec = +1;
				update = true;
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
					update = true;
				} break;
			}
		}
		
		if (mods & GLFW_MOD_CONTROL) { // ... ALT+key
			switch (key) {
				case GLFW_KEY_O: {
					printf("Open File menu: ");
					
					_filename_buf[0] = '\0';
					fgets(_filename_buf, arrlen(_filename_buf), stdin);
					
					auto len = strlen(_filename_buf);
					if (_filename_buf[len -1] == '\n') {
						_filename_buf[len -1] = '\0';
					}
					
					open_file(_filename_buf);
					update = true;
				} break;
			}
		}
	}
	
	if (action != GLFW_REPEAT) { // pressed or released ...
		
		if (mods & GLFW_MOD_ALT) { // ... ALT+key
			switch (key) {
				case GLFW_KEY_T: {
					_resizing_tab_spaces = action == GLFW_PRESS;
					update = true;
				} break;
			}
		}
	}
	
	if (_resizing_tab_spaces) {
		opt.tab_spaces = max(opt.tab_spaces +generic_incdec, 1);
	}
	
	if (update) draw_if_not_continuous_drawing();
}

static void glfw_refresh (GLFWwindow* wnd) {
	draw_if_not_continuous_drawing();
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
	glfwSetFramebufferSizeCallback(wnd, glfw_resize);
	
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
	
	f64 t = glfwGetTime();
	
	do {
		bool was_continuous_drawing = continuous_drawing;
		if (!was_continuous_drawing) {
			glfwWaitEvents();
		} else {
			glfwPollEvents();
		}
		
		// NOTE: probably shouldn't use dt when event based drawing, since dt is the time since last frame
		{
			f64 now = glfwGetTime();
			dt = now -t;
			//printf(">>>>>> now %f t %f -> dt %f ms\n", now, t, dt * 1000);
			t = now;
		}
		
		if (was_continuous_drawing) {
			draw();
		}
		
	} while (!glfwWindowShouldClose(wnd));
	
	glfwDestroyWindow(wnd);
	glfwTerminate();
	
	return 0;
}

static void platform_present_frame () {
	glfwSwapBuffers(wnd);
}