#pragma once

#include <Windows.h>
#include <string>
#include <functional>
#include <stdexcept>
#include <format>

#define VPTR_EW__DISABLE_KEYMENU case WM_SYSCOMMAND: \
		if ((wparam & 0xfff0) == SC_KEYMENU) \
			return 0; \
		break;

#ifdef UNICODE
#define STRING std::wstring
#define STRING_VIEW std::wstring_view
#else
#define STRING std::string
#define STRING_VIEW std::string_view
#endif

namespace voidptr {
	using window_position_t = std::pair<int, int>;
	using window_size_t = std::pair<int, int>;

	struct i_window_create_params {
	public:
		template <typename T> auto get() { return reinterpret_cast<T*>(this); }
	};

	class i_easy_window {
	protected:
		const STRING name;
	public:
		explicit i_easy_window(STRING_VIEW name) : name(name) {}
		virtual ~i_easy_window() = default;
		virtual void destroy_window() = 0;
		virtual void create_window(i_window_create_params*) = 0;
		virtual void start_window_loop() = 0;
		virtual void loop_cycle() = 0;
	};

	#ifdef WIN32
	using win32_window_flags_t = uint32_t;

	struct win32_creation_params : public i_window_create_params {
		win32_window_flags_t flags;
		window_position_t position;
		window_size_t size;
	};

	class win32_window : public i_easy_window {
	private:
		static LRESULT WINAPI static_windproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
			if (msg == WM_NCCREATE) {
				const auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
				auto _this = cs->lpCreateParams;
				SetWindowLongPtr(hwnd, 0, reinterpret_cast<LONG_PTR>(_this));
			}

			if (auto* window = reinterpret_cast<win32_window*>((GetWindowLongPtr(hwnd, 0)))) {
				return window->wndproc(hwnd, msg, wparam, lparam);
			} else {
				return ::DefWindowProc(hwnd, msg, wparam, lparam);
			}
		}

	protected:
		const STRING name;
		const STRING classname;
		bool alive;
		HWND hwnd;
		WNDCLASSEX wc;

	public:
		explicit win32_window(STRING_VIEW name) : i_easy_window(name), classname(STRING(name) + TEXT("_WINDOWCLASS")) {}
		~win32_window() override {
			destroy_window();
		}

		virtual void destroy_window() override {
			if (hwnd) {
				DestroyWindow(hwnd);
			}

			if (wc.hInstance) {
				UnregisterClass(wc.lpszClassName, wc.hInstance);
			}
		}

		virtual void create_window(i_window_create_params* params) override {
			auto& [x, y] = params->get<win32_creation_params>()->position;
			auto& [w, h] = params->get<win32_creation_params>()->size;

			wc = {
				sizeof(WNDCLASSEX), CS_CLASSDC, static_windproc, 0l, sizeof(win32_window*), GetModuleHandle(0),
				0, 0, 0, 0, classname.c_str()
			};

			RegisterClassEx(&wc);
			hwnd = CreateWindow(wc.lpszClassName, name.c_str(), params->get<win32_creation_params>()->flags, x, y, w, h, 0, 0, wc.hInstance, (void*)this);
			if (!hwnd) {
				throw std::logic_error(std::format("Cannot create window with error: {:x}", GetLastError()));
			}
		}

		inline void show_window(int nc = SW_SHOWDEFAULT) const {
			ShowWindow(hwnd, nc);
		}

		inline void update_window() const {
			UpdateWindow(hwnd);
		}

		virtual void start_window_loop() override {
			alive = true;
			while (alive) {
				MSG msg;
				while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
					this->handle_message(msg);
				}
				if (!alive)
					break;
				this->loop_cycle();
			}
		}

		virtual void loop_cycle() override {}

		void handle_message(MSG msg) {
			if (msg.message == WM_QUIT) {
				alive = false;
			}
		}

		virtual LRESULT WINAPI wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
			switch (msg) {
			case WM_DESTROY:
				::PostQuitMessage(0);
				return 0;
			}
			return ::DefWindowProc(hwnd, msg, wparam, lparam);
		}
	};
	#endif

	#ifdef DIRECT3D_VERSION
	struct directx9_creation_params : public win32_creation_params {};

	class directx9_window : public win32_window {
	private:
		void create_device() {
			if ((d3d = Direct3DCreate9(D3D_SDK_VERSION)) == NULL) {
				throw std::runtime_error(std::format("Cannot create d3d"));
			}

			memset(&present_parameters, 0x0, sizeof(present_parameters));
			present_parameters.Windowed = TRUE;
			present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
			present_parameters.BackBufferFormat = D3DFMT_UNKNOWN;
			present_parameters.EnableAutoDepthStencil = TRUE;
			present_parameters.AutoDepthStencilFormat = D3DFMT_D16;
			present_parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
			present_parameters.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
			if (d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &device) < 0) {
				throw std::runtime_error(std::format("Cannot create directx device"));
			}
		}

		void cleanup_device() {
			if (device) {
				device->Release();
				device = nullptr;
			}
			if (d3d) {
				d3d->Release();
				d3d = nullptr;
			}
		}

	protected:
		virtual void reset_device() {
			device->Reset(&present_parameters);
		}

	protected:
		IDirect3DDevice9* device;
		IDirect3D9* d3d;
		D3DPRESENT_PARAMETERS present_parameters;

	public:
		directx9_window(STRING_VIEW name) : win32_window(name) {}
		virtual ~directx9_window() override {
			cleanup_device();
		}

		virtual void when_device_created() {}

		virtual void start_window_loop() override {
			create_device();
			when_device_created();
			win32_window::start_window_loop();
		}

		virtual void render() {}

		virtual void apply_render() {
			device->EndScene();
		}

		virtual void loop_cycle() override {
			device->SetRenderState(D3DRS_ZENABLE, FALSE);
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
			device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 255, 255), 1.f, 0);

			if (device->BeginScene() >= 0) {
				render();
				apply_render();
			}

			if (const auto result = device->Present(0, 0, 0, 0);
				result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
				reset_device();
		}
	};

	#endif
}