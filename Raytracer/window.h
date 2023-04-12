#pragma once

#define WND_CLASS_NAME "marchingcubes"

#define CS_THREAD_NUM 16

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
	if (ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam))
		return 1;

	switch (message) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		break;
	}
	return DefWindowProcA(window, message, wparam, lparam);
}

class Window {
private:
	UINT m_width, m_height;
	HWND m_handle = 0;
	WNDCLASSA m_window_class = {};
	ComPtr<IDXGIDevice3> m_dxgi_device;
	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;
	ComPtr<IDXGISwapChain> m_swap_chain;
	ComPtr<ID3D11RenderTargetView> m_render_target;
	ComPtr<ID3D11VertexShader> m_vertex_shader;
	ComPtr<ID3D11PixelShader> m_pixel_shader;
	ComPtr<ID3D11InputLayout> m_input_layout;
	ComPtr<ID3D11Buffer> m_screen_vertex_buffer;
	ComPtr<ID3D11Texture2D> m_compute_texture;
	ComPtr<ID3D11Texture2D> m_screen_texture;
	ComPtr<ID3D11UnorderedAccessView> m_screen_tex_uav;
	ComPtr<ID3D11ShaderResourceView> m_screen_srv;
	ComPtr<ID3D11SamplerState> m_sampler;
	ComPtr<ID3D11ComputeShader> m_compute_shader;

	Input m_input;
public:
	HWND GetHandle() { return m_handle; }
	ID3D11Device* GetDevice() { return m_device.Get(); }
	ID3D11DeviceContext* GetContext() { return m_context.Get(); }
	Input* GetInput() { return &m_input; }
	bool PullMessages() {
		MSG message;
		if (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessageA(&message);
			if (message.message == WM_QUIT)
				exit(0);
			return 1;
		}
		return 0;
	}
	Window(const char* window_name, UINT width, UINT height) :
		m_input(m_handle) {
		// Window
		m_window_class.lpszClassName = WND_CLASS_NAME;
		m_window_class.lpfnWndProc = WindowProc;
		m_window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
		RegisterClassA(&m_window_class);
		m_handle = CreateWindowA(
			WND_CLASS_NAME, window_name, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			100, 100, width, height, 0, 0, 0, 0
		);

		RECT r;
		GetClientRect(m_handle, &r);
		m_width = r.right - r.left;
		m_height = r.bottom - r.top;

		// DirectX
		D3D_FEATURE_LEVEL levels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2,
			D3D_FEATURE_LEVEL_9_1
		};

