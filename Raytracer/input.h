#pragma once

class Input {
private:
	HWND m_window_handle;
	int16_t m_mouse_x = 0;
	int16_t m_mouse_y = 0;
	int16_t m_delta_mouse_x = 0;
	int16_t m_delta_mouse_y = 0;
	void UpdateMousePos() {
		int16_t lmx = m_mouse_x;
		int16_t lmy = m_mouse_y;
		POINT p;
		GetCursorPos(&p);
		ScreenToClient(m_window_handle, &p);
		m_mouse_x = p.x;
		m_mouse_y = p.y;
		m_delta_mouse_x = m_mouse_x - lmx;
		m_delta_mouse_y = m_mouse_y - lmy;
	}
public:
	int16_t GetMouseX() { return m_mouse_x; }
	int16_t GetMouseY() { return m_mouse_y; }
	int16_t GetDeltaMouseX() { return m_delta_mouse_x; }
	int16_t GetDeltaMouseY() { return m_delta_mouse_y; }
	Input(HWND window_handle) {
		m_window_handle = window_handle;
	}
	void Update() {
		UpdateMousePos();
	}
	bool GetKeyDown(uint8_t key) {
		return GetAsyncKeyState(key);
	}
};