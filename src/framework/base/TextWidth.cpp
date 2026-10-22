#include "framework/base/TextWidth.h"

namespace {

    // ===================== Unicode 宽度区间表 =====================

    struct Range {
        unsigned int lo;
        unsigned int hi;
    };

    // 判断码点是否落在任一区间内（区间数量由数组长度自动推导）。
    template <auto N>
    constexpr bool InRanges(unsigned int cp, const Range(&ranges)[N]) noexcept {
        for (decltype(N) i = 0; i < N; ++i) {
            if (cp >= ranges[i].lo && cp <= ranges[i].hi) {
                return true;
            }
        }
        return false;
    }

    // --------- 宽字符（东亚 W / F，宽度 2） ---------
    constexpr Range kWideRanges[] = {
        {0x1100,  0x115F},   // 谚文字母 Choseong
        {0x2329,  0x232A},   // 〈 〉
        {0x2E80,  0x303E},   // 部首扩展 / 康熙部首 / CJK 符号
        {0x3041,  0x33FF},   // 平假名 / 片假名 / 注音 / 韩文兼容 / CJK 兼容
        {0x3400,  0x4DBF},   // CJK 扩展 A
        {0x4E00,  0x9FFF},   // CJK 统一表意文字
        {0xA000,  0xA4CF},   // 彝文
        {0xA960,  0xA97F},   // 谚文字母扩展 A
        {0xAC00,  0xD7A3},   // 韩文音节
        {0xF900,  0xFAFF},   // CJK 兼容表意文字
        {0xFE10,  0xFE19},   // 竖排标点
        {0xFE30,  0xFE6F},   // CJK 兼容形式 / 小写变体
        {0xFF00,  0xFF60},   // 全角 ASCII
        {0xFFE0,  0xFFE6},   // 全角符号
        {0x16FE0, 0x16FE4},  // 表意文字符号
        {0x17000, 0x187F7},  // 西夏文
        {0x18800, 0x18CD5},  // 西夏文构件
        {0x1B000, 0x1B2FF},  // 假名补充 / 假名扩展
        {0x1F200, 0x1F2FF},  // 带圈表意文字补充
        {0x1F300, 0x1F64F},  // 杂项符号 / 表情符号
        {0x1F900, 0x1F9FF},  // 补充符号与图形
        {0x1FA00, 0x1FAFF},  // 棋类符号
        {0x20000, 0x2FFFD},  // CJK 扩展 B–F
        {0x30000, 0x3FFFD},  // CJK 扩展 G
    };

