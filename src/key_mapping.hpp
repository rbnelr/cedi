
#include <unordered_map>

enum command_e {
	CMD_IGNORE=0,
	
	CMD_MOVE_CURSOR_LEFT,
	CMD_MOVE_CURSOR_RIGHT,
	CMD_MOVE_CURSOR_UP,
	CMD_MOVE_CURSOR_DOWN,
	
	CMD_SCROLL_PAGE_UP,
	CMD_SCROLL_PAGE_DOWN,
	
	CMD_INSERT_ENTER,
	CMD_INSERT_TAB,
	
	CMD_DELETE_PREV,
	CMD_DELETE_NEXT,
	
	CMD_
};
struct Event_To_Cmd {
	int key, action, mods;
	
	NOINLINE bool operator== (Event_To_Cmd cr r) const {
		return key == r.key && action == r.action && mods == r.mods;
	}
};
namespace std {
	template <> struct hash<Event_To_Cmd> {
		NOINLINE size_t operator() (Event_To_Cmd cr k) const {
			return    std::hash<int>()(k.key)
					^ std::hash<int>()(k.action)
					^ std::hash<int>()(k.mods);
		}
	};
}

static std::unordered_map<Event_To_Cmd, command_e> _map_event_to_cmd = {
	{ {GLFW_KEY_LEFT,		GLFW_PRESS,		0},		CMD_MOVE_CURSOR_LEFT },
	{ {GLFW_KEY_LEFT,		GLFW_REPEAT,	0},		CMD_MOVE_CURSOR_LEFT },
	{ {GLFW_KEY_RIGHT,		GLFW_PRESS,		0},		CMD_MOVE_CURSOR_RIGHT },
	{ {GLFW_KEY_RIGHT,		GLFW_REPEAT,	0},		CMD_MOVE_CURSOR_RIGHT },
	{ {GLFW_KEY_UP,			GLFW_PRESS,		0},		CMD_MOVE_CURSOR_UP },
	{ {GLFW_KEY_UP,			GLFW_REPEAT,	0},		CMD_MOVE_CURSOR_UP },
	{ {GLFW_KEY_DOWN,		GLFW_PRESS,		0},		CMD_MOVE_CURSOR_DOWN },
	{ {GLFW_KEY_DOWN,		GLFW_REPEAT,	0},		CMD_MOVE_CURSOR_DOWN },
	
	{ {GLFW_KEY_PAGE_UP,	GLFW_PRESS,		0},		CMD_SCROLL_PAGE_UP },
	{ {GLFW_KEY_PAGE_UP,	GLFW_REPEAT,	0},		CMD_SCROLL_PAGE_UP },
	{ {GLFW_KEY_PAGE_DOWN,	GLFW_PRESS,		0},		CMD_SCROLL_PAGE_DOWN },
	{ {GLFW_KEY_PAGE_DOWN,	GLFW_REPEAT,	0},		CMD_SCROLL_PAGE_DOWN },
	
	{ {GLFW_KEY_ENTER,		GLFW_PRESS,		0},		CMD_INSERT_ENTER },
	{ {GLFW_KEY_ENTER,		GLFW_REPEAT,	0},		CMD_INSERT_ENTER },
	{ {GLFW_KEY_KP_ENTER,	GLFW_PRESS,		0},		CMD_INSERT_ENTER },
	{ {GLFW_KEY_KP_ENTER,	GLFW_REPEAT,	0},		CMD_INSERT_ENTER },
	{ {GLFW_KEY_TAB,		GLFW_PRESS,		0},		CMD_INSERT_TAB },
	{ {GLFW_KEY_TAB,		GLFW_REPEAT,	0},		CMD_INSERT_TAB },
	
	{ {GLFW_KEY_BACKSPACE,	GLFW_PRESS,		0},		CMD_DELETE_PREV },
	{ {GLFW_KEY_BACKSPACE,	GLFW_REPEAT,	0},		CMD_DELETE_PREV },
	{ {GLFW_KEY_DELETE,		GLFW_PRESS,		0},		CMD_DELETE_NEXT },
	{ {GLFW_KEY_DELETE,		GLFW_REPEAT,	0},		CMD_DELETE_NEXT },
};

static NOINLINE command_e map_event_to_cmd (int key, int action, int mods) {
	auto cmd_it = _map_event_to_cmd.find( Event_To_Cmd{key,action,mods} );
	if (cmd_it == _map_event_to_cmd.end()) return CMD_IGNORE;
	return cmd_it->second;
}

