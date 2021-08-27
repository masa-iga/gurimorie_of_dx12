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

enum class BoneType
{
	kRotation,
	kRotAndMove,
	kIk,
	kUndefined,
	kIkChild,
	kRotationChild,
	kIkDestination,
	kInvisible,
};

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
	uint32_t boneType = 0;
	uint32_t ikParentBone = 0;
	DirectX::XMFLOAT3 startPos = { };
	DirectX::XMFLOAT3 endPos = { };
	std::vector<BoneNode*> children;
};

struct Motion
{
	uint32_t frameNo = 0;
	DirectX::XMVECTOR quaternion = { };
	DirectX::XMFLOAT3 offset = { };
	DirectX::XMFLOAT2 p1 = { }; // Bezier curve control point 0
	DirectX::XMFLOAT2 p2 = { }; // Bezier curve control point 1

	Motion(uint32_t fno,
		const DirectX::XMVECTOR& q,
		const DirectX::XMFLOAT3& ofst,
		const DirectX::XMFLOAT2& ip1,
		const DirectX::XMFLOAT2& ip2)
		: frameNo(fno)
		, quaternion(q)
		, offset(ofst)
		, p1(ip1)
		, p2(ip2)
	{ }
};

struct VMDIkEnable
{
	uint32_t frameNo = 0;
	std::unordered_map<std::string, bool> ikEnableTable;
};

struct PmdIk
{
	uint16_t boneIdx = 0;
	uint16_t targetIdx = 0;
	//uint8_t chainLen = 0;
	uint16_t iterations = 0;
	float limit = 0.0f;
	std::vector<uint16_t> nodeIdxes;
};

class PmdActor {
public:
	enum class Model;
	static void release();
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
	HRESULT createResources();
	HRESULT createWhiteTexture();
	HRESULT createBlackTexture();
	HRESULT createGrayGradiationTexture();
	HRESULT createDebugResources();
	HRESULT createTransformResource();
	HRESULT createMaterialResrouces();
	void updateMotion();
	void recursiveMatrixMultiply(const BoneNode& node, const DirectX::XMMATRIX& mat);
	void IKSolve(uint32_t frameNo);
	void solveLookAt(const PmdIk& ik);
	void solveCosineIK(const PmdIk& ik);
	void solveCCDIK(const PmdIk& ik);

	static Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	static Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	static Microsoft::WRL::ComPtr<ID3D12PipelineState> m_shadowPipelineState;
	static Microsoft::WRL::ComPtr<ID3DBlob> m_vsBlob;
	static Microsoft::WRL::ComPtr<ID3DBlob> m_psBlob;
	static Microsoft::WRL::ComPtr<ID3DBlob> m_shadowVsBlob;

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
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_transformDescHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_transformResource = nullptr;
	DirectX::XMMATRIX* m_worldMatrixPointer = nullptr; // needs to be aligned 16 bytes
	DirectX::XMMATRIX* m_boneMatrixPointer = nullptr;
	uint32_t m_duration = 0;
	std::map<std::string, BoneNode> m_boneNodeTable;
	std::vector<std::string> m_boneNameArray;
	std::vector<BoneNode*> m_boneNodeAddressArray;
	std::vector<uint32_t> m_kneeIdxes;
	std::vector<DirectX::XMMATRIX> m_boneMatrices;
	std::unordered_map<std::string, std::vector<Motion>> m_motionData;
	std::vector<PmdIk> m_pmdIks;
	std::vector<VMDIkEnable> m_ikEnableData;

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

