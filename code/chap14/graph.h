#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <DirectXMath.h>
#include <Windows.h>
#include <array>
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)

class RenderGraph
{
public:
	RenderGraph();
	HRESULT init();
	void set(float val);
	void update();
	HRESULT render(ID3D12GraphicsCommandList* list, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);

private:
	struct Vertex {
		DirectX::XMFLOAT3 pos = { };
	};

	static constexpr size_t kNumElements = 60;
	static constexpr size_t kNumMaxVertices = 256;

	HRESULT compileShaders();
	HRESULT createVertexBuffer();
	HRESULT createPipelineState();
	void uploadVertices(const std::array<Vertex, kNumMaxVertices> vertices);

	std::array<float, kNumElements> m_dataArray = { };
	size_t m_wrIdx = 0;
	Microsoft::WRL::ComPtr<ID3DBlob> m_vs = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_ps = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = { };
};

