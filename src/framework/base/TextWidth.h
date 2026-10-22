#pragma once
/*
 * 计算文本在等宽终端下的显示宽度（列数）。
 *
 * text: 以 '\0' 结尾的字符串；为 nullptr 返回 0。
 *
 * mode: 编码模式(大小写不敏感)，目前支持：
 *         "utf-8" / "utf8" / nullptr   按 UTF-8 解码；
 *                                      宽字符计 2，组合符/零宽字符/控制符计 0，
 *                                      其余可打印字符计 1；
 *         其它值                       按单字节 ASCII 处理：
 *                                      控制字符计 0，其余计 1。
 *       默认为 "utf-8"。
 *
 * 返回值：显示宽度（列数）。非法 UTF-8 字节按 1 列计。
 */

// 函数: CalcTextWidth
// 描述: 计算文本在等宽终端下的显示宽度（列数）。
int CalcTextWidth(const char* text, const char* mode = "utf-8");
