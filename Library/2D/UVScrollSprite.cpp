#include <sstream>
#include "UVScrollSprite.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Shader.h"
#include "../ErrorLogger.h"


// 回転
inline void  Rotate(float& x, float& y, float cx, float cy, float cos, float sin)
{
	x -= cx;
	y -= cy;

	float tx{ x }, ty{ y };
	x = cos * tx + -sin * ty;
	y = sin * tx + cos * ty;

	x += cx;
	y += cy;
};

UVScrollSprite::UVScrollSprite(const wchar_t* filename)
{
	Graphics* gfx = &Graphics::Instance();
	ID3D11Device* device = gfx->GetDevice();

	//--- < 頂点情報のセット > ---
	vertex vertices[] =
	{
		{{ -0.5f,  0.5f, 0 }, { 1, 1, 1, 1 }, { 0, 0 }},
		{{  0.5f,  0.5f, 0 }, { 1, 0, 0, 1 }, { 1, 0 }},
		{{ -0.5f, -0.5f, 0 }, { 0, 1, 0, 1 }, { 0, 1 }},
		{{  0.5f, -0.5f, 0 }, { 0, 0, 1, 1 }, { 1, 1 }},
	};

	//--- < 頂点バッファオブジェクトの生成 > ---
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(vertices);
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA subresourceData{};
	subresourceData.pSysMem = vertices;
	subresourceData.SysMemPitch = 0;
	subresourceData.SysMemSlicePitch = 0;
	HRESULT hr = device->CreateBuffer(&bufferDesc, &subresourceData, vertexBuffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

	//--- < 頂点シェーダーオブジェクトと入力レイアウトオブジェクトの生成 > ---
	const char* csoName{ "./Data/Shader/UVScroll_VS.cso" };

	D3D11_INPUT_ELEMENT_DESC inputElementDesc[]
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	CreateVsFromCso(csoName, vertexShader.GetAddressOf(), inputLayout.GetAddressOf(), inputElementDesc, _countof(inputElementDesc));

	//--- < ピクセルシェーダーオブジェクトの生成 > ---
	csoName = { "./Data/Shader/UVScroll_PS.cso" };
	CreatePsFromCso(csoName, pixelShader.GetAddressOf());

	//--- < 画像ファイルのロードとshaderResourceViewの生成とテクスチャ情報の取得 > ---
	LoadTextureFromFile(filename, shaderResourceView.GetAddressOf(), &texture2dDesc);

	//--- 定数バッファオブジェクトの生成 ---
	D3D11_BUFFER_DESC constantBufferDesc{};
	constantBufferDesc.ByteWidth = sizeof(Constants);
	constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&constantBufferDesc, nullptr, constantBuffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

}

UVScrollSprite::~UVScrollSprite()
{
}

void UVScrollSprite::Render(DirectX::XMFLOAT2 uvScrollValue, float dx, float dy, float dw, float dh, float r, float g, float b, float a, float angle)
{
	Render(uvScrollValue, dx, dy, dw, dh, r, g, b, a, angle, 0, 0, texture2dDesc.Width, texture2dDesc.Height);
}

void UVScrollSprite::Render(DirectX::XMFLOAT2 uvScrollValue, float dx, float dy, float dw, float dh, float r, float g, float b, float a, float angle, float sx, float sy, float sw, float sh)
{

	Graphics* gfx = &Graphics::Instance();
	ID3D11DeviceContext* dc = gfx->GetDeviceContext();

	//--- < スクリーン(ビューポート)のサイズを取得 > ---
	D3D11_VIEWPORT viewport{};
	UINT numViewports{ 1 };
	dc->RSGetViewports(&numViewports, &viewport);

	//--- < 矩形の各頂点の位置(スクリーン座標系)を計算する > ---

	// left top
	float x0{ dx };
	float y0{ dy };
	// right top
	float x1{ dx + dw };
	float y1{ dy };
	// left bottom
	float x2{ dx };
	float y2{ dy + dh };
	// right bottom
	float x3{ dx + dw };
	float y3{ dy + dh };

	//--- < 頂点回転処理 > ---

	// 回転の中心を矩形の中心点にした場合
	float cx = dx + dw * 0.5f;
	float cy = dy + dh * 0.5f;

	float cos{ cosf(DirectX::XMConvertToRadians(angle)) };
	float sin{ sinf(DirectX::XMConvertToRadians(angle)) };

	Rotate(x0, y0, cx, cy, cos, sin);
	Rotate(x1, y1, cx, cy, cos, sin);
	Rotate(x2, y2, cx, cy, cos, sin);
	Rotate(x3, y3, cx, cy, cos, sin);


	//--- < スクリーン座標系からNDCへ座標変換 > ---
	x0 = 2.0f * x0 / viewport.Width - 1.0f;
	y0 = 1.0f - 2.0f * y0 / viewport.Height;
	x1 = 2.0f * x1 / viewport.Width - 1.0f;
	y1 = 1.0f - 2.0f * y1 / viewport.Height;
	x2 = 2.0f * x2 / viewport.Width - 1.0f;
	y2 = 1.0f - 2.0f * y2 / viewport.Height;
	x3 = 2.0f * x3 / viewport.Width - 1.0f;
	y3 = 1.0f - 2.0f * y3 / viewport.Height;


	//--- < テクセル座標系からUV座標系への変換 > ---
	float u0{ sx / texture2dDesc.Width };
	float v0{ sy / texture2dDesc.Height };
	float u1{ (sx + sw) / texture2dDesc.Width };
	float v1{ (sy + sh) / texture2dDesc.Height };

	//--- < 計算結果で頂点バッファオブジェクトを更新する > ---
	HRESULT hr{ S_OK };
	D3D11_MAPPED_SUBRESOURCE mappedSubresource{};
	hr = dc->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
	_ASSERT_EXPR(SUCCEEDED(hr), hrTrace(hr));

	vertex* vertices{ reinterpret_cast<vertex*>(mappedSubresource.pData) };
	if (vertices != nullptr)
	{
		vertices[0].position = { x0 ,y0, 0 };
		vertices[1].position = { x1 ,y1, 0 };
		vertices[2].position = { x2 ,y2, 0 };
		vertices[3].position = { x3 ,y3, 0 };
		vertices[0].color = vertices[1].color = vertices[2].color = vertices[3].color = { r,g,b,a };

		vertices[0].texcoord = { u0,v0 };
		vertices[1].texcoord = { u1,v0 };
		vertices[2].texcoord = { u0,v1 };
		vertices[3].texcoord = { u1,v1 };
	}
	dc->Unmap(vertexBuffer.Get(), 0);

	//--- < 頂点バッファーのバインド > ---
	UINT stride{ sizeof(vertex) };
	UINT offset{ 0 };
	dc->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);

	//--- < プリミティブタイプおよにデータ順序に関する情報のバインド > ---
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//--- < 入力レイアウトオブジェクトのバインド > ---
	dc->IASetInputLayout(inputLayout.Get());

	//--- < シェーダーのバインド > ---
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);

	//--- < シェーダーリソースのバインド > ---
	dc->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());

	// --- < 定数バッファの更新バインド > ---
	Constants data = { uvScrollValue };
	dc->UpdateSubresource(constantBuffer.Get(), 0, 0, &data, 0, 0);
	dc->VSSetConstantBuffers(7, 1, constantBuffer.GetAddressOf());

	//--- < プリミティブの描画 > ---
	dc->Draw(4, 0);
}