		// This flag adds support for surfaces with a color-channel ordering different
		// from the API default. It is required for compatibility with Direct2D.
		UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(DEBUG) || defined(_DEBUG)
		deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		// Create the Direct3D 11 API device object and a corresponding context.
		HRESULT hr = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, deviceFlags, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &m_device, 0, &m_context);
		if (FAILED(hr)) return;

		hr = m_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&m_dxgi_device);
		if (FAILED(hr)) return;

		DXGI_SWAP_CHAIN_DESC desc;
		ZeroMemory(&desc, sizeof(DXGI_SWAP_CHAIN_DESC));
		desc.Windowed = 1;
		desc.BufferCount = 2;
		desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.SampleDesc.Count = 1;      //multisampling setting
		desc.SampleDesc.Quality = 0;    //vendor-specific flag
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		desc.OutputWindow = m_handle;

		// Create swap chain
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		Microsoft::WRL::ComPtr<IDXGIFactory> factory;

		hr = m_dxgi_device->GetAdapter(&adapter);
		if (FAILED(hr)) return;

		adapter->GetParent(IID_PPV_ARGS(&factory));
		hr = factory->CreateSwapChain(m_device.Get(), &desc, &m_swap_chain);
		if (FAILED(hr)) return;

		ComPtr<ID3D11Resource> back_buffer;
		m_swap_chain->GetBuffer(0, __uuidof(ID3D11Resource), &back_buffer);
		m_device->CreateRenderTargetView(back_buffer.Get(), 0, &m_render_target);

		ComPtr<ID3DBlob> blob;

		// Vertex shader
		D3DReadFileToBlob(L"../x64/Debug/vertex_shader.cso", &blob);
		m_device->CreateVertexShader(
			blob->GetBufferPointer(),
			blob->GetBufferSize(),
			0, &m_vertex_shader
		);
		m_context->VSSetShader(m_vertex_shader.Get(), 0, 0);

		// Create input layout
		D3D11_INPUT_ELEMENT_DESC input_layout_desc[] = {
			{"Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};
		m_device->CreateInputLayout(
			input_layout_desc, ARRAYSIZE(input_layout_desc),
			blob->GetBufferPointer(), blob->GetBufferSize(), &m_input_layout
		);
		m_context->IASetInputLayout(m_input_layout.Get());

		// Pixel shader
		D3DReadFileToBlob(L"../x64/Debug/pixel_shader.cso", &blob);
		m_device->CreatePixelShader(
			blob->GetBufferPointer(),
			blob->GetBufferSize(),
			0, &m_pixel_shader
		);
		m_context->PSSetShader(m_pixel_shader.Get(), 0, 0);

		// Set primitive topology
		m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Configure viewport
		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = m_width;
		viewport.Height = m_height;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;
		m_context->RSSetViewports(1, &viewport);

		// Create screen vertex buffer
		float vertices[] = {
			1.0f,  1.0f, 1.0f, 1.0f,
			1.0f, -1.0f, 1.0f, 0.0f,
		   -1.0f,  1.0f, 0.0f, 1.0f,
			1.0f, -1.0f, 1.0f, 0.0f,
		   -1.0f, -1.0f, 0.0f, 0.0f,
		   -1.0f,  1.0f, 0.0f, 1.0f
		};
		D3D11_BUFFER_DESC bufferDesc;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.ByteWidth = sizeof(vertices);
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = vertices;
		InitData.SysMemPitch = 0;
		InitData.SysMemSlicePitch = 0;
		hr = m_device->CreateBuffer(&bufferDesc, &InitData, &m_screen_vertex_buffer);

		// Create screen texture
		D3D11_TEXTURE2D_DESC tex_desc;
		ZeroMemory(&tex_desc, sizeof(tex_desc));
		tex_desc.Width = WND_WIDTH;
		tex_desc.Height = WND_HEIGHT;
		tex_desc.MipLevels = 1;
		tex_desc.ArraySize = 1;
		tex_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.SampleDesc.Quality = 0;
		tex_desc.Usage = D3D11_USAGE_DEFAULT;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		tex_desc.CPUAccessFlags = 0;
		tex_desc.MiscFlags = 0;
		hr = m_device->CreateTexture2D(&tex_desc, 0, &m_compute_texture);
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		hr = m_device->CreateTexture2D(&tex_desc, 0, &m_screen_texture);

		// Create and bind screen texture UAV
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.Format = DXGI_FORMAT_UNKNOWN;
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice = 0;
		hr = m_device->CreateUnorderedAccessView(m_compute_texture.Get(), &uav_desc, &m_screen_tex_uav);
		m_context->CSSetUnorderedAccessViews(0, 1, m_screen_tex_uav.GetAddressOf(), 0);

		// Create and bind the screen SRV
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = tex_desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MostDetailedMip = 0;
		srv_desc.Texture2D.MipLevels = 1;
		hr = m_device->CreateShaderResourceView(m_screen_texture.Get(), &srv_desc, &m_screen_srv);
		m_context->PSSetShaderResources(0, 1, m_screen_srv.GetAddressOf());

		// Create and bind texture sampler
		D3D11_SAMPLER_DESC sampler_desc = {};
		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		hr = m_device->CreateSamplerState(&sampler_desc, &m_sampler);
		m_context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());

		// Create the compute shader
		D3DReadFileToBlob(L"../x64/Debug/compute_shader.cso", &blob);
		hr = m_device->CreateComputeShader(
			blob->GetBufferPointer(),
			blob->GetBufferSize(),
			0, &m_compute_shader
		);
	}
	void Clear(float r, float g, float b) {
		float color[4] = { r, g, b, 1 };
		m_context->OMSetRenderTargets(1, m_render_target.GetAddressOf(), 0);
		m_context->ClearRenderTargetView(m_render_target.Get(), color);
	}
	void Render() {
		const UINT stride = sizeof(float) * 4;
		const UINT offset = 0;
		m_context->IASetVertexBuffers(0, 1, m_screen_vertex_buffer.GetAddressOf(), &stride, &offset);
		m_context->CSSetShader(m_compute_shader.Get(), 0, 0);
		m_context->CopyResource(m_screen_texture.Get(), m_compute_texture.Get());
		m_context->Dispatch(WND_WIDTH / CS_THREAD_NUM, WND_HEIGHT / CS_THREAD_NUM, 1);
		m_context->Draw(6, 0);
	}
	void Present() {
		m_swap_chain->Present(1, 0);
	}
};