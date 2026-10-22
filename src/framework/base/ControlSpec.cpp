#include "framework/base/ControlSpec.h"

// ================================
// 文件: ControlSpec.cpp
// 描述: 控件类型约定的行为端 —— 控件数据结构的成员函数定义(接口契约见同目录 ControlSpec.md)
// ================================

// ================================
// 区域: 控件数据类型谓词
// ================================

// 函数: IsKnownType
// 描述: 判断渲染层是否认识本控件类型(未知类型与未使用槽位都不认识)
BOOL CtrlInfo::IsKnownType() const
{
	return (type != CTRL_TYPE_INVALID && type != CTRL_TYPE_UNKNOWN) ? TRUE : FALSE;
}
