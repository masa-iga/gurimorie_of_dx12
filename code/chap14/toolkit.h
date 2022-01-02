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

#define HAVE_RECT_SHADER (1)

class Toolkit
{
public:
	struct Vertex {
		DirectX::XMFLOAT3 pos = { };
	};

	Toolkit() = default;
	HRESULT init();
	void teardown();
	HRESULT drawClear(ID3D12GraphicsCommandList* list, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	HRESULT drawClearBlend(ID3D12GraphicsCommandList* list, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	HRESULT drawRect(ID3D12GraphicsCommandList* list, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);

private:
	struct OutputColor {
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 0.0f;
	};

	enum class DrawType {
		kClear,
		kRect,
		kEnd,
	};

	static constexpr std::array<size_t, static_cast<size_t>(DrawType::kEnd)> kNumVertices = {
		3,
		5,
	};
	static constexpr std::array<size_t, static_cast<size_t>(DrawType::kEnd)> kVertexBufferSizes = {
		sizeof(Vertex) * kNumVertices.at(static_cast<size_t>(DrawType::kClear)),
		sizeof(Vertex) * kNumVertices.at(static_cast<size_t>(DrawType::kRect)),
	};

	std::array<OutputColor, static_cast<size_t>(DrawType::kEnd)> kDefaultOutputColors = {
		OutputColor(0.2f, 0.2f, 0.2f, 0.75f),
		OutputColor(0.0f, 0.0f, 0.0f, 1.0f),
	};

	HRESULT compileShaders();
	HRESULT createVertexBuffer();
	HRESULT createConstantBuffer();
	HRESULT createPipelineState();
	HRESULT uploadVertices();
	HRESULT uploadOutputColor(DrawType type, OutputColor outputColor);
	HRESULT drawClearInternal(ID3D12GraphicsCommandList* list, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect, bool blend);

	std::array<Microsoft::WRL::ComPtr<ID3DBlob>, static_cast<uint32_t>(DrawType::kEnd)> m_vsArray = { };
	std::array<Microsoft::WRL::ComPtr<ID3DBlob>, static_cast<uint32_t>(DrawType::kEnd)> m_psArray = { };
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, static_cast<uint32_t>(DrawType::kEnd)> m_vertexBuffers = { };
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, static_cast<uint32_t>(DrawType::kEnd)> m_constantOutputColorBuffers = { };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_constantOutputColorHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineStateBlend = nullptr;
#if HAVE_RECT_SHADER
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineStateRect = nullptr;
#endif // HAVE_RECT_SHADER
	std::array<D3D12_VERTEX_BUFFER_VIEW, static_cast<uint32_t>(DrawType::kEnd)> m_vertexBufferViews = { };
};