    // --------- 零宽字符（组合符 / 变体选择符 / 方向控制符 / 控制码） ---------
    constexpr Range kZeroRanges[] = {
        {0x0300,  0x036F},   // 组合附加符号
        {0x0483,  0x0489},   // 西里尔组合符
        {0x0591,  0x05BD},   // 希伯来
        {0x05BF,  0x05BF},
        {0x05C1,  0x05C2},
        {0x05C4,  0x05C5},
        {0x05C7,  0x05C7},
        {0x0610,  0x061A},   // 阿拉伯
        {0x064B,  0x065F},
        {0x0670,  0x0670},
        {0x06D6,  0x06DC},
        {0x06DF,  0x06E4},
        {0x06E7,  0x06E8},
        {0x06EA,  0x06ED},
        {0x0711,  0x0711},   // 叙利亚
        {0x0730,  0x074A},
        {0x07A6,  0x07B0},   // 它拿文
        {0x07EB,  0x07F3},
        {0x0816,  0x0819},   // 撒玛利亚
        {0x081B,  0x0823},
        {0x0825,  0x0827},
        {0x0829,  0x082D},
        {0x0859,  0x085B},   // 曼达
        {0x08E3,  0x0903},   // 阿拉伯扩展 C + 天城文
        {0x093A,  0x093A},
        {0x093C,  0x093C},
        {0x0941,  0x0948},
        {0x094D,  0x094D},
        {0x0951,  0x0957},
        {0x0962,  0x0963},
        {0x0981,  0x0981},   // 孟加拉文
        {0x09BC,  0x09BC},
        {0x09C1,  0x09C4},
        {0x09CD,  0x09CD},
        {0x09E2,  0x09E3},
        {0x0A01,  0x0A02},   // 古木基文
        {0x0A3C,  0x0A3C},
        {0x0A41,  0x0A42},
        {0x0A47,  0x0A48},
        {0x0A4B,  0x0A4D},
        {0x0A70,  0x0A71},
        {0x0B01,  0x0B01},   // 奥里亚文
        {0x0B3C,  0x0B3C},
        {0x0B3F,  0x0B3F},
        {0x0B41,  0x0B44},
        {0x0B4D,  0x0B4D},
        {0x0C3E,  0x0C40},   // 泰卢固文
        {0x0C46,  0x0C48},
        {0x0C4A,  0x0C4D},
        {0x0D41,  0x0D44},   // 马拉雅拉姆文
        {0x0D4D,  0x0D4D},
        {0x0E31,  0x0E31},   // 泰文
        {0x0E34,  0x0E3A},
        {0x0E47,  0x0E4E},
        {0x0EB1,  0x0EB1},   // 老挝文
        {0x0EB4,  0x0EBC},
        {0x0EC8,  0x0ECD},
        {0x0F18,  0x0F19},   // 藏文
        {0x0F35,  0x0F35},
        {0x0F37,  0x0F37},
        {0x0F39,  0x0F39},
        {0x0F71,  0x0F7E},
        {0x0F80,  0x0F84},
        {0x0F86,  0x0F87},
        {0x0F8D,  0x0F97},
        {0x0F99,  0x0FBC},
        {0x0FC6,  0x0FC6},
        {0x135D,  0x135F},   // 埃塞俄比亚
        {0x1AB0,  0x1AFF},   // 组合附加符号扩展
        {0x1DC0,  0x1DFF},   // 组合附加符号补充
        {0x20D0,  0x20FF},   // 组合符号用的附加标记
        {0x2CEF,  0x2CF1},   // 科普特组合符
        {0x2D7F,  0x2D7F},   // 提非纳
        {0x302A,  0x302F},   // CJK 声调标记
        {0x3099,  0x309A},   // 日文浊点
        {0xA66F,  0xA672},   // 西里尔扩展 B
        {0xA674,  0xA67D},
        {0xA69E,  0xA69F},
        {0xA6F0,  0xA6F1},   // 巴姆穆
        {0xA806,  0xA806},   // 西拉埃塞
        {0xA8C4,  0xA8C5},   // 索拉什特拉
        {0xA8E0,  0xA8F1},   // 天城文扩展
        {0xA926,  0xA92D},   // 克耶黎
        {0xA947,  0xA951},   // 雷布查
        {0xA980,  0xA982},   // 爪哇
        {0xA9B3,  0xA9B3},
        {0xA9B6,  0xA9B9},
        {0xA9BC,  0xA9BC},
        {0xA9E5,  0xA9E5},
        {0xAA29,  0xAA2E},   // 占语
        {0xAA31,  0xAA32},
        {0xAA35,  0xAA36},
        {0xAA43,  0xAA43},
        {0xAA4C,  0xAA4C},
        {0xAA7C,  0xAA7C},   // 梅泰文
        {0xAAB0,  0xAAB0},   // 傣文
        {0xAAB2,  0xAAB4},
        {0xAAB7,  0xAAB8},
        {0xAABE,  0xAABF},
        {0xAAC1,  0xAAC1},
        {0xAAEC,  0xAAED},   // 梅泰文扩展
        {0xAAF6,  0xAAF6},
        {0xABE5,  0xABE5},   // 梅泰文 Mayek
        {0xABE8,  0xABE8},
        {0xABED,  0xABED},
        {0xFB1E,  0xFB1E},   // 希伯来展示形式
        {0xFE00,  0xFE0F},   // 变体选择符
        {0xFE20,  0xFE2F},   // 组合半角标记
        {0x101FD, 0x101FD},  // 斐斯托斯圆盘
        {0x102E0, 0x102E0},  // 科普特闰日
        {0x10376, 0x1037A},  // 古波斯
        {0x10A01, 0x10A03},  // 佉卢文
        {0x10A05, 0x10A06},
        {0x10A0C, 0x10A0F},
        {0x10A38, 0x10A3A},
        {0x10A3F, 0x10A3F},
        {0x11001, 0x11001},  // 婆罗米文
        {0x11038, 0x11046},
        {0x1107F, 0x11081},
        {0x110B3, 0x110B6},
        {0x110B9, 0x110BA},
        {0x11100, 0x11102},  // 佉卢文扩展
        {0x11127, 0x1112B},
        {0x1112D, 0x11134},
        {0x11173, 0x11173},  // 迈蒂利文
        {0x11180, 0x11181},  // 夏拉达文
        {0x111B6, 0x111BE},
        {0x1122F, 0x11231},  // 可普提文
        {0x11234, 0x11234},
        {0x11236, 0x11237},
        {0x112DF, 0x112DF},  // 可普提文扩展
        {0x112E3, 0x112EA},
        {0x11300, 0x11301},  // 兰札文
        {0x1133C, 0x1133C},
        {0x11340, 0x11340},
        {0x11366, 0x1136C},
        {0x11370, 0x11374},
        {0x114B3, 0x114B8},  // 提鲁塔文
        {0x114BA, 0x114BA},
        {0x114BF, 0x114C0},
        {0x114C2, 0x114C3},
        {0x115B2, 0x115B5},  // 悉昙文
        {0x115BC, 0x115BD},
        {0x115BF, 0x115C0},
        {0x11633, 0x1163A},  // 莫迪文
        {0x1163D, 0x1163D},
        {0x1163F, 0x11640},
        {0x116AB, 0x116AB},  // 塔克里文
        {0x116AD, 0x116AD},
        {0x116B0, 0x116B5},
        {0x116B7, 0x116B7},
        {0x1171D, 0x1171F},  // 阿洪姆文
        {0x11722, 0x11725},
        {0x11727, 0x1172B},
        {0x16AF0, 0x16AF4},  // 巴萨文
        {0x16B30, 0x16B36},  // 帕哈瓦姆文
        {0x1BC9D, 0x1BC9E},  // 杜普洛扬文
        {0x1D167, 0x1D169},  // 音乐符号
        {0x1D17B, 0x1D182},
        {0x1D185, 0x1D18B},
        {0x1D1AA, 0x1D1AD},
        {0x1D242, 0x1D244},  // 古希腊音乐符号
        {0x1DA00, 0x1DA36},  // 萨顿书写符号
        {0x1DA3B, 0x1DA6C},
        {0x1DA75, 0x1DA75},
        {0x1DA84, 0x1DA84},
        {0x1DA9B, 0x1DA9F},
        {0x1DAA1, 0x1DAAF},
        {0x1E8D0, 0x1E8D6},  // 门德基卡库伊
        {0xE0100, 0xE01EF},  // 变体选择符补充
        // 零宽 / 方向控制符
        {0x200B,  0x200F},   // ZWSP, ZWNJ, ZWJ, LRM, RLM
        {0x202A,  0x202E},   // LRE, RLE, PDF, LRO, RLO
        {0x2060,  0x2064},   // 单词连接符 / 不可见操作符
        {0xFEFF,  0xFEFF},   // 零宽不换行空格 / BOM
    };

