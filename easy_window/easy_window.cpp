#include <d3d9.h>

#include "easy_window.h"

#include <iostream>

class my_render : public voidptr::directx9_window {
public:
	my_render(std::string_view name) : voidptr::directx9_window(name) {}

	virtual void render() override {
		D3DCOLOR rectColor = D3DCOLOR_XRGB(255, 255, 255);	//No point in using alpha because clear & alpha dont work!
		D3DRECT BarRect = { 0, 0, 100, 100 };

		device->Clear(1, &BarRect, D3DCLEAR_TARGET | D3DCLEAR_TARGET, rectColor, 0, 0);
	}
};

int main() {
	try {
		my_render window("I`m eblan");
		voidptr::directx9_creation_params params;
		params.flags = WS_OVERLAPPED | WS_SYSMENU;
		params.size = { 400, 600 };
		params.position = { 100, 100 };

		window.create_window(&params);

		window.show_window();
		window.update_window();
		window.start_window_loop();

	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
}

