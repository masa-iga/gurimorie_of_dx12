#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <DirectXMath.h>
#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)

class Toolkit
{
public:
	Toolkit() = default;
	HRESULT init();
	void teardown();
	HRESULT drawClear(ID3D12GraphicsCommandList* list, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);

private:
	struct Vertex {
		DirectX::XMFLOAT3 pos = { };
	};

	static constexpr size_t kNumVertices = 3;
	static constexpr size_t kVertexBufferSize = kNumVertices * sizeof(Vertex);

	HRESULT compileShaders();
	HRESULT createVertexBuffer();
	HRESULT createPipelineState();
	HRESULT uploadVertices();

	Microsoft::WRL::ComPtr<ID3DBlob> m_vs = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_ps = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = { };
};