    // 单个码点的显示宽度
    constexpr int CharWidth(unsigned int cp) noexcept {
        if (cp < 0x20u || cp == 0x7Fu) return 0;            // C0 控制字符、DEL
        if (cp < 0x80u) return 1;                           // ASCII 可打印
        if (InRanges(cp, kZeroRanges)) return 0;            // 组合符 / 零宽 / 方向控制
        if (InRanges(cp, kWideRanges)) return 2;            // 东亚宽字符
        return 1;
    }

    // ===================== UTF-8 解码 =====================

    constexpr bool IsCont(unsigned char c) noexcept {
        return (c & 0xC0u) == 0x80u;
    }

    // 从 p 处解码一个 UTF-8 码点。
    // 成功：*cp 写入码点，p 前进相应字节数，返回消耗字节数（1~4）。
    // 失败：p 保持不变，返回 0。
    int Utf8Decode(const char*& p, unsigned int& cp) noexcept {
        const auto* s = reinterpret_cast<const unsigned char*>(p);
        const unsigned char c0 = s[0];

        if (c0 < 0x80u) {                       // 0xxxxxxx
            cp = c0;
            p += 1;
            return 1;
        }
        if ((c0 & 0xE0u) == 0xC0u) {            // 110xxxxx 10xxxxxx
            const unsigned char c1 = s[1];
            if (!IsCont(c1)) return 0;
            const unsigned int v = ((c0 & 0x1Fu) << 6) | (c1 & 0x3Fu);
            if (v < 0x80u) return 0;            // 过长编码
            cp = v;
            p += 2;
            return 2;
        }
        if ((c0 & 0xF0u) == 0xE0u) {            // 1110xxxx 10xxxxxx 10xxxxxx
            const unsigned char c1 = s[1];
            if (!IsCont(c1)) return 0;
            const unsigned char c2 = s[2];
            if (!IsCont(c2)) return 0;
            const unsigned int v = ((c0 & 0x0Fu) << 12) |
                ((c1 & 0x3Fu) << 6) |
                (c2 & 0x3Fu);
            if (v < 0x800u) return 0;           // 过长编码
            if (v >= 0xD800u && v <= 0xDFFFu) return 0;  // UTF-16 代理区
            cp = v;
            p += 3;
            return 3;
        }
        if ((c0 & 0xF8u) == 0xF0u) {            // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            const unsigned char c1 = s[1];
            if (!IsCont(c1)) return 0;
            const unsigned char c2 = s[2];
            if (!IsCont(c2)) return 0;
            const unsigned char c3 = s[3];
            if (!IsCont(c3)) return 0;
            const unsigned int v = ((c0 & 0x07u) << 18) |
                ((c1 & 0x3Fu) << 12) |
                ((c2 & 0x3Fu) << 6) |
                (c3 & 0x3Fu);
            if (v < 0x10000u || v > 0x10FFFFu) return 0;
            cp = v;
            p += 4;
            return 4;
        }
        return 0;
    }

