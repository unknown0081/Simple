#include <dxgi.h>
#include <memory>
#include "Graphics.h"
#include "Shader.h"
#include "../ErrorLogger.h"
#include "../AdapterReader.h"
#include "../RegisterNum.h"
#include "../../External/ImGui/imgui.h"



// 初期化
void Graphics::Initialize(HWND hwnd, int windowWidth, int windowHeight)
{
	///////// AMD か NVIDIA のグラフィックボードがある場合はそれを使用する /////////

	// adapterの取得（pc内のすべてのグラフィックボードの取得）
	std::vector<AdapterData> adapters = AdapterReader::GetAdapters();
	if (adapters.size() < 1)	// adapterが取得できなかった場合の処理
	{
		ErrorLogger::Log("No DXGI Adapters found.");
	}

	AdapterData adapter = AdapterReader::GetAdapters()[0];	// 先頭のグラフィックボード(AMD NVIDIA がない場合はこれを使用する)

	for (AdapterData& adp : adapters)
	{
		if (adp.description.VendorId == 0x1002/*AMD*/ || adp.description.VendorId == 0x10DE/*NVIDIA*/)
		{
			adapter = adp;
			break;
		}
	}

	// -------------------- swapchain, device, deviceContext の作成 ---------------------
	
	// swapchainの情報
	DXGI_SWAP_CHAIN_DESC scd = { 0 };	// 作成に使用するswapchainの説明
	ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

	scd.BufferDesc.Width = windowWidth;		// 解像度の幅
	scd.BufferDesc.Height = windowHeight;		// 解像度の高さ
	scd.BufferDesc.RefreshRate.Numerator = 60;	// リフレッシュレートの分子(違うかも)
	scd.BufferDesc.RefreshRate.Denominator = 1;	// リフレッシュレートの分母(違うかも)
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	scd.SampleDesc.Count = 1;
	scd.SampleDesc.Quality = 0;

	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = 1;
	scd.OutputWindow = hwnd;	// 出力ウィンドウのハンドル
	scd.Windowed = TRUE;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;	// Presentを呼び出した後にバックバッファーを破棄する設定
	scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;	// ウィンドウモードから全画面モードに切り替えた時に表示モード(または解像度)を変更する

	HRESULT hr;
	hr = D3D11CreateDeviceAndSwapChain(
		adapter.pAdapter,		// IDXGI Adapter
		D3D_DRIVER_TYPE_UNKNOWN,
		NULL,
		NULL,
		NULL,
		0,
		D3D11_SDK_VERSION,					// SDKのバージョン。この値固定
		&scd,								// swapchainの情報
		this->swapchain.GetAddressOf(),		// IDXGISwapChainのアドレス
		this->device.GetAddressOf(),		// ID3D11Deviceのアドレス
		NULL,
		this->deviceContext.GetAddressOf()	// ID3D11DeviceContextのアドレス
	);
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

	// ---------------------------- renderTargetView の作成 -----------------------------

	// renderTargetのバッファーを取得
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	hr = this->swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

	hr = this->device->CreateRenderTargetView(backBuffer.Get(), NULL, this->renderTargetView.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

	// ---------------------------- DepthStencilView の作成 -----------------------------

	// DepthStencilBufferの作成(バッファとして使用するがテクスチャの設定を行う)
	ID3D11Texture2D* depthStencilBuffer = {};
	D3D11_TEXTURE2D_DESC texture2dDesc = {};
	texture2dDesc.Width = windowWidth;
	texture2dDesc.Height = windowHeight;
	texture2dDesc.MipLevels = 1;
	texture2dDesc.ArraySize = 1;
	texture2dDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	texture2dDesc.SampleDesc.Count = 1;
	texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
	texture2dDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	texture2dDesc.CPUAccessFlags = 0;
	texture2dDesc.MiscFlags = 0;
	hr = this->device->CreateTexture2D(&texture2dDesc, NULL, &depthStencilBuffer);
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

	// ---------------------------- depthStencilView の作成 -----------------------------
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
	depthStencilViewDesc.Format = texture2dDesc.Format;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;
	hr = this->device->CreateDepthStencilView(depthStencilBuffer, &depthStencilViewDesc, depthStencilView.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

	depthStencilBuffer->Release();

	// ---------------------- depthStencilState オブジェクトの生成 ----------------------
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	// 深度テスト:ON	深度ライト:ON
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	hr = device->CreateDepthStencilState(&depthStencilDesc, depthStencilStates[static_cast<size_t>(DEPTHSTENCIL_STATE::ZT_ON_ZW_ON)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// 深度テスト:ON	深度ライト:OFF
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	hr = device->CreateDepthStencilState(&depthStencilDesc, depthStencilStates[static_cast<size_t>(DEPTHSTENCIL_STATE::ZT_ON_ZW_OFF)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// 深度テスト:OFF	深度ライト:ON
	depthStencilDesc.DepthEnable = FALSE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	hr = device->CreateDepthStencilState(&depthStencilDesc, depthStencilStates[static_cast<size_t>(DEPTHSTENCIL_STATE::ZT_OFF_ZW_ON)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// 深度テスト:OFF	深度ライト:OFF
	depthStencilDesc.DepthEnable = FALSE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	hr = device->CreateDepthStencilState(&depthStencilDesc, depthStencilStates[static_cast<size_t>(DEPTHSTENCIL_STATE::ZT_OFF_ZW_OFF)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));


	// -------------------------------- viewport の作成 ---------------------------------

	// renderTargetViewのどの範囲を使用するかを決める
	CD3D11_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight));

	// viewportの設定(Rasterizerの設定)
	this->deviceContext->RSSetViewports(1, &viewport);


	// ------------------------------- rasterizer の作成 --------------------------------
	D3D11_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.DepthBias = 0;
	rasterizerDesc.DepthBiasClamp = 0;
	rasterizerDesc.SlopeScaledDepthBias = 0;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.ScissorEnable = FALSE;
	rasterizerDesc.MultisampleEnable = FALSE;
	// 正面の定義：半時計周り	 描画する三角形：正面向き		塗りつぶしモード：塗りつぶし
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.AntialiasedLineEnable = FALSE;
	hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerStates[static_cast<size_t>(RASTERIZER_STATE::CLOCK_FALSE_SOLID)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// 正面の定義：半時計周り	 描画する三角形：正面向き		塗りつぶしモード：ワイヤーフレーム
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.AntialiasedLineEnable = TRUE;
	hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerStates[static_cast<size_t>(RASTERIZER_STATE::CLOCK_FALSE_WIREFRAME)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// 正面の定義：半時計周り	 描画する三角形：全て		塗りつぶしモード：塗りつぶし
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.AntialiasedLineEnable = TRUE;
	hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerStates[static_cast<size_t>(RASTERIZER_STATE::CLOCK_FALSE_CULL_NONE)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// 正面の定義：半時計周り	 描画する三角形：全て		塗りつぶしモード：ワイヤーフレーム
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.AntialiasedLineEnable = TRUE;
	hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerStates[static_cast<size_t>(RASTERIZER_STATE::CLOCK_FALSE_WIREFRAME_CULL_NONE)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// 正面の定義：時計周り	 描画する三角形：正面向き		塗りつぶしモード：塗りつぶし
	rasterizerDesc.FrontCounterClockwise = TRUE;
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.AntialiasedLineEnable = FALSE;
	hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerStates[static_cast<size_t>(RASTERIZER_STATE::CLOCK_TRUE_SOLID)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// 正面の定義：時計周り	 描画する三角形：正面向き		塗りつぶしモード：ワイヤーフレーム
	rasterizerDesc.FrontCounterClockwise = TRUE;
	rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.AntialiasedLineEnable = TRUE;
	hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerStates[static_cast<size_t>(RASTERIZER_STATE::CLOCK_TRUE_WIREFRAME)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// 正面の定義：時計周り	 描画する三角形：全て		塗りつぶしモード：塗りつぶし
	rasterizerDesc.FrontCounterClockwise = TRUE;
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.AntialiasedLineEnable = TRUE;
	hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerStates[static_cast<size_t>(RASTERIZER_STATE::CLOCK_TRUE_CULL_NONE)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// 正面の定義：時計周り	 描画する三角形：全て		塗りつぶしモード：ワイヤーフレーム
	rasterizerDesc.FrontCounterClockwise = TRUE;
	rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.AntialiasedLineEnable = TRUE;
	hr = device->CreateRasterizerState(&rasterizerDesc, rasterizerStates[static_cast<size_t>(RASTERIZER_STATE::CLOCK_TRUE_WIREFRAME_CULL_NONE)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

	// ------------------------------- Blend State の作成 -------------------------------
	D3D11_BLEND_DESC blendDesc = { 0 };
	// none
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = this->device->CreateBlendState(&blendDesc, blendStates[static_cast<size_t>(BLEND_STATE::NONE)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// alpha
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = this->device->CreateBlendState(&blendDesc, blendStates[static_cast<size_t>(BLEND_STATE::ALPHA)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// add
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&blendDesc, blendStates[static_cast<size_t>(BLEND_STATE::ADD)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// multiply
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&blendDesc, blendStates[static_cast<size_t>(BLEND_STATE::MULTIPLY)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

	// particleAlpha(Alpha-To-Coverageを使う)　		今後も使うなら要改善
	//blendDesc.AlphaToCoverageEnable = TRUE;
	//blendDesc.IndependentBlendEnable = FALSE;
	//blendDesc.RenderTarget[0].BlendEnable = TRUE;
	//blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	//blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	//blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	//blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	//blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	//blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	//blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blendDesc.IndependentBlendEnable = false;
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = this->device->CreateBlendState(&blendDesc, blendStates[static_cast<size_t>(BLEND_STATE::PARTICLE_APLHA)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

	// ------------------------------- SamplerStateの作成 -------------------------------
	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0;
	samplerDesc.MaxAnisotropy = 16;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	// ポイントサンプリング
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::POINT)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// 線形補間
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// 異方性補間
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::ANISOTROPIC)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// テキスト用
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.MaxAnisotropy = 1;
	hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::TEXT_LINEAR)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// LINEAR_BORDER_BLACK
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_BLACK)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// LINEAR_BORDER_WHITE
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.BorderColor[0] = FLT_MAX;
	samplerDesc.BorderColor[1] = FLT_MAX;
	samplerDesc.BorderColor[2] = FLT_MAX;
	samplerDesc.BorderColor[3] = FLT_MAX;
	hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_WHITE)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));
	// SHADOWMAP
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.BorderColor[0] = 1;
	samplerDesc.BorderColor[1] = 1;
	samplerDesc.BorderColor[2] = 1;
	samplerDesc.BorderColor[3] = 1;
	hr = device->CreateSamplerState(&samplerDesc, samplerStates[static_cast<size_t>(SAMPLER_STATE::SHADOW)].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

	// ----------------------------- ConstantBuffer の作成 ------------------------------
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(ColorFilter);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;
	hr = device->CreateBuffer(&bufferDesc, nullptr, constantBuffers[2].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

	bufferDesc.ByteWidth = sizeof(ShadowMapData);
	hr = device->CreateBuffer(&bufferDesc, nullptr, constantBuffers[5].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));


	// ----- FrameBuffer の作成 -----
	frameBuffers[0] = std::make_unique<FrameBuffer>(windowWidth, windowHeight);		// 通常描画
	frameBuffers[1] = std::make_unique<FrameBuffer>(windowWidth, windowHeight);		// 高輝度抽出
	frameBuffers[2] = std::make_unique<FrameBuffer>(windowWidth/2, windowHeight);	// 横ブラー用
	frameBuffers[3] = std::make_unique<FrameBuffer>(windowWidth/2, windowHeight/2);	// 縦ブラー用
	frameBuffers[4] = std::make_unique<FrameBuffer>(windowWidth, windowHeight);		// カラーフィルター
	frameBuffers[5] = std::make_unique<FrameBuffer>(windowWidth, windowHeight);		// 最終出力
	
	// ----- FullScreenQuad の作成 -----
	bitBlockTransfer = std::make_unique<FullScreenQuad>();

	// samplerStateの設定
	deviceContext->PSSetSamplers(_pointSampler, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::POINT)].GetAddressOf());
	deviceContext->PSSetSamplers(_linearSampler, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR)].GetAddressOf());
	deviceContext->PSSetSamplers(_anisotropicSampler, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::ANISOTROPIC)].GetAddressOf());
	deviceContext->PSSetSamplers(_textLinarSampler, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::TEXT_LINEAR)].GetAddressOf());
	deviceContext->PSSetSamplers(_linearBorderBlackSampler, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_BLACK)].GetAddressOf());
	deviceContext->PSSetSamplers(_linearBorderWhiteSampler, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::LINEAR_BORDER_WHITE)].GetAddressOf());
	deviceContext->PSSetSamplers(_shadowSampler, 1, samplerStates[static_cast<size_t>(SAMPLER_STATE::SHADOW)].GetAddressOf());
}

// バッファ切り替え
void Graphics::Present()
{
	swapchain->Present(sync ? 1 : 0, NULL);
}



//--------------------------------------------------------------
//  各state設定
//--------------------------------------------------------------

// depthStencilの設定
void Graphics::SetDepthStencil(DEPTHSTENCIL_STATE state)
{
	deviceContext->OMSetDepthStencilState(depthStencilStates[static_cast<size_t>(state)].Get(), 1);
}
// rasterizerの設定
void Graphics::SetRasterizer(RASTERIZER_STATE state)
{
	deviceContext->RSSetState(rasterizerStates[static_cast<size_t>(state)].Get());
}
// blendの設定
void Graphics::SetBlend(BLEND_STATE state)
{
	deviceContext->OMSetBlendState(blendStates[static_cast<size_t>(state)].Get(), NULL, 0xFFFFFFFF);
}



