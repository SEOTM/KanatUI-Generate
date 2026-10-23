// ================================
// 文件: ResourceLoader.cpp
// 描述: 资源与路径加载原语的实现(口径、分层与迁出动因见 ResourceLoader.h 的文件头说明)
// ================================

// ================================
// 区域: 文件包含区
// ================================
#include "framework/dispatch/ResourceLoader.h"
#include "framework/base/mem/AllocatorHub.h"

// ================================
// 区域: 文件内静态状态
// ================================
static LPCSTR sExeDir = NULL;		// exe 目录(池内串; 含末尾 '\'; 未初始化 ⇒ NULL)

// ================================
// 区域: 接口实现区
// ================================
// 函数: Init
// 描述: 缓存 exe 所在目录(幂等; 失败时目录保持空)
VOID ResourceLoader::Init()
{
	if (sExeDir != NULL)
	{
		return;				// 幂等: 已缓存
	}
	CHAR szExe[MAX_PATH]{};
	DWORD dwLen = GetModuleFileNameA(NULL, szExe, MAX_PATH);
	if (dwLen > 0 && dwLen < MAX_PATH)
	{
		DWORD dwPos = dwLen;
		while (dwPos > 0 && szExe[dwPos - 1] != '\\')
		{
			dwPos--;
		}
		CHAR szDir[MAX_PATH]{};
		lstrcpynA(szDir, szExe, (INT)(dwPos + 1));
		sExeDir = AllocatorHub::CopyString(szDir);
	}
}
// 函数: GetFilePath
// 描述: 拼接 exe 目录与文件名(池内新串; 每次调用新分配)
LPCSTR ResourceLoader::GetFilePath(LPCSTR filename)
{
	LPCSTR ret = NULL;
	if (sExeDir != NULL && filename != NULL)
	{
		CHAR szFull[MAX_PATH * 2]{};
		lstrcpyA(szFull, sExeDir);
		lstrcatA(szFull, filename);
		ret = AllocatorHub::CopyString(szFull);
	}
	return ret;
}
// 函数: FileExists
// 描述: 普通文件是否存在(加载链的"外部文件"级别判据)
BOOL ResourceLoader::FileExists(LPCSTR path)
{
	if (path == NULL)
	{
		return FALSE;
	}
	return (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) ? TRUE : FALSE;
}
