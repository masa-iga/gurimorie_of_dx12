#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3d12.h>
#include <DirectXMath.h>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <wrl.h>
#pragma warning(pop)

#pragma pack(1) // size of the struct is 38 bytes, so need to prevent padding
struct PMDVertex
{
	DirectX::XMFLOAT3 pos = { };
	DirectX::XMFLOAT3 normal = { };
	DirectX::XMFLOAT2 uv = { };
	UINT16 boneNo[2] = { };
	UINT8 boneWeight = 0;
	UINT8 edgeFlag = 0;
};
static_assert(sizeof(PMDVertex) == 38);
#pragma pack()

struct MaterialForHlsl
{
	DirectX::XMFLOAT3 diffuse = { };
	float alpha = 0.0f;
	DirectX::XMFLOAT3 specular = { };
	float specularity = 0.0f;
	DirectX::XMFLOAT3 ambient = { };
};

struct AdditionalMaterial
{
	std::string texPath;
	INT toonIdx = 0;
	bool edgeFlg = false;
};

struct Material
{
	UINT indicesNum = 0;
	MaterialForHlsl material;
	AdditionalMaterial additional;
};

struct BoneNode
{
	int32_t boneIdx = 0;
	DirectX::XMFLOAT3 startPos = { };
	DirectX::XMFLOAT3 endPos = { };
	std::vector<BoneNode*> children;
};

struct Motion
{
	uint32_t frameNo = 0;
	DirectX::XMVECTOR quaternion;

	Motion(uint32_t fno, const DirectX::XMVECTOR& q)
		: frameNo(fno)
		, quaternion(q)
	{ }
};

class PmdActor {
public:
	enum class Model;
	static void release();
	static std::pair<const D3D12_INPUT_ELEMENT_DESC*, UINT> getInputElementDesc();
	static ID3D12PipelineState* getPipelineState();
	static ID3D12RootSignature* getRootSignature();
	static D3D12_PRIMITIVE_TOPOLOGY getPrimitiveTopology();

	PmdActor();
	HRESULT loadAsset(Model model);
	void enableAnimation(bool enable);
	void update(bool animationReversed);
	HRESULT render(ID3D12DescriptorHeap* sceneDescHeap) const;

private:
	static HRESULT loadShaders();
	static HRESULT createRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>* rootSignature);
	static HRESULT createPipelineState();

	HRESULT loadPmd(Model model);
	HRESULT loadVmd();
	Microsoft::WRL::ComPtr<ID3D12Resource> loadTextureFromFile(const std::string& texPath);
	HRESULT createResources();
	HRESULT createWhiteTexture();
	HRESULT createBlackTexture();
	HRESULT createGrayGradiationTexture();
	HRESULT createDebugResources();
	HRESULT createTransformResource();
	HRESULT createMaterialResrouces();
	void updateMotion();
	void recursiveMatrixMultiply(const BoneNode& node, const DirectX::XMMATRIX& mat);

	static Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	static Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	static Microsoft::WRL::ComPtr<ID3DBlob> m_vsBlob;
	static Microsoft::WRL::ComPtr<ID3DBlob> m_psBlob;

	bool m_bAnimation = false;
	DWORD m_animationStartTime = 0;
	std::vector<PMDVertex> m_vertices;
	std::vector<UINT16> m_indices;
	std::vector<Material> m_materials;
	UINT m_vertNum = 0;
	UINT m_indicesNum = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	Microsoft::WRL::ComPtr<ID3D12Resource> m_ibResource = nullptr;
	D3D12_INDEX_BUFFER_VIEW m_ibView = { };
	Microsoft::WRL::ComPtr<ID3D12Resource> m_materialResource = nullptr;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_toonResources;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_textureResources;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_sphResources;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_spaResources;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_materialDescHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_whiteTextureResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_blackTextureResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_grayGradiationTextureResource = nullptr;
	std::map<std::string, ID3D12Resource*> m_resourceTable;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_transformDescHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_transformResource = nullptr;
	DirectX::XMMATRIX* m_worldMatrixPointer = nullptr; // needs to be aligned 16 bytes
	DirectX::XMMATRIX* m_boneMatrixPointer = nullptr;
	uint32_t m_duration = 0;
	std::map<std::string, BoneNode> m_boneNodeTable;
	std::vector<DirectX::XMMATRIX> m_boneMatrices;
	std::unordered_map<std::string, std::vector<Motion>> m_motionData;

	D3D12_VERTEX_BUFFER_VIEW m_debugVbView = { };
	D3D12_INDEX_BUFFER_VIEW m_debugIbView = { };
};

enum class PmdActor::Model
{
	kMiku,
	kMikuMetal,
	kLuka,
	kLen,
	kKaito,
	kHaku,
	kRin,
	kMeiko,
	kNeru,
};

