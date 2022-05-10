#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <array>
#include <d3d12.h>
#include <Windows.h>
#include <wrl.h>
#pragma warning(pop)

class Shadow {
public:
	enum class Type
	{
		kQuadR,
		kQuadRgba,
		kEnd,
	};

	HRESULT init();
	void pushRenderCommand(Type type, ID3D12Resource* resource, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
	HRESULT render(ID3D12GraphicsCommandList* pList, D3D12_CPU_DESCRIPTOR_HANDLE dstRt);

private:
	enum class TypeInternal
	{
		kFrameLine = static_cast<int32_t>(Type::kEnd),
		kEnd,
	};

	enum class MeshType
	{
		kQuad,
		kFrameLine,
		kEnd,
	};

	enum class RootParamIndex
	{
		kSrvR,
		kSrvRgba,
	};

	struct Command {
		Type m_type = Type::kQuadR;
		ID3D12Resource* m_resource = nullptr;
		D3D12_VIEWPORT m_viewport = { };
		D3D12_RECT m_scissorRect = { };
	};

	static constexpr size_t kMaxNumCommand = 8;
    static constexpr UINT kMaxNumDescriptor = kMaxNumCommand;

	HRESULT compileShaders();
	HRESULT createVertexBuffer();
	HRESULT createPipelineState();
	HRESULT createDescriptorHeap();
	void setupSrv();

	Microsoft::WRL::ComPtr<ID3DBlob> m_commonVs = nullptr;
	std::array<Microsoft::WRL::ComPtr<ID3DBlob>, static_cast<size_t>(TypeInternal::kEnd)> m_psArray = { };
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, static_cast<size_t>(TypeInternal::kEnd)> m_pipelineStates = { };
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, static_cast<size_t>(MeshType::kEnd)> m_vbResources = { };
	std::array<D3D12_VERTEX_BUFFER_VIEW, static_cast<size_t>(MeshType::kEnd)> m_vbViews = { };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvDescHeap = nullptr;
	std::array<Command, kMaxNumCommand> m_commands = { };
	size_t m_numCommand = 0;
};

