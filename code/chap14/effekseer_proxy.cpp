#include "effekseer_proxy.h"

#if NDEBUG
#pragma comment(lib, "EffekseerRendererDX12.lib")
#pragma comment(lib, "Effekseer.lib")
#pragma comment(lib, "LLGI.lib")
#else
#pragma comment(lib, "EffekseerRendererDX12d.lib")
#pragma comment(lib, "Effekseerd.lib")
#pragma comment(lib, "LLGId.lib")
#endif // NDEBUG

HRESULT EffekseerProxy::init()
{
	return S_OK;
}
