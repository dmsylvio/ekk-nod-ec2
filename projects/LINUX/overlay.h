#pragma once
#define SDL_DISABLE_SYSWM_WAYLAND
#include <SDL3/SDL_syswm.h>
#include <X11/extensions/shapeconst.h>
#include <dlfcn.h>

inline bool enable_overlay_input_passthrough(SDL_Window* window)
{
	SDL_SysWMinfo info{};
	if (SDL_GetWindowWMInfo(window, &info, SDL_SYSWM_CURRENT_VERSION) != 0 ||
	    info.subsystem != SDL_SYSWM_X11)
		return false;

	static void* x11 = dlopen("libX11.so.6", RTLD_NOW | RTLD_LOCAL);
	static void* xext = dlopen("libXext.so.6", RTLD_NOW | RTLD_LOCAL);
	if (!x11 || !xext)
		return false;
	const auto attributes = reinterpret_cast<int (*)(Display*, Window, XWindowAttributes*)>(
		dlsym(x11, "XGetWindowAttributes"));
	const auto shape = reinterpret_cast<void (*)(Display*, Window, int, int, int, XRectangle*, int, int, int)>(
		dlsym(xext, "XShapeCombineRectangles"));
	const auto query = reinterpret_cast<Bool (*)(Display*, int*, int*)>(
		dlsym(xext, "XShapeQueryExtension"));
	const auto change_attributes = reinterpret_cast<int (*)(Display*, Window, unsigned long, XSetWindowAttributes*)>(
		dlsym(x11, "XChangeWindowAttributes"));
	const auto flush = reinterpret_cast<int (*)(Display*)>(dlsym(x11, "XFlush"));
	int event_base = 0, error_base = 0;
	XWindowAttributes details{};
	if (!attributes || !shape || !query || !flush || !change_attributes ||
	    !query(info.info.x11.display, &event_base, &error_base) ||
	    !attributes(info.info.x11.display, info.info.x11.window, &details) || details.depth != 32)
		return false;
	XSetWindowAttributes settings{};
	settings.override_redirect = True;
	change_attributes(info.info.x11.display, info.info.x11.window, CWOverrideRedirect, &settings);
	shape(info.info.x11.display, info.info.x11.window, ShapeInput, 0, 0, nullptr, 0, ShapeSet, Unsorted);
	flush(info.info.x11.display);
	return true;
}

#include <X11/Xutil.h>
#include <vector>
#include <cstring>

inline bool find_cs2_window(SDL_Window* overlay, SDL_Rect& bounds, bool* focused = nullptr)
{
	SDL_SysWMinfo info{};
	if (SDL_GetWindowWMInfo(overlay, &info, SDL_SYSWM_CURRENT_VERSION) != 0)
		return false;
	static void* x11 = dlopen("libX11.so.6", RTLD_NOW | RTLD_LOCAL);
	const auto tree = reinterpret_cast<Status (*)(Display*, Window, Window*, Window*, Window**, unsigned int*)>(dlsym(x11, "XQueryTree"));
	const auto hint = reinterpret_cast<Status (*)(Display*, Window, XClassHint*)>(dlsym(x11, "XGetClassHint"));
	const auto attributes = reinterpret_cast<Status (*)(Display*, Window, XWindowAttributes*)>(dlsym(x11, "XGetWindowAttributes"));
	const auto translate = reinterpret_cast<Bool (*)(Display*, Window, Window, int, int, int*, int*, Window*)>(dlsym(x11, "XTranslateCoordinates"));
	const auto free_data = reinterpret_cast<int (*)(void*)>(dlsym(x11, "XFree"));
	if (!tree || !hint || !attributes || !translate || !free_data)
		return false;
	Display* display = info.info.x11.display;
	const Window root = RootWindow(display, info.info.x11.screen);
	std::vector<Window> windows{root};
	for (size_t index = 0; index < windows.size() && index < 4096; ++index)
	{
		Window current = windows[index];
		XClassHint identity{};
		if (hint(display, current, &identity))
		{
			const bool match = identity.res_class && std::strcmp(identity.res_class, "cs2") == 0;
			if (identity.res_name) free_data(identity.res_name);
			if (identity.res_class) free_data(identity.res_class);
			XWindowAttributes details{};
			Window child = 0;
			if (match && attributes(display, current, &details) && details.map_state == IsViewable &&
			    translate(display, current, root, 0, 0, &bounds.x, &bounds.y, &child))
			{
				if (focused)
				{
					const auto get_focus = reinterpret_cast<int (*)(Display*, Window*, int*)>(dlsym(x11, "XGetInputFocus"));
					Window active = 0;
					int revert = 0;
					if (get_focus) get_focus(display, &active, &revert);
					*focused = active == current;
				}
				bounds.w = details.width;
				bounds.h = details.height;
				return bounds.w > 0 && bounds.h > 0;
			}
		}
		Window returned_root = 0, parent = 0, *children = nullptr;
		unsigned int count = 0;
		if (tree(display, current, &returned_root, &parent, &children, &count))
		{
			if (children && windows.size() + count <= 4096)
				windows.insert(windows.end(), children, children + count);
			if (children) free_data(children);
		}
	}
	return false;
}

