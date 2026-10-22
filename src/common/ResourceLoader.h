// ================================
// 文件: ResourceLoader.h
// 描述: 资源与路径加载原语 —— exe 目录解析、路径拼接与"随 exe 内置的资源节"读取。
// ================================
#pragma once
#include <windows.h>

// ================================
// 命名空间: ResourceLoader
// 描述: 资源与路径加载原语(进程级单例语义)
// ================================
namespace ResourceLoader
{
	// 函数: Init
	// 描述: 缓存 exe 所在目录(含末尾 '\'; 应用启动期调用一次; 幂等)
	// 说明: 取 GetModuleFileNameA 的目录部分; 失败时目录保持空(此时 GetFilePath 返回 NULL)。
	VOID Init();

	// 函数: GetExeDir
	// 描述: 取缓存的 exe 目录(含末尾 '\'; 未初始化/失败 ⇒ NULL)
	LPCSTR GetExeDir();

	// 函数: GetFilePath
	// 描述: 拼接 exe 目录与文件名(内存池字符串; 每次调用新分配)
	// 返回: 目录未就绪或入参为 NULL ⇒ NULL
	LPCSTR GetFilePath(LPCSTR filename);

	// 函数: FileExists
	// 描述: 普通文件是否存在(加载链的"外部文件"级别判据)
	BOOL FileExists(LPCSTR path);

	// 函数: LoadResource
	// 描述: 读取随 exe 内置的资源节字节并拷进内存池(末尾补一位 '\0')
	// 说明: 资源字节不保证以 '\0' 结尾, 而 JSON 解析需要 NUL ⇒ 统一补一位;
	//   补的字节不计入 pcbData(bin 解析按显式长度, 不受影响)。
	// 参数:
	//		IN	dwResId:	资源 ID(由调用方定义)
	//		IN	szResType:	资源类型串(如 "JSON" / "BIN")
	//		OUT	ppData:		数据首址(内存池副本; 失败置 NULL)
	//		OUT	pcbData:	字节数(不含补的 '\0')
	// 返回:
	//		BOOL: TRUE = 命中(ppData/pcbData 有效)
	BOOL LoadResource(DWORD dwResId, LPCSTR szResType, LPVOID* ppData, DWORD* pcbData);
}