    // ===================== 模式字符串处理 =====================

    constexpr char ToLowerAscii(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
    }

    constexpr bool StrEqNoCase(const char* a, const char* b) noexcept {
        while (*a && *b) {
            if (ToLowerAscii(*a) != ToLowerAscii(*b)) return false;
            ++a;
            ++b;
        }
        return *a == *b;
    }

    constexpr bool IsUtf8Mode(const char* mode) noexcept {
        if (mode == nullptr) return true;
        return StrEqNoCase(mode, "utf-8") || StrEqNoCase(mode, "utf8");
    }

}  // namespace

// ===================== 公开接口 =====================

int CalcTextWidth(const char* text, const char* mode) {
    if (text == nullptr) return 0;

    int width = 0;

    if (IsUtf8Mode(mode)) {
        const char* p = text;
        while (*p) {
            unsigned int cp = 0;
            const char* before = p;
            if (Utf8Decode(p, cp) > 0) {
                width += CharWidth(cp);
            }
            else {
                // 非法 UTF-8 字节：跳过 1 字节，按 1 列计
                p = before + 1;
                width += 1;
            }
        }
    }
    else {
        // 单字节模式
        for (const char* p = text; *p; ++p) {
            const auto c = static_cast<unsigned char>(*p);
            if (c >= 0x20u && c != 0x7Fu) {
                width += 1;
            }
        }
    }

    return width;
}