class OverlayBuffer
{
public:
	OverlayBuffer() = default;
	OverlayBuffer(const OverlayBuffer&) = delete;
	OverlayBuffer& operator=(const OverlayBuffer&) = delete;
	~OverlayBuffer() { close(); }

	bool open(SDL_Window* window, int width, int height)
	{
		SDL_SysWMinfo info{};
		if (SDL_GetWindowWMInfo(window, &info, SDL_SYSWM_CURRENT_VERSION) != 0 ||
		    info.subsystem != SDL_SYSWM_X11)
			return false;
		display = info.info.x11.display;
		drawable = info.info.x11.window;
		gc = XCreateGC(display, drawable, 0, nullptr);
		if (!gc) return false;
		XSetGraphicsExposures(display, gc, False);
		return resize(width, height);
	}

	bool resize(int width, int height)
	{
		if (width <= 0 || height <= 0) return false;
		if (width == w && height == h && pixmap) return true;
		const Pixmap next = XCreatePixmap(display, drawable, width, height, 32);
		if (!next) return false;
		if (pixmap) XFreePixmap(display, pixmap);
		pixmap = next;
		w = width;
		h = height;
		repaint = true;
		begin();
		return true;
	}

	void begin()
	{
		if (painted || repaint)
		{
			XSetForeground(display, gc, 0);
			XFillRectangle(display, pixmap, gc, 0, 0, w, h);
		}
		dirty = false;
	}

	void fill(int x, int y, int width, int height, unsigned long pixel)
	{
		if (width <= 0 || height <= 0) return;
		XSetForeground(display, gc, pixel);
		XFillRectangle(display, pixmap, gc, x, y, width, height);
		dirty = true;
	}

	void present()
	{
		if (dirty || painted || repaint)
		{
			XCopyArea(display, pixmap, drawable, gc, 0, 0, w, h, 0, 0);
			XFlush(display);
		}
		painted = dirty;
		repaint = false;
	}

	void invalidate() { repaint = true; }

	bool size(int* width, int* height) const
	{
		*width = w;
		*height = h;
		return pixmap != 0;
	}

	void close()
	{
		if (pixmap) XFreePixmap(display, pixmap);
		if (gc) XFreeGC(display, gc);
		pixmap = 0;
		gc = nullptr;
	}

private:
	Display* display = nullptr;
	Window drawable = 0;
	GC gc = nullptr;
	Pixmap pixmap = 0;
	int w = 0;
	int h = 0;
	bool painted = false;
	bool dirty = false;
	bool repaint = false;
};
