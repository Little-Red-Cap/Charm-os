// QR encode module (ported from C)

module;
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <new>
#include <ranges>
#include <span>

export module alg.qr_encode;

namespace alg::qr {

    inline constexpr std::int16_t kMaxModuleSize = 33;
    inline constexpr std::int16_t kMaxAllCodeword = 400;
    inline constexpr std::int16_t kMaxDataCodeword = 400;
    inline constexpr std::int16_t kMaxCodeBlock = 153;
    inline constexpr std::int16_t k_qr_margin = 4;
    inline constexpr std::int16_t k_qr_ver1_size = 21;

    inline constexpr int k_qr_level_l = 0;
    inline constexpr int k_qr_level_m = 1;
    inline constexpr int k_qr_level_q = 2;
    inline constexpr int k_qr_level_h = 3;

    inline constexpr int k_qr_mode_numeral = 0;
    inline constexpr int k_qr_mode_alphabet = 1;
    inline constexpr int k_qr_mode_8bit = 2;
    inline constexpr int k_qr_mode_kanji = 3;
    inline constexpr int k_qr_mode_chinese = 4;

    inline constexpr int k_qr_version_s = 0;
    inline constexpr int k_qr_version_m = 1;
    inline constexpr int k_qr_version_l = 2;

    export enum class Level : std::uint8_t {
        L = k_qr_level_l,
        M = k_qr_level_m,
        Q = k_qr_level_q,
        H = k_qr_level_h,
    };

    export enum class Mode : std::uint8_t {
        Numeral = k_qr_mode_numeral,
        Alphabet = k_qr_mode_alphabet,
        Byte8 = k_qr_mode_8bit,
        Kanji = k_qr_mode_kanji,
        Chinese = k_qr_mode_chinese,
    };
} // namespace alg::qr

// C-port interface (globals + functions). Intentionally non-exported.
namespace alg::qr::detail {
struct State {
	int symbol_size{0};
	std::uint8_t module_data_buf[kMaxModuleSize][kMaxModuleSize]{};
	int data_codeword_bit{0};
	std::uint8_t data_codewords_buf[kMaxDataCodeword]{};
	int data_block_count{0};
	std::uint8_t block_modes_buf[kMaxDataCodeword]{};
	std::uint8_t block_lengths_buf[kMaxDataCodeword]{};
	int all_codeword_count{0};
	std::uint8_t all_codewords_buf[kMaxAllCodeword]{};
	std::uint8_t rs_work_buf[kMaxCodeBlock]{};
	int qr_level{0};
	int qr_version{0};
	bool auto_extent{false};
	int masking_no{0};
};
inline thread_local State* current_state = nullptr;

inline State& state() noexcept { return *current_state; }
inline State* set_current(State* st) noexcept { State* prev = current_state; current_state = st; return prev; }
inline void restore_current(State* prev) noexcept { current_state = prev; }
struct rs_block_info
{
	std::uint16_t rs_block_count;
	std::uint16_t total_codeword_count;
	std::uint16_t data_codeword_count;
};
struct qr_version_info
{
	std::uint16_t version_id;
	std::uint16_t total_codeword_count;
	std::uint16_t data_codeword_count[4];
	std::uint16_t align_point_count;
	std::uint16_t align_points_buf[6];
	rs_block_info rs_block_info1[4];
	rs_block_info rs_block_info2[4];
};
bool encode_data(char *source);
int get_encode_version(int version, char *source, int source_len);
int encode_source_data(char *source, int source_len, int ver_group);
int get_bit_length(std::uint8_t	 mode, int data_len, int ver_group);
int set_bit_stream(int bit_index, std::uint16_t data_word, int data_len);
bool is_numeral_data(std::uint8_t c);
bool is_alphabet_data(std::uint8_t c);
bool is_kanji_data(std::uint8_t c1, std::uint8_t c2);
bool is_chinese_data(std::uint8_t c1, std::uint8_t c2);
std::uint8_t	 alphabet_to_binary(std::uint8_t c);
std::uint16_t kanji_to_binary(std::uint16_t wc);
std::uint16_t chinese_to_binary(std::uint16_t wc);
void get_rs_codeword(std::uint8_t	 *rs_work_buffer, int data_codeword_count, int rs_codeword_count);
void format_module();
void set_function_module();
void set_finder_pattern(int x, int y);
void set_alignment_pattern(int x, int y);
void set_version_pattern();
void set_codeword_pattern();
void set_masking_pattern(int pattern_no);
void set_format_info_pattern(int pattern_no);
int count_penalty();
void print_2d_code();

const qr_version_info qr_version_info_data[] = {
    {0},
    {
        1,
        26,
        {19, 16, 13, 9},
        0,
        {0, 0, 0, 0, 0, 0},
        {
            {1, 26, 19},
            {1, 26, 16},
            {1, 26, 13},
            {1, 26, 9},
        },
        {
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0},
        }
    },
    {
        2,
        44,
        {34, 28, 22, 16},
        1,
        {18, 0, 0, 0, 0, 0},
        {
            {1, 44, 34},
            {1, 44, 28},
            {1, 44, 22},
            {1, 44, 16},
        },
        {
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0},
        }
    },
	{
		3,
		70,
		{55, 44, 34, 26},
		1,
		{22, 0, 0, 0, 0, 0},
		{
			{1, 70, 55},
			{1, 70, 44},
			{2, 35, 17},
			{2, 35, 13},
		},
		{
			{0, 0, 0},
			{0, 0, 0},
			{0, 0, 0},
			{0, 0, 0},
		}
	},
	{
		4,
		100,
		{80, 64, 48, 36},
		1,
		{26, 0, 0, 0, 0, 0},
		{
			{1, 100, 80},
			{2, 50, 32},
			{2, 50, 24},
			{4, 25, 9},
		},
		{
			{0, 0, 0},
			{0, 0, 0},
			{0, 0, 0},
			{0, 0, 0},
		}
	},
    {
        5,
        134,
        {108, 86, 62, 46},
        1,
        {30, 0, 0, 0, 0, 0},
        {
            {1, 134, 108},
            {2, 67, 43},
            {2, 33, 15},
            {2, 33, 11},
        },
        {
            {0, 0, 0},
            {0, 0, 0},
            {2, 34, 16},
            {2, 34, 12},
        }
    },
    {
        6,
        172,
        {136, 108, 76, 60},
        1,
        {34, 0, 0, 0, 0, 0},
        {
            {2, 86, 68},
            {4, 43, 27},
            {4, 43, 19},
            {4, 43, 15},
        },
        {
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0},
        }
    },
    {
        7,
        196,
        {156, 124, 88, 66},
        2,
        {22, 38, 0, 0, 0, 0},
        {
            {2, 98, 78},
            {4, 49, 31},
            {2, 32, 14},
            {4, 39, 13},
        },
        {
            {0, 0, 0},
            {0, 0, 0},
            {4, 33, 15},
            {1, 40, 14},
        }
    },
    {
        8,
        242,
        {194, 154, 110, 86},
        2,
        {24, 42, 0, 0, 0, 0},
        {
            {2, 121, 97},
            {2, 60, 38},
            {4, 40, 18},
            {4, 40, 14},
        },
        {
            {0, 0, 0},
            {2, 61, 39},
            {2, 41, 19},
            {2, 41, 15},
        }
    },
    {
        9,
        292,
        {232, 182, 132, 100},
        2,
        {26, 46, 0, 0, 0, 0},
        {
            {2, 146, 116},
            {3, 58, 36},
            {4, 36, 16},
            {4, 36, 12},
        },
        {
            {0, 0, 0},
            {2, 59, 37},
            {4, 37, 17},
            {4, 37, 13},
        }
    },
    {
        10,
        346,
        {274, 216, 154, 122},
        2,
        {28, 50, 0, 0, 0, 0},
        {
            {2, 86, 68},
            {4, 69, 43},
            {6, 43, 19},
            {6, 43, 15},
        },
        {
            {2, 87, 69},
            {1, 70, 44},
            {2, 44, 20},
            {2, 44, 16},
        }
    },
    {
        11,
        404,
        {324, 254, 180, 140},
        2,
        {30, 54, 0, 0, 0, 0},
        {
            {4, 101, 81},
            {1, 80, 50},
            {4, 50, 22},
            {3, 36, 12},
        },
        {
            {0, 0, 0},
            {4, 81, 51},
            {4, 51, 23},
            {8, 37, 13},
        }
    },
    {
        12,
        466,
        {370, 290, 206, 158},
        2,
        {32, 58, 0, 0, 0, 0},
        {
            {2, 116, 92},
            {6, 58, 36},
            {4, 46, 20},
            {7, 42, 14},
        },
        {
            {2, 117, 93},
            {2, 59, 37},
            {6, 47, 21},
            {4, 43, 15},
        }
    },
    {
        13,
        532,
        {428, 334, 244, 180},
        2,
        {34, 62, 0, 0, 0, 0},
        {
            {4, 133, 107},
            {8, 59, 37},
            {8, 44, 20},
            {12, 33, 11},
        },
        {
            {0, 0, 0},
            {1, 60, 38},
            {4, 45, 21},
            {4, 34, 12},
        }
    },
    {
        14,
        581,
        {461, 365, 261, 197},
        3,
        {26, 46, 66, 0, 0, 0},
        {
            {3, 145, 115},
            {4, 64, 40},
            {11, 36, 16},
            {11, 36, 12},
        },
        {
            {1, 146, 116},
            {5, 65, 41},
            {5, 37, 17},
            {5, 37, 13},
        }
    },
    {
        15,
        655,
        {523, 415, 295, 223},
        3,
        {26, 48, 70, 0, 0, 0},
        {
            {5, 109, 87},
            {5, 65, 41},
            {5, 54, 24},
            {11, 36, 12},
        },
        {
            {1, 110, 88},
            {5, 66, 42},
            {7, 55, 25},
            {7, 37, 13},
        }
    },
    {
        16,
        733,
        {589, 453, 325, 253},
        3,
        {26, 50, 74, 0, 0, 0},
        {
            {5, 122, 98},
            {7, 73, 45},
            {15, 43, 19},
            {3, 45, 15},
        },
        {
            {1, 123, 99},
            {3, 74, 46},
            {2, 44, 20},
            {13, 46, 16},
        }
    },
    {
        17,
        815,
        {647, 507, 367, 283},
        3,
        {30, 54, 78, 0, 0, 0},
        {
            {1, 135, 107},
            {10, 74, 46},
            {1, 50, 22},
            {2, 42, 14},
        },
        {
            {5, 136, 108},
            {1, 75, 47},
            {15, 51, 23},
            {17, 43, 15},
        }
    },
    {
        18,
        901,
        {721, 563, 397, 313},
        3,
        {30, 56, 82, 0, 0, 0},
        {
            {5, 150, 120},
            {9, 69, 43},
            {17, 50, 22},
            {2, 42, 14},
        },
        {
            {1, 151, 121},
            {4, 70, 44},
            {1, 51, 23},
            {19, 43, 15},
        }
    },
    {
        19,
        991,
        {795, 627, 445, 341},
        3,
        {30, 58, 86, 0, 0, 0},
        {
            {3, 141, 113},
            {3, 70, 44},
            {17, 47, 21},
            {9, 39, 13},
        },
        {
            {4, 142, 114},
            {11, 71, 45},
            {4, 48, 22},
            {16, 40, 14},
        }
    },
    {
        20,
        1085,
        {861, 669, 485, 385},
        3,
        {34, 62, 90, 0, 0, 0},
        {
            {3, 135, 107},
            {3, 67, 41},
            {15, 54, 24},
            {15, 43, 15},
        },
        {
            {5, 136, 108},
            {13, 68, 42},
            {5, 55, 25},
            {10, 44, 16},
        }
    },
    {
        21,
        1156,
        {932, 714, 512, 406},
        4,
        {28, 50, 72, 94, 0, 0},
        {
            {4, 144, 116},
            {17, 68, 42},
            {17, 50, 22},
            {19, 46, 16},
        },
        {
            {4, 145, 117},
            {0, 0, 0},
            {6, 51, 23},
            {6, 47, 17},
        }
    },
    {
        22,
        1258,
        {1006, 782, 568, 442},
        4,
        {26, 50, 74, 98, 0, 0},
        {
            {2, 139, 111},
            {17, 74, 46},
            {7, 54, 24},
            {34, 37, 13},
        },
        {
            {7, 140, 112},
            {0, 0, 0},
            {16, 55, 25},
            {0, 0, 0},
        }
    },
    {
        23,
        1364,
        {1094, 860, 614, 464},
        4,
        {30, 54, 78, 102, 0, 0},
        {
            {4, 151, 121},
            {4, 75, 47},
            {11, 54, 24},
            {16, 45, 15},
        },
        {
            {5, 152, 122},
            {14, 76, 48},
            {14, 55, 25},
            {14, 46, 16},
        }
    },
    {
        24,
        1474,
        {1174, 914, 664, 514},
        4,
        {28, 54, 80, 106, 0, 0},
        {
            {6, 147, 117},
            {6, 73, 45},
            {11, 54, 24},
            {30, 46, 16},
        },
        {
            {4, 148, 118},
            {14, 74, 46},
            {16, 55, 25},
            {2, 47, 17},
        }
    },
    {
        25,
        1588,
        {1276, 1000, 718, 538},
        4,
        {32, 58, 84, 110, 0, 0},
        {
            {8, 132, 106},
            {8, 75, 47},
            {7, 54, 24},
            {22, 45, 15},
        },
        {
            {4, 133, 107},
            {13, 76, 48},
            {22, 55, 25},
            {13, 46, 16},
        }
    },
    {
        26,
        1706,
        {1370, 1062, 754, 596},
        4,
        {30, 58, 86, 114, 0, 0},
        {
            {10, 142, 114},
            {19, 74, 46},
            {28, 50, 22},
            {33, 46, 16},
        },
        {
            {2, 143, 115},
            {4, 75, 47},
            {6, 51, 23},
            {4, 47, 17},
        }
    },
    {
        27,
        1828,
        {1468, 1128, 808, 628},
        4,
        {34, 62, 90, 118, 0, 0},
        {
            {8, 152, 122},
            {22, 73, 45},
            {8, 53, 23},
            {12, 45, 15},
        },
        {
            {4, 153, 123},
            {3, 74, 46},
            {26, 54, 24},
            {28, 46, 16},
        }
    },
    {
        28,
        1921,
        {1531, 1193, 871, 661},
        5,
        {26, 50, 74, 98, 122, 0},
        {
            {3, 147, 117},
            {3, 73, 45},
            {4, 54, 24},
            {11, 45, 15},
        },
        {
            {10, 148, 118},
            {23, 74, 46},
            {31, 55, 25},
            {31, 46, 16},
        }
    },
    {
        29,
        2051,
        {1631, 1267, 911, 701},
        5,
        {30, 54, 78, 102, 126, 0},
        {
            {7, 146, 116},
            {21, 73, 45},
            {1, 53, 23},
            {19, 45, 15},
        },
        {
            {7, 147, 117},
            {7, 74, 46},
            {37, 54, 24},
            {26, 46, 16},
        }
    },
    {
        30,
        2185,
        {1735, 1373, 985, 745},
        5,
        {26, 52, 78, 104, 130, 0},
        {
            {5, 145, 115},
            {19, 75, 47},
            {15, 54, 24},
            {23, 45, 15},
        },
        {
            {10, 146, 116},
            {10, 76, 48},
            {25, 55, 25},
            {25, 46, 16},
        }
    },
    {
        31,
        2323,
        {1843, 1455, 1033, 793},
        5,
        {30, 56, 82, 108, 134, 0},
        {
            {13, 145, 115},
            {2, 74, 46},
            {42, 54, 24},
            {23, 45, 15},
        },
        {
            {3, 146, 116},
            {29, 75, 47},
            {1, 55, 25},
            {28, 46, 16},
        }
    },
    {
        32,
        2465,
        {1955, 1541, 1115, 845},
        5,
        {34, 60, 86, 112, 138, 0},
        {
            {17, 145, 115},
            {10, 74, 46},
            {10, 54, 24},
            {19, 45, 15},
        },
        {
            {0, 0, 0},
            {23, 75, 47},
            {35, 55, 25},
            {35, 46, 16},
        }
    },
    {
        33,
        2611,
        {2071, 1631, 1171, 901},
        5,
        {30, 58, 86, 114, 142, 0},
        {
            {17, 145, 115},
            {14, 74, 46},
            {29, 54, 24},
            {11, 45, 15},
        },
        {
            {1, 146, 116},
            {21, 75, 47},
            {19, 55, 25},
            {46, 46, 16},
        }
    },
    {
        34,
        2761,
        {2191, 1725, 1231, 961},
        5,
        {34, 62, 90, 118, 146, 0},
        {
            {13, 145, 115},
            {14, 74, 46},
            {44, 54, 24},
            {59, 46, 16},
        },
        {
            {6, 146, 116},
            {23, 75, 47},
            {7, 55, 25},
            {1, 47, 17},
        }
    },
    {
        35,
        2876,
        {2306, 1812, 1286, 986},
        6,
        {30, 54, 78, 102, 126, 150},
        {
            {12, 151, 121},
            {12, 75, 47},
            {39, 54, 24},
            {22, 45, 15},
        },
        {
            {7, 152, 122},
            {26, 76, 48},
            {14, 55, 25},
            {41, 46, 16},
        }
    },
    {
        36,
        3034,
        {2434, 1914, 1354, 1054},
        6,
        {24, 50, 76, 102, 128, 154},
        {
            {6, 151, 121},
            {6, 75, 47},
            {46, 54, 24},
            {2, 45, 15},
        },
        {
            {14, 152, 122},
            {34, 76, 48},
            {10, 55, 25},
            {64, 46, 16},
        }
    },
    {
        37,
        3196,
        {2566, 1992, 1426, 1096},
        6,
        {28, 54, 80, 106, 132, 158},
        {
            {17, 152, 122},
            {29, 74, 46},
            {49, 54, 24},
            {24, 45, 15},
        },
        {
            {4, 153, 123},
            {14, 75, 47},
            {10, 55, 25},
            {46, 46, 16},
        }
    },
    {
        38,
        3362,
        {2702, 2102, 1502, 1142},
        6,
        {32, 58, 84, 110, 136, 162},
        {
            {4, 152, 122},
            {13, 74, 46},
            {48, 54, 24},
            {42, 45, 15},
        },
        {
            {18, 153, 123},
            {32, 75, 47},
            {14, 55, 25},
            {32, 46, 16},
        }
    },
    {
        39,
        3532,
        {2812, 2216, 1582, 1222},
        6,
        {26, 54, 82, 110, 138, 166},
        {
            {20, 147, 117},
            {40, 75, 47},
            {43, 54, 24},
            {10, 45, 15},
        },
        {
            {4, 148, 118},
            {7, 76, 48},
            {22, 55, 25},
            {67, 46, 16},
        }
    },
    {
        40,
        3706,
        {2956, 2334, 1666, 1276},
        6,
        {30, 58, 86, 114, 142, 170},
        {
            {19, 148, 118},
            {18, 75, 47},
            {34, 54, 24},
            {20, 45, 15},
        },
        {
            {6, 149, 119},
            {31, 76, 48},
            {34, 55, 25},
            {61, 46, 16},
        }
    }
};

#define symbol_size (state().symbol_size)
#define module_data_buf (state().module_data_buf)
#define data_codeword_bit (state().data_codeword_bit)
#define data_codewords_buf (state().data_codewords_buf)
#define data_block_count (state().data_block_count)
#define all_codeword_count (state().all_codeword_count)
#define all_codewords_buf (state().all_codewords_buf)
#define rs_work_buf (state().rs_work_buf)
#define qr_level (state().qr_level)
#define qr_version (state().qr_version)
#define auto_extent (state().auto_extent)
#define masking_no (state().masking_no)
inline std::uint8_t& module_at(int x, int y) noexcept { return module_data_buf[x][y]; }
inline bool module_reserved(int x, int y) noexcept { return (module_at(x, y) & 0x20) != 0; }
inline void module_set(int x, int y, std::uint8_t v) noexcept { module_at(x, y) = v; }
inline auto data_codewords() noexcept -> std::span<std::uint8_t> { return {data_codewords_buf, kMaxDataCodeword}; }
inline auto block_modes() noexcept -> std::span<std::uint8_t> { return {state().block_modes_buf, kMaxDataCodeword}; }
inline auto block_lengths() noexcept -> std::span<std::uint8_t> { return {state().block_lengths_buf, kMaxDataCodeword}; }
inline auto all_codewords() noexcept -> std::span<std::uint8_t> { return {all_codewords_buf, (std::size_t)all_codeword_count}; }
inline auto rs_work() noexcept -> std::span<std::uint8_t> { return {rs_work_buf, kMaxCodeBlock}; }
const std::uint8_t exp_to_int_table_data[] = {
      1,   2,   4,   8,  16,  32,  64, 128,  29,  58, 116, 232, 205, 135,  19,  38,
     76, 152,  45,  90, 180, 117, 234, 201, 143,   3,   6,  12,  24,  48,  96, 192,
    157,  39,  78, 156,  37,  74, 148,  53, 106, 212, 181, 119, 238, 193, 159,  35,
     70, 140,   5,  10,  20,  40,  80, 160,  93, 186, 105, 210, 185, 111, 222, 161,
     95, 190,  97, 194, 153,  47,  94, 188, 101, 202, 137,  15,  30,  60, 120, 240,
    253, 231, 211, 187, 107, 214, 177, 127, 254, 225, 223, 163,  91, 182, 113, 226,
    217, 175,  67, 134,  17,  34,  68, 136,  13,  26,  52, 104, 208, 189, 103, 206,
    129,  31,  62, 124, 248, 237, 199, 147,  59, 118, 236, 197, 151,  51, 102, 204,
    133,  23,  46,  92, 184, 109, 218, 169,  79, 158,  33,  66, 132,  21,  42,  84,
    168,  77, 154,  41,  82, 164,  85, 170,  73, 146,  57, 114, 228, 213, 183, 115,
    230, 209, 191,  99, 198, 145,  63, 126, 252, 229, 215, 179, 123, 246, 241, 255,
    227, 219, 171,  75, 150,  49,  98, 196, 149,  55, 110, 220, 165,  87, 174,  65,
    130,  25,  50, 100, 200, 141,   7,  14,  28,  56, 112, 224, 221, 167,  83, 166,
     81, 162,  89, 178, 121, 242, 249, 239, 195, 155,  43,  86, 172,  69, 138,   9,
     18,  36,  72, 144,  61, 122, 244, 245, 247, 243, 251, 235, 203, 139,  11,  22,
     44,  88, 176, 125, 250, 233, 207, 131,  27,  54, 108, 216, 173,  71, 142,   1
};
const std::uint8_t	 int_to_exp_table_data[] = {  0,   0,   1,  25,   2,  50,  26, 198,   3, 223,  51, 238,  27, 104, 199,  75,
							  4, 100, 224,  14,  52, 141, 239, 129,  28, 193, 105, 248, 200,   8,  76, 113,
							  5, 138, 101,  47, 225,  36,  15,  33,  53, 147, 142, 218, 240,  18, 130,  69,
							 29, 181, 194, 125, 106,  39, 249, 185, 201, 154,   9, 120,  77, 228, 114, 166,
							  6, 191, 139,  98, 102, 221,  48, 253, 226, 152,  37, 179,  16, 145,  34, 136,
							 54, 208, 148, 206, 143, 150, 219, 189, 241, 210,  19,  92, 131,  56,  70,  64,
							 30,  66, 182, 163, 195,  72, 126, 110, 107,  58,  40,  84, 250, 133, 186,  61,
							202,  94, 155, 159,  10,  21, 121,  43,  78, 212, 229, 172, 115, 243, 167,  87,
							  7, 112, 192, 247, 140, 128,  99,  13, 103,  74, 222, 237,  49, 197, 254,  24,
							227, 165, 153, 119,  38, 184, 180, 124,  17,  68, 146, 217,  35,  32, 137,  46,
							 55,  63, 209,  91, 149, 188, 207, 205, 144, 135, 151, 178, 220, 252, 190,  97,
							242,  86, 211, 171,  20,  42,  93, 158, 132,  60,  57,  83,  71, 109,  65, 162,
							 31,  45,  67, 216, 183, 123, 164, 118, 196,  23,  73, 236, 127,  12, 111, 246,
							108, 161,  59,  82,  41, 157,  85, 170, 251,  96, 134, 177, 187, 204,  62,  90,
							203,  89,  95, 176, 156, 169, 160,  81,  11, 245,  22, 235, 122, 117,  44, 215,
							 79, 174, 213, 233, 230, 231, 173, 232, 116, 214, 244, 234, 168,  80,  88, 175};
const std::uint8_t	 rs_exp_table_7[]  = {87, 229, 146, 149, 238, 102,  21};
const std::uint8_t	 rs_exp_table_10[] = {251,  67,  46,  61, 118,  70,  64,  94,  32,  45};
const std::uint8_t	 rs_exp_table_13[] = { 74, 152, 176, 100,  86, 100, 106, 104, 130, 218, 206, 140,  78};
const std::uint8_t	 rs_exp_table_15[] = {  8, 183,  61,  91, 202,  37,  51,  58,  58, 237, 140, 124,   5,  99, 105};
const std::uint8_t	 rs_exp_table_16[] = {120, 104, 107, 109, 102, 161,  76,   3,  91, 191, 147, 169, 182, 194, 225, 120};
const std::uint8_t	 rs_exp_table_17[] = { 43, 139, 206,  78,  43, 239, 123, 206, 214, 147,  24,  99, 150,  39, 243, 163, 136};
const std::uint8_t	 rs_exp_table_18[] = {215, 234, 158,  94, 184,  97, 118, 170,  79, 187, 152, 148, 252, 179,   5,  98,  96, 153};
const std::uint8_t	 rs_exp_table_20[] = { 17,  60,  79,  50,  61, 163,  26, 187, 202, 180, 221, 225,  83, 239, 156, 164, 212, 212, 188, 190};
const std::uint8_t	 rs_exp_table_22[] = {210, 171, 247, 242,  93, 230,  14, 109, 221,  53, 200,  74,   8, 172,  98,  80, 219, 134, 160, 105,
						   165, 231};
const std::uint8_t	 rs_exp_table_24[] = {229, 121, 135,  48, 211, 117, 251, 126, 159, 180, 169, 152, 192, 226, 228, 218, 111,   0, 117, 232,
						    87,  96, 227,  21};
const std::uint8_t	 rs_exp_table_26[] = {173, 125, 158,   2, 103, 182, 118,  17, 145, 201, 111,  28, 165,  53, 161,  21, 245, 142,  13, 102,
						    48, 227, 153, 145, 218,  70};
const std::uint8_t	 rs_exp_table_28[] = {168, 223, 200, 104, 224, 234, 108, 180, 110, 190, 195, 147, 205,  27, 232, 201,  21,  43, 245,  87,
						    42, 195, 212, 119, 242,  37,   9, 123};
const std::uint8_t	 rs_exp_table_30[] = { 41, 173, 145, 152, 216,  31, 179, 182,  50,  48, 110,  86, 239,  96, 222, 125,  42, 173, 226, 193,
						   224, 130, 156,  37, 251, 216, 238,  40, 192, 180};
const std::uint8_t	 rs_exp_table_32[] = { 10,   6, 106, 190, 249, 167,   4,  67, 209, 138, 138,  32, 242, 123,  89,  27, 120, 185,  80, 156,
						    38,  69, 171,  60,  28, 222,  80,  52, 254, 185, 220, 241};
const std::uint8_t	 rs_exp_table_34[] = {111,  77, 146,  94,  26,  21, 108,  19, 105,  94, 113, 193,  86, 140, 163, 125,  58, 158, 229, 239,
						   218, 103,  56,  70, 114,  61, 183, 129, 167,  13,  98,  62, 129,  51};
const std::uint8_t	 rs_exp_table_36[] = {200, 183,  98,  16, 172,  31, 246, 234,  60, 152, 115,   0, 167, 152, 113, 248, 238, 107,  18,  63,
						   218,  37,  87, 210, 105, 177, 120,  74, 121, 196, 117, 251, 113, 233,  30, 120};
const std::uint8_t	 rs_exp_table_38[] = {159,  34,  38, 228, 230,  59, 243,  95,  49, 218, 176, 164,  20,  65,  45, 111,  39,  81,  49, 118,
						   113, 222, 193, 250, 242, 168, 217,  41, 164, 247, 177,  30, 238,  18, 120, 153,  60, 193};
const std::uint8_t	 rs_exp_table_40[] = { 59, 116,  79, 161, 252,  98, 128, 205, 128, 161, 247,  57, 163,  56, 235, 106,  53,  26, 187, 174,
						   226, 104, 170,   7, 175,  35, 181, 114,  88,  41,  47, 163, 125, 134,  72,  20, 232,  53,  35,  15};
const std::uint8_t	 rs_exp_table_42[] = {250, 103, 221, 230,  25,  18, 137, 231,   0,   3,  58, 242, 221, 191, 110,  84, 230,   8, 188, 106,
						    96, 147,  15, 131, 139,  34, 101, 223,  39, 101, 213, 199, 237, 254, 201, 123, 171, 162, 194, 117,
						    50,  96};
const std::uint8_t	 rs_exp_table_44[] = {190,   7,  61, 121,  71, 246,  69,  55, 168, 188,  89, 243, 191,  25,  72, 123,   9, 145,  14, 247,
						     1, 238,  44,  78, 143,  62, 224, 126, 118, 114,  68, 163,  52, 194, 217, 147, 204, 169,  37, 130,
						   113, 102,  73, 181};
const std::uint8_t	 rs_exp_table_46[] = {112,  94,  88, 112, 253, 224, 202, 115, 187,  99,  89,   5,  54, 113, 129,  44,  58,  16, 135, 216,
						   169, 211,  36,   1,   4,  96,  60, 241,  73, 104, 234,   8, 249, 245, 119, 174,  52,  25, 157, 224,
						    43, 202, 223,  19,  82,  15};
const std::uint8_t	 rs_exp_table_48[] = {228,  25, 196, 130, 211, 146,  60,  24, 251,  90,  39, 102, 240,  61, 178,  63,  46, 123, 115,  18,
						   221, 111, 135, 160, 182, 205, 107, 206,  95, 150, 120, 184,  91,  21, 247, 156, 140, 238, 191,  11,
						    94, 227,  84,  50, 163,  39,  34, 108};
const std::uint8_t	 rs_exp_table_50[] = {232, 125, 157, 161, 164,   9, 118,  46, 209,  99, 203, 193,  35,   3, 209, 111, 195, 242, 203, 225,
						    46,  13,  32, 160, 126, 209, 130, 160, 242, 215, 242,  75,  77,  42, 189,  32, 113,  65, 124,  69,
						   228, 114, 235, 175, 124, 170, 215, 232, 133, 205};
const std::uint8_t	 rs_exp_table_52[] = {116,  50,  86, 186,  50, 220, 251,  89, 192,  46,  86, 127, 124,  19, 184, 233, 151, 215,  22,  14,
						    59, 145,  37, 242, 203, 134, 254,  89, 190,  94,  59,  65, 124, 113, 100, 233, 235, 121,  22,  76,
						    86,  97,  39, 242, 200, 220, 101,  33, 239, 254, 116,  51};
const std::uint8_t	 rs_exp_table_54[] = {183,  26, 201,  87, 210, 221, 113,  21,  46,  65,  45,  50, 238, 184, 249, 225, 102,  58, 209, 218,
						   109, 165,  26,  95, 184, 192,  52, 245,  35, 254, 238, 175, 172,  79, 123,  25, 122,  43, 120, 108,
						   215,  80, 128, 201, 235,   8, 153,  59, 101,  31, 198,  76,  31, 156};
const std::uint8_t	 rs_exp_table_56[] = {106, 120, 107, 157, 164, 216, 112, 116,   2,  91, 248, 163,  36, 201, 202, 229,   6, 144, 254, 155,
						   135, 208, 170, 209,  12, 139, 127, 142, 182, 249, 177, 174, 190,  28,  10,  85, 239, 184, 101, 124,
						   152, 206,  96,  23, 163,  61,  27, 196, 247, 151, 154, 202, 207,  20,  61,  10};
const std::uint8_t	 rs_exp_table_58[] = { 82, 116,  26, 247,  66,  27,  62, 107, 252, 182, 200, 185, 235,  55, 251, 242, 210, 144, 154, 237,
						   176, 141, 192, 248, 152, 249, 206,  85, 253, 142,  65, 165, 125,  23,  24,  30, 122, 240, 214,   6,
						   129, 218,  29, 145, 127, 134, 206, 245, 117,  29,  41,  63, 159, 142, 233, 125, 148, 123};
const std::uint8_t	 rs_exp_table_60[] = {107, 140,  26,  12,   9, 141, 243, 197, 226, 197, 219,  45, 211, 101, 219, 120,  28, 181, 127,   6,
						   100, 247,   2, 205, 198,  57, 115, 219, 101, 109, 160,  82,  37,  38, 238,  49, 160, 209, 121,  86,
						    11, 124,  30, 181,  84,  25, 194,  87,  65, 102, 190, 220,  70,  27, 209,  16,  89,   7,  33, 240};
const std::uint8_t	 rs_exp_table_62[] = { 65, 202, 113,  98,  71, 223, 248, 118, 214,  94,   0, 122,  37,  23,   2, 228,  58, 121,   7, 105,
						   135,  78, 243, 118,  70,  76, 223,  89,  72,  50,  70, 111, 194,  17, 212, 126, 181,  35, 221, 117,
						   235,  11, 229, 149, 147, 123, 213,  40, 115,   6, 200, 100,  26, 246, 182, 218, 127, 215,  36, 186,
						   110, 106};
const std::uint8_t	 rs_exp_table_64[] = { 45,  51, 175,   9,   7, 158, 159,  49,  68, 119,  92, 123, 177, 204, 187, 254, 200,  78, 141, 149,
						   119,  26, 127,  53, 160,  93, 199, 212,  29,  24, 145, 156, 208, 150, 218, 209,   4, 216,  91,  47,
						   184, 146,  47, 140, 195, 195, 125, 242, 238,  63,  99, 108, 140, 230, 242,  31, 204,  11, 178, 243,
						   217, 156, 213, 231};
const std::uint8_t	 rs_exp_table_66[] = {  5, 118, 222, 180, 136, 136, 162,  51,  46, 117,  13, 215,  81,  17, 139, 247, 197, 171,  95, 173,
						    65, 137, 178,  68, 111,  95, 101,  41,  72, 214, 169, 197,  95,   7,  44, 154,  77, 111, 236,  40,
						   121, 143,  63,  87,  80, 253, 240, 126, 217,  77,  34, 232, 106,  50, 168,  82,  76, 146,  67, 106,
						   171,  25, 132,  93,  45, 105};
const std::uint8_t	 rs_exp_table_68[] = {247, 159, 223,  33, 224,  93,  77,  70,  90, 160,  32, 254,  43, 150,  84, 101, 190, 205, 133,  52,
						    60, 202, 165, 220, 203, 151,  93,  84,  15,  84, 253, 173, 160,  89, 227,  52, 199,  97,  95, 231,
						    52, 177,  41, 125, 137, 241, 166, 225, 118,   2,  54,  32,  82, 215, 175, 198,  43, 238, 235,  27,
						   101, 184, 127,   3,   5,   8, 163, 238};
constexpr std::span<const std::uint8_t> exp_to_int_table{exp_to_int_table_data};
constexpr std::span<const std::uint8_t> int_to_exp_table{int_to_exp_table_data};
constexpr std::span<const std::uint8_t> rs_exp_table[] = {
							{},      {},      {},      {},      {},      {},      {},      std::span{rs_exp_table_7},  {},      {},
							std::span{rs_exp_table_10}, {},      {},      std::span{rs_exp_table_13}, {},      std::span{rs_exp_table_15}, std::span{rs_exp_table_16}, std::span{rs_exp_table_17}, std::span{rs_exp_table_18}, {},
							std::span{rs_exp_table_20}, {},      std::span{rs_exp_table_22}, {},      std::span{rs_exp_table_24}, {},      std::span{rs_exp_table_26}, {},      std::span{rs_exp_table_28}, {},
							std::span{rs_exp_table_30}, {},      std::span{rs_exp_table_32}, {},      std::span{rs_exp_table_34}, {},      std::span{rs_exp_table_36}, {},      std::span{rs_exp_table_38}, {},
							std::span{rs_exp_table_40}, {},      std::span{rs_exp_table_42}, {},      std::span{rs_exp_table_44}, {},      std::span{rs_exp_table_46}, {},      std::span{rs_exp_table_48}, {},
							std::span{rs_exp_table_50}, {},      std::span{rs_exp_table_52}, {},      std::span{rs_exp_table_54}, {},      std::span{rs_exp_table_56}, {},      std::span{rs_exp_table_58}, {},
							std::span{rs_exp_table_60}, {},      std::span{rs_exp_table_62}, {},      std::span{rs_exp_table_64}, {},      std::span{rs_exp_table_66}, {},      std::span{rs_exp_table_68}};
constexpr std::uint8_t                  indicator_len_numeral_data[]  = {10, 12, 14};
constexpr std::uint8_t                  indicator_len_alphabet_data[] = { 9, 11, 13};
constexpr std::uint8_t                  indicator_len_8bit_data[]     = { 8, 16, 16};
constexpr std::uint8_t                  indicator_len_kanji_data[]    = { 8, 10, 12};
constexpr std::span<const std::uint8_t> indicator_len_numeral{indicator_len_numeral_data};
constexpr std::span<const std::uint8_t> indicator_len_alphabet{indicator_len_alphabet_data};
constexpr std::span<const std::uint8_t> indicator_len_8bit{indicator_len_8bit_data};
constexpr std::span<const std::uint8_t> indicator_len_kanji{indicator_len_kanji_data};
bool                                    encode_data(char *source)
{
	int          version      = 1;
	int          auto_extent_flag   = 1;
	std::uint8_t padding_code = 0xec;
	int          data_cw_index  = 0;
	int          block_no      = 0;
	int          source_len_hint      = 0;
	qr_level                   = 0;
	masking_no               = 0;
	int source_len               = source_len_hint > 0 ? source_len_hint : strlen(source);
	if (source_len == 0)
		return false;
	int encode_version = get_encode_version(version, source, source_len);
	if (encode_version == 0)
		return false;
	if (version == 0) {
		qr_version = encode_version;
	}
	else {
		if (encode_version <= version) {
			qr_version = version;
		}
		else {
			if (auto_extent_flag)
				qr_version = encode_version;
			else
				return false;
		}
	}
        int data_codeword_count = qr_version_info_data[qr_version].data_codeword_count[qr_level];
	int terminator_bits   = std::min(4, (data_codeword_count * 8) - data_codeword_bit);
	if (terminator_bits > 0)
		data_codeword_bit = set_bit_stream(data_codeword_bit, 0, terminator_bits);
	auto data_cw = data_codewords();
	for (int i = (data_codeword_bit + 7) / 8; i < data_codeword_count; ++i) {
		data_cw[i]    = padding_code;
		padding_code = (std::uint8_t)(padding_code == 0xec ? 0x11 : 0xec);
	}
        all_codeword_count = qr_version_info_data[qr_version].total_codeword_count;
	auto all_cw = all_codewords();
	auto rs_buf = rs_work();
	std::fill(all_cw.begin(), all_cw.end(), 0);
	const int rs_block_count1   = qr_version_info_data[qr_version].rs_block_info1[qr_level].rs_block_count;
	const int rs_block_count2   = qr_version_info_data[qr_version].rs_block_info2[qr_level].rs_block_count;
	const int block_sum = rs_block_count1 + rs_block_count2;
	const int data_cw_count1  = qr_version_info_data[qr_version].rs_block_info1[qr_level].data_codeword_count;
	const int data_cw_count2  = qr_version_info_data[qr_version].rs_block_info2[qr_level].data_codeword_count;
	for (block_no = 0; block_no < rs_block_count1; ++block_no) {
		for (auto j : std::views::iota(0, data_cw_count1)) {
			all_cw[(block_sum * j) + block_no] = data_cw[data_cw_index++];
		}
	}
	for (int block2_index = 0; block2_index < rs_block_count2; ++block2_index) {
		for (auto j : std::views::iota(0, data_cw_count2)) {
			if (j < data_cw_count1) {
				all_cw[(block_sum * j) + block_no] = data_cw[data_cw_index++];
			}
			else {
				all_cw[(block_sum * data_cw_count1) + block2_index] = data_cw[data_cw_index++];
			}
		}
		++block_no;
	}
	const int rs_cw_count1 = qr_version_info_data[qr_version].rs_block_info1[qr_level].total_codeword_count - data_cw_count1;
	const int rs_cw_count2 = qr_version_info_data[qr_version].rs_block_info2[qr_level].total_codeword_count - data_cw_count2;
	data_cw_index      = 0;
	block_no          = 0;
	for (block_no = 0; block_no < rs_block_count1; ++block_no) {
		std::fill(rs_buf.begin(), rs_buf.end(), 0);
		std::copy_n(data_cw.data() + data_cw_index, data_cw_count1, rs_buf.data());
		get_rs_codeword(rs_buf.data(), data_cw_count1, rs_cw_count1);
		for (auto j : std::views::iota(0, rs_cw_count1)) {
			all_cw[data_codeword_count + (block_sum * j) + block_no] = rs_buf[j];
		}
		data_cw_index += data_cw_count1;
	}
	for (block_no = 0; block_no < rs_block_count2; ++block_no) {
		std::fill(rs_buf.begin(), rs_buf.end(), 0);
		std::copy_n(data_cw.data() + data_cw_index, data_cw_count2, rs_buf.data());
		get_rs_codeword(rs_buf.data(), data_cw_count2, rs_cw_count2);
		for (auto j : std::views::iota(0, rs_cw_count2)) {
			all_cw[data_codeword_count + (block_sum * j) + block_no] = rs_buf[j];
		}
		data_cw_index += data_cw_count2;
	}
	symbol_size = qr_version * 4 + 17;
	format_module();
	return true;
}
int get_encode_version(int version, char *source, int source_len)
{
	const int ver_group = version >= 27 ? k_qr_version_l : (version >= 10 ? k_qr_version_m : k_qr_version_s);
	for (auto group : std::views::iota(ver_group, k_qr_version_l + 1)) {
		if (encode_source_data(source, source_len, group)) {
			const int begin = (group == k_qr_version_s) ? 1 : (group == k_qr_version_m ? 10 : 27);
			const int end   = (group == k_qr_version_s) ? 9 : (group == k_qr_version_m ? 26 : 40);
                        auto      view  = std::span{qr_version_info_data}.subspan(
                            (std::size_t)begin,
                            (std::size_t)(end - begin + 1));
			for (auto i : std::views::iota(0, (int)view.size())) {
				const auto& info = view[(std::size_t)i];
				if ((data_codeword_bit + 7) / 8 <= info.data_codeword_count[qr_level])
					return begin + i;
			}
		}
	}
	return 0;
}
int encode_source_data(char *source, int source_len, int ver_group)
{
	int           src_bits, dst_bits;
	int           block     = 0;
	int           complete_len = 0;
	std::uint16_t bin_code;
	auto block_modes_view = block_modes();
	auto block_lengths_view = block_lengths();
	std::fill(block_lengths_view.begin(), block_lengths_view.end(), 0);
	data_block_count = 0;
	for (auto i : std::views::iota(0, source_len)) {
		std::uint8_t byMode;
		if (i < source_len - 1 && is_kanji_data(source[i], source[i + 1]))
			byMode = k_qr_mode_kanji;
		else if (is_numeral_data(source[i]))
			byMode = k_qr_mode_numeral;
		else if (is_alphabet_data(source[i]))
			byMode = k_qr_mode_alphabet;
		else
			byMode = k_qr_mode_8bit;
		if (i == 0)
			block_modes_view[0] = byMode;
		if (block_modes_view[data_block_count] != byMode)
			block_modes_view[++data_block_count] = byMode;
		++block_lengths_view[data_block_count];
		if (byMode == k_qr_mode_kanji) {
			++block_lengths_view[data_block_count];
			++i;
		}
	}
	++data_block_count;
	while (block < data_block_count - 1) {
		if ((block_modes_view[block] == k_qr_mode_numeral && block_modes_view[block + 1] == k_qr_mode_alphabet) ||
			(block_modes_view[block] == k_qr_mode_alphabet && block_modes_view[block + 1] == k_qr_mode_numeral)) {
			src_bits = get_bit_length(block_modes_view[block], block_lengths_view[block], ver_group) +
				get_bit_length(block_modes_view[block + 1], block_lengths_view[block + 1], ver_group);
			dst_bits = get_bit_length(k_qr_mode_alphabet, block_lengths_view[block] + block_lengths_view[block + 1], ver_group);
			if (src_bits > dst_bits) {
				int join_position = 0;
				int join_front;
				int join_behind;
				if (block >= 1 && block_modes_view[block - 1] == k_qr_mode_8bit) {
					join_front = get_bit_length(k_qr_mode_8bit, block_lengths_view[block - 1] + block_lengths_view[block],
					                           ver_group) +
						get_bit_length(block_modes_view[block + 1], block_lengths_view[block + 1], ver_group);
					if (join_front > dst_bits + get_bit_length(k_qr_mode_8bit, block_lengths_view[block - 1], ver_group))
						join_front = 0;
				}
				else
					join_front = 0;
				if (block < data_block_count - 2 && block_modes_view[block + 2] == k_qr_mode_8bit) {
					join_behind = get_bit_length(block_modes_view[block], block_lengths_view[block], ver_group) +
						get_bit_length(k_qr_mode_8bit, block_lengths_view[block + 1] + block_lengths_view[block + 2], ver_group);
					if (join_behind > dst_bits + get_bit_length(k_qr_mode_8bit, block_lengths_view[block + 2], ver_group))
						join_behind = 0;
				}
				else
					join_behind = 0;
				if (join_front != 0 && join_behind != 0) {
					join_position = (join_front < join_behind) ? -1 : 1;
				}
				else {
					join_position = (join_front != 0) ? -1 : ((join_behind != 0) ? 1 : 0);
				}
				if (join_position != 0) {
					if (join_position == -1) {
						block_lengths_view[block - 1] += block_lengths_view[block];
						for (auto i = block; i < data_block_count - 1; ++i) {
							block_modes_view[i]  = block_modes_view[i + 1];
							block_lengths_view[i] = block_lengths_view[i + 1];
						}
					}
					else {
						block_modes_view[block + 1]  = k_qr_mode_8bit;
						block_lengths_view[block + 1] += block_lengths_view[block + 2];
						for (auto i = block + 2; i < data_block_count - 1; ++i) {
							block_modes_view[i]  = block_modes_view[i + 1];
							block_lengths_view[i] = block_lengths_view[i + 1];
						}
					}
					--data_block_count;
				}
				else {
					if (block < data_block_count - 2 && block_modes_view[block + 2] == k_qr_mode_alphabet) {
						block_lengths_view[block + 1] += block_lengths_view[block + 2];
						for (auto i = block + 2; i < data_block_count - 1; ++i) {
							block_modes_view[i]  = block_modes_view[i + 1];
							block_lengths_view[i] = block_lengths_view[i + 1];
						}
						--data_block_count;
					}
					block_modes_view[block]  = k_qr_mode_alphabet;
					block_lengths_view[block] += block_lengths_view[block + 1];
					for (auto i = block + 1; i < data_block_count - 1; ++i) {
						block_modes_view[i]  = block_modes_view[i + 1];
						block_lengths_view[i] = block_lengths_view[i + 1];
					}
					--data_block_count;
					if (block >= 1 && block_modes_view[block - 1] == k_qr_mode_alphabet) {
						block_lengths_view[block - 1] += block_lengths_view[block];
						for (auto i = block; i < data_block_count - 1; ++i) {
							block_modes_view[i]  = block_modes_view[i + 1];
							block_lengths_view[i] = block_lengths_view[i + 1];
						}
						--data_block_count;
					}
				}
				continue;
			}
		}
		++block;
	}
	block = 0;
	while (block < data_block_count - 1) {
		src_bits = get_bit_length(block_modes_view[block], block_lengths_view[block], ver_group)
			+ get_bit_length(block_modes_view[block + 1], block_lengths_view[block + 1], ver_group);
		dst_bits = get_bit_length(k_qr_mode_8bit, block_lengths_view[block] + block_lengths_view[block + 1], ver_group);
		if (block >= 1 && block_modes_view[block - 1] == k_qr_mode_8bit)
			dst_bits -= (4 + indicator_len_8bit[ver_group]);
		if (block < data_block_count - 2 && block_modes_view[block + 2] == k_qr_mode_8bit)
			dst_bits -= (4 + indicator_len_8bit[ver_group]);
		if (src_bits > dst_bits) {
			if (block >= 1 && block_modes_view[block - 1] == k_qr_mode_8bit) {
				block_lengths_view[block - 1] += block_lengths_view[block];
				for (auto i = block; i < data_block_count - 1; ++i) {
					block_modes_view[i]  = block_modes_view[i + 1];
					block_lengths_view[i] = block_lengths_view[i + 1];
				}
				--data_block_count;
				--block;
			}
			if (block < data_block_count - 2 && block_modes_view[block + 2] == k_qr_mode_8bit) {
				block_lengths_view[block + 1] += block_lengths_view[block + 2];
				for (auto i = block + 2; i < data_block_count - 1; ++i) {
					block_modes_view[i]  = block_modes_view[i + 1];
					block_lengths_view[i] = block_lengths_view[i + 1];
				}
				--data_block_count;
			}
			block_modes_view[block]  = k_qr_mode_8bit;
			block_lengths_view[block] += block_lengths_view[block + 1];
			for (auto i = block + 1; i < data_block_count - 1; ++i) {
				block_modes_view[i]  = block_modes_view[i + 1];
				block_lengths_view[i] = block_lengths_view[i + 1];
			}
			--data_block_count;
			if (block >= 1)
				--block;
			continue;
		}
		++block;
	}
	data_codeword_bit = 0;
	std::fill(data_codewords().begin(), data_codewords().end(), 0);
	for (auto i : std::views::iota(0, data_block_count)) {
		if (data_codeword_bit == -1)
			break;
		if (block_modes_view[i] == k_qr_mode_numeral) {
			data_codeword_bit = set_bit_stream(data_codeword_bit, 1, 4);
			data_codeword_bit = set_bit_stream(data_codeword_bit, (std::uint16_t)block_lengths_view[i],
			                                   indicator_len_numeral[ver_group]);
			for (auto j : std::views::iota(0, (int)block_lengths_view[i])) {
				if ((j % 3) != 0)
					continue;
				if (j < block_lengths_view[i] - 2) {
					bin_code = (std::uint16_t)(((source[complete_len + j] - '0') * 100) +
						((source[complete_len + j + 1] - '0') * 10) +
						(source[complete_len + j + 2] - '0'));
					data_codeword_bit = set_bit_stream(data_codeword_bit, bin_code, 10);
				}
				else if (j == block_lengths_view[i] - 2) {
					bin_code = (std::uint16_t)(((source[complete_len + j] - '0') * 10) +
						(source[complete_len + j + 1] - '0'));
					data_codeword_bit = set_bit_stream(data_codeword_bit, bin_code, 7);
				}
				else if (j == block_lengths_view[i] - 1) {
					bin_code            = (std::uint16_t)(source[complete_len + j] - '0');
					data_codeword_bit = set_bit_stream(data_codeword_bit, bin_code, 4);
				}
			}
			complete_len += block_lengths_view[i];
		}
		else if (block_modes_view[i] == k_qr_mode_alphabet) {
			data_codeword_bit = set_bit_stream(data_codeword_bit, 2, 4);
			data_codeword_bit = set_bit_stream(data_codeword_bit, (std::uint16_t)block_lengths_view[i],
			                                   indicator_len_alphabet[ver_group]);
			for (auto j : std::views::iota(0, (int)block_lengths_view[i])) {
				if ((j % 2) != 0)
					continue;
				if (j < block_lengths_view[i] - 1) {
					bin_code = (std::uint16_t)((alphabet_to_binary(source[complete_len + j]) * 45) +
						alphabet_to_binary(source[complete_len + j + 1]));
					data_codeword_bit = set_bit_stream(data_codeword_bit, bin_code, 11);
				}
				else {
					bin_code            = (std::uint16_t)alphabet_to_binary(source[complete_len + j]);
					data_codeword_bit = set_bit_stream(data_codeword_bit, bin_code, 6);
				}
			}
			complete_len += block_lengths_view[i];
		}
		else if (block_modes_view[i] == k_qr_mode_8bit) {
			data_codeword_bit = set_bit_stream(data_codeword_bit, 4, 4);
			data_codeword_bit = set_bit_stream(data_codeword_bit, (std::uint16_t)block_lengths_view[i],
			                                   indicator_len_8bit[ver_group]);
			for (auto j : std::views::iota(0, (int)block_lengths_view[i])) {
				data_codeword_bit = set_bit_stream(data_codeword_bit, (std::uint16_t)source[complete_len + j], 8);
			}
			complete_len += block_lengths_view[i];
		}
		else {
			data_codeword_bit = set_bit_stream(data_codeword_bit, 8, 4);
			data_codeword_bit = set_bit_stream(data_codeword_bit, (std::uint16_t)(block_lengths_view[i] / 2),
			                                   indicator_len_kanji[ver_group]);
			for (auto j : std::views::iota(0, (int)(block_lengths_view[i] / 2))) {
				bin_code = kanji_to_binary(
					(std::uint16_t)(((std::uint8_t)source[complete_len + (j * 2)] << 8) + (std::uint8_t)source[
						complete_len + (j * 2) + 1]));
				data_codeword_bit = set_bit_stream(data_codeword_bit, bin_code, 13);
			}
			complete_len += block_lengths_view[i];
		}
	}
	return (data_codeword_bit != -1);
}
int get_bit_length(std::uint8_t mode, int data_len, int ver_group)
{
	int ncBits = 0;
	switch (mode) {
	case k_qr_mode_numeral:
		ncBits = 4 + indicator_len_numeral[ver_group] + (10 * (data_len / 3));
		switch (data_len % 3) {
		case 1: ncBits += 4;
			break;
		case 2: ncBits += 7;
			break;
		default: break;
		}
		break;
	case k_qr_mode_alphabet: ncBits = 4 + indicator_len_alphabet[ver_group] + (11 * (data_len / 2)) + (6 * (data_len % 2));
		break;
	case k_qr_mode_8bit: ncBits = 4 + indicator_len_8bit[ver_group] + (8 * data_len);
		break;
	default: ncBits = 4 + indicator_len_kanji[ver_group] + (13 * data_len);
		break;
	}
	return ncBits;
}

int set_bit_stream(int bit_index, std::uint16_t data_word, int data_len)
{
	if (bit_index == -1 || bit_index + data_len > kMaxDataCodeword * 8)
		return -1;
	auto data_cw = data_codewords();
	for (const auto i : std::views::iota(0, data_len)) {
		if (data_word & (1 << (data_len - i - 1))) {
			data_cw[(bit_index + i) / 8] |= 1 << (7 - ((bit_index + i) % 8));
		}
	}
	return bit_index + data_len;
}
bool is_numeral_data(std::uint8_t c)
{
	if (c >= '0' && c <= '9')
		return true;
	return false;
}
bool is_alphabet_data(std::uint8_t c)
{
	if (c >= '0' && c <= '9')
		return true;
	if (c >= 'A' && c <= 'Z')
		return true;
	if (c == ' ' || c == '$' || c == '%' || c == '*' || c == '+' || c == '-' || c == '.' || c == '/' || c == ':')
		return true;
	return false;
}
bool is_kanji_data(std::uint8_t c1, std::uint8_t c2)
{
	if (((c1 >= 0x81 && c1 <= 0x9f) || (c1 >= 0xe0 && c1 <= 0xeb)) && (c2 >= 0x40)) {
		if ((c1 == 0x9f && c2 > 0xfc) || (c1 == 0xeb && c2 > 0xbf))
			return false;
		return true;
	}
	return false;
}
bool is_chinese_data(std::uint8_t c1, std::uint8_t c2)
{
	if ((c1 >= 0xa1 && c1 < 0xaa) || (c1 >= 0xb0 && c1 <= 0xfa)) {
		if (c2 >= 0xa1 && c2 <= 0xfe) return true;
	}
	return false;
}
std::uint8_t alphabet_to_binary(std::uint8_t c)
{
	if (c >= '0' && c <= '9') return (std::uint8_t)(c - '0');
	if (c >= 'A' && c <= 'Z') return (std::uint8_t)(c - 'A' + 10);
	if (c == ' ') return 36;
	if (c == '$') return 37;
	if (c == '%') return 38;
	if (c == '*') return 39;
	if (c == '+') return 40;
	if (c == '-') return 41;
	if (c == '.') return 42;
	if (c == '/') return 43;
	return 44;
}
std::uint16_t kanji_to_binary(std::uint16_t wc)
{
	if (wc >= 0x8140 && wc <= 0x9ffc)
		wc -= 0x8140;
	else
		wc -= 0xc140;
	return (std::uint16_t)(((wc >> 8) * 0xc0) + (wc & 0x00ff));
}
std::uint16_t chinese_to_binary(std::uint16_t wc)
{
	if (wc >= 0xa1a1 && wc <= 0xa9fe) {
		return (std::uint16_t)((((wc >> 8) - 0xa1) * 0x60) + ((wc & 0x00ff) - 0xa1));
	}
	if (wc >= 0xb0a1 && wc <= 0xf9fe) {
		return (std::uint16_t)((((wc >> 8) - 0xa6) * 0x60) + ((wc & 0x00ff) - 0xa1));
	}
	return (std::uint16_t)((((wc >> 8) - 0xa6) * 0x60) + ((wc & 0x00ff) - 0xa1));
}
void get_rs_codeword(std::uint8_t *rs_work_buffer, int data_codeword_count, int rs_codeword_count)
{
	const auto exp_table = rs_exp_table[rs_codeword_count];
	for (int remaining = data_codeword_count; remaining > 0; --remaining) {
		if (rs_work_buffer[0] != 0) {
			std::uint8_t exp_first = int_to_exp_table[rs_work_buffer[0]];
			int          j         = 0;
			for (std::uint8_t exp : exp_table) {
				std::uint8_t exp_element = (std::uint8_t)((exp + exp_first) % 255);
				rs_work_buffer[j]            = (std::uint8_t)(rs_work_buffer[j + 1] ^ exp_to_int_table[exp_element]);
				++j;
			}
			for (auto j : std::views::iota(rs_codeword_count, data_codeword_count + rs_codeword_count - 1))
				rs_work_buffer[j] = rs_work_buffer[j + 1];
		}
		else {
			for (auto j : std::views::iota(0, data_codeword_count + rs_codeword_count - 1))
				rs_work_buffer[j] = rs_work_buffer[j + 1];
		}
	}
}
void format_module()
{
	std::fill(&module_data_buf[0][0],
		&module_data_buf[0][0] + (kMaxModuleSize * kMaxModuleSize),
		std::uint8_t{0});
	set_function_module();
	set_codeword_pattern();
	if (masking_no == -1) {
		masking_no = 0;
		set_masking_pattern(masking_no);
		set_format_info_pattern(masking_no);
		int nMipenalty = count_penalty();
		for (auto i : std::views::iota(1, 8)) {
			set_masking_pattern(i);
			set_format_info_pattern(i);
			const int penalty = count_penalty();
			if (penalty < nMipenalty) {
				nMipenalty  = penalty;
				masking_no = i;
			}
		}
	}
	set_masking_pattern(masking_no);
	set_format_info_pattern(masking_no);
	for (auto x : std::views::iota(0, symbol_size)) {
		for (auto y : std::views::iota(0, symbol_size)) {
			module_set(x, y, (module_at(x, y) & 0x11) != 0);
		}
	}
}
void set_function_module()
{
	set_finder_pattern(0, 0);
	set_finder_pattern(symbol_size - 7, 0);
	set_finder_pattern(0, symbol_size - 7);
	for (auto i : std::views::iota(0, 8)) {
		module_set(i, 7, '\x20');
		module_set(7, i, '\x20');
		module_set(symbol_size - 8, i, '\x20');
		module_set(symbol_size - 8 + i, 7, '\x20');
		module_set(i, symbol_size - 8, '\x20');
		module_set(7, symbol_size - 8 + i, '\x20');
	}
	for (auto i : std::views::iota(0, 9)) {
		module_set(i, 8, '\x20');
		module_set(8, i, '\x20');
	}
	for (auto i : std::views::iota(0, 8)) {
		module_set(symbol_size - 8 + i, 8, '\x20');
		module_set(8, symbol_size - 8 + i, '\x20');
	}
	set_version_pattern();
	const auto align_points = std::span(
		qr_version_info_data[qr_version].align_points_buf,
		qr_version_info_data[qr_version].align_point_count);
	for (auto ax : align_points) {
		set_alignment_pattern(ax, 6);
		set_alignment_pattern(6, ax);
		for (auto ay : align_points) {
			set_alignment_pattern(ax, ay);
		}
	}
	for (auto i : std::views::iota(8, symbol_size - 8)) {
		const std::uint8_t v = (i % 2) == 0 ? '\x30' : '\x20';
		module_set(i, 6, v);
		module_set(6, i, v);
	}
}
void set_finder_pattern(int x, int y)
{
	static std::uint8_t byPattern[] = { 0x7f, 0x41, 0x5d, 0x5d, 0x5d, 0x41, 0x7f };
	for (auto i : std::views::iota(0, 7)) {
		for (auto j : std::views::iota(0, 7)) {
			module_set(x + j, y + i, (byPattern[i] & (1 << (6 - j))) ? '\x30' : '\x20');
		}
	}
}

void set_alignment_pattern(int x, int y)
{
	static std::uint8_t byPattern[] = { 0x1f, 0x11, 0x15, 0x11, 0x1f };
	if (module_reserved(x, y))
		return;
	x -= 2;
	y -= 2;
	for (auto i : std::views::iota(0, 5)) {
		for (auto j : std::views::iota(0, 5)) {
			module_set(x + j, y + i, (byPattern[i] & (1 << (4 - j))) ? '\x30' : '\x20');
		}
	}
}

void set_version_pattern()
{
	if (qr_version <= 6)
		return;
	int ver_data = qr_version << 12;
	for (auto i : std::views::iota(0, 6)) {
		if (ver_data & (1 << (17 - i))) {
			ver_data ^= (0x1f25 << (5 - i));
		}
	}
	ver_data += qr_version << 12;
	for (auto i : std::views::iota(0, 6)) {
		for (auto j : std::views::iota(0, 3)) {
			const std::uint8_t v = (ver_data & (1 << (i * 3 + j))) ? '\x30' : '\x20';
			module_set(symbol_size - 11 + j, i, v);
			module_set(i, symbol_size - 11 + j, v);
		}
	}
}

void set_codeword_pattern()
{
	int x       = symbol_size;
	int y       = symbol_size - 1;
	int nCoef_x = 1;
	int nCoef_y = 1;
	for (auto i : std::views::iota(0, all_codeword_count)) {
		for (auto j : std::views::iota(0, 8)) {
			do {
				x       += nCoef_x;
				nCoef_x *= -1;
				if (nCoef_x < 0) {
					y += nCoef_y;
					if (y < 0 || y == symbol_size) {
						y       = (y < 0) ? 0 : symbol_size - 1;
						nCoef_y *= -1;
						x       -= 2;
						if (x == 6)
							--x;
					}
				}
			}
			while (module_reserved(x, y));
			module_set(x, y, (all_codewords()[i] & (1 << (7 - j))) ? '\x02' : '\x00');
		}
	}
}

void set_masking_pattern(int pattern_no)
{
	bool bMask;
	for (auto i : std::views::iota(0, symbol_size)) {
		for (auto j : std::views::iota(0, symbol_size)) {
			if (!module_reserved(j, i)) {
				switch (pattern_no) {
				case 0: bMask = ((i + j) % 2 == 0) ? true : false;
					break;
				case 1: bMask = (i % 2 == 0) ? true : false;
					break;
				case 2: bMask = (j % 3 == 0) ? true : false;
					break;
				case 3: bMask = ((i + j) % 3 == 0) ? true : false;
					break;
				case 4: bMask = (((i / 2) + (j / 3)) % 2 == 0) ? true : false;
					break;
				case 5: bMask = (((i * j) % 2) + ((i * j) % 3) == 0) ? true : false;
					break;
				case 6: bMask = ((((i * j) % 2) + ((i * j) % 3)) % 2 == 0) ? true : false;
					break;
				default: bMask = ((((i * j) % 3) + ((i + j) % 2)) % 2 == 0) ? true : false;
					break;
				}
				module_set(j, i, (std::uint8_t)((module_at(j, i) & 0xfe) | (((module_at(j, i) & 0x02) > 1) ^ bMask)));
			}
		}
	}
}

void set_format_info_pattern(int pattern_no)
{
	int format_info;
	switch (qr_level) {
	case k_qr_level_m: format_info = 0x00;
		break;
	case k_qr_level_l: format_info = 0x08;
		break;
	case k_qr_level_q: format_info = 0x18;
		break;
	default: format_info = 0x10;
		break;
	}
	format_info     += pattern_no;
	int format_data = format_info << 10;
	for (auto i : std::views::iota(0, 5)) {
		if (format_data & (1 << (14 - i))) {
			format_data ^= (0x0537 << (4 - i));
		}
	}
	format_data += format_info << 10;
	format_data ^= 0x5412;
	for (auto i : std::views::iota(0, 6))
		module_set(8, i, (format_data & (1 << i)) ? '\x30' : '\x20');
	module_set(8, 7, (format_data & (1 << 6)) ? '\x30' : '\x20');
	module_set(8, 8, (format_data & (1 << 7)) ? '\x30' : '\x20');
	module_set(7, 8, (format_data & (1 << 8)) ? '\x30' : '\x20');
	for (auto i : std::views::iota(9, 15))
		module_set(14 - i, 8, (format_data & (1 << i)) ? '\x30' : '\x20');
	for (auto i : std::views::iota(0, 8))
		module_set(symbol_size - 1 - i, 8, (format_data & (1 << i)) ? '\x30' : '\x20');
	module_set(8, symbol_size - 8, '\x30');
	for (auto i : std::views::iota(8, 15))
		module_set(8, symbol_size - 15 + i, (format_data & (1 << i)) ? '\x30' : '\x20');
}

int count_penalty()
{
	int penalty = 0;
	auto penalty_run_length = [&](auto is_dark) {
		int penalty = 0;
		int j = 0;
		while (j < symbol_size - 4) {
			int nRun = 1;
			int k = j + 1;
			for (; k < symbol_size; ++k) {
				if (is_dark(j) == is_dark(k))
					++nRun;
				else
					break;
			}
			if (nRun >= 5) {
				penalty += 3 + (nRun - 5);
			}
			j = k;
		}
		return penalty;
	};
	auto penalty_2x2 = [&]() {
		int penalty = 0;
		for (auto i : std::views::iota(0, symbol_size - 1)) {
			for (auto j : std::views::iota(0, symbol_size - 1)) {
				if ((((module_at(i, j) & 0x11) == 0) == ((module_at(i + 1, j) & 0x11) == 0)) &&
					(((module_at(i, j) & 0x11) == 0) == ((module_at(i, j + 1) & 0x11) == 0)) &&
					(((module_at(i, j) & 0x11) == 0) == ((module_at(i + 1, j + 1) & 0x11) == 0))) {
					penalty += 3;
				}
			}
		}
		return penalty;
	};
	auto has_finder_pattern = [&](auto is_dark, int j) -> bool {
		if (((j == 0) || !is_dark(j - 1)) &&
			is_dark(j) &&
			!is_dark(j + 1) &&
			is_dark(j + 2) &&
			is_dark(j + 3) &&
			is_dark(j + 4) &&
			!is_dark(j + 5) &&
			is_dark(j + 6) &&
			((j == symbol_size - 7) || !is_dark(j + 7))) {
			const bool pre = (j < 2 || !is_dark(j - 2)) &&
				(j < 3 || !is_dark(j - 3)) &&
				(j < 4 || !is_dark(j - 4));
			const bool post = (j >= symbol_size - 8 || !is_dark(j + 8)) &&
				(j >= symbol_size - 9 || !is_dark(j + 9)) &&
				(j >= symbol_size - 10 || !is_dark(j + 10));
			return pre || post;
		}
		return false;
	};
	auto penalty_pattern = [&](auto is_dark) {
		int penalty = 0;
		for (auto j : std::views::iota(0, symbol_size - 6)) {
			if (has_finder_pattern(is_dark, j)) {
				penalty += 40;
			}
		}
		return penalty;
	};
	auto penalty_balance = [&]() {
		int count = 0;
		for (auto i : std::views::iota(0, symbol_size)) {
			for (auto j : std::views::iota(0, symbol_size)) {
				if (!(module_at(i, j) & 0x11)) {
					++count;
				}
			}
		}
		int skew = (50 - ((count * 100) / (symbol_size * symbol_size)));
		if (skew < 0) {
			skew = -skew;
		}
		return (skew / 5) * 10;
	};
	for (auto i : std::views::iota(0, symbol_size)) {
		penalty += penalty_run_length([&](int col) { return (module_at(i, col) & 0x11) != 0; });
	}
	for (auto i : std::views::iota(0, symbol_size)) {
		penalty += penalty_run_length([&](int row) { return (module_at(row, i) & 0x11) != 0; });
	}
	penalty += penalty_2x2();
	for (auto i : std::views::iota(0, symbol_size)) {
		penalty += penalty_pattern([&](int col) { return (module_at(i, col) & 0x11) != 0; });
	}
	for (auto i : std::views::iota(0, symbol_size)) {
		penalty += penalty_pattern([&](int row) { return (module_at(row, i) & 0x11) != 0; });
	}
	penalty += penalty_balance();
	return penalty;
}

void print_2d_code()
{
	constexpr std::uint8_t kPrintHeader0 = 0x1b;
	constexpr std::uint8_t kPrintHeader1 = 0x2a;
	constexpr std::uint8_t kBlockBitH = 4;
	constexpr std::uint8_t kBlockBitV = 2;
	auto emit_block = [&](int row_index,
		std::uint8_t* print_buff,
		std::uint8_t print_num,
		int bit_v,
		int bit_h_minus1) {
		int x = 4;
		for (auto j : std::views::iota(0, symbol_size)) {
			const int k = row_index * bit_v;
			const auto top = module_at(j, k);
			const auto bot = module_at(j, k + 1);
			if (top == 1 && bot == 1) {
				print_buff[x++] = 0xFF;
			}
			else if (top == 1 && bot == 0) {
				print_buff[x++] = 0xF0;
			}
			else if (top == 0 && bot == 1) {
				print_buff[x++] = 0x0F;
			}
			else {
				print_buff[x++] = 0x00;
			}
			for (int y = 0; y < bit_h_minus1; ++y) {
				print_buff[x++] = print_buff[x - 1];
			}
		}
		for (int z = 0; z < static_cast<int>(print_num) + 4; ++z) {
		}
	};
	auto emit_tail = [&](int row_index,
		std::uint8_t* print_buff,
		std::uint8_t print_num,
		int bit_h_minus1) {
		int x = 4;
		for (auto j : std::views::iota(0, symbol_size)) {
			if (module_at(j, row_index) == 1) {
				print_buff[x++] = 0xF0;
			}
			else {
				print_buff[x++] = 0x00;
			}
			for (int y = 0; y < bit_h_minus1; ++y) {
				print_buff[x++] = print_buff[x - 1];
			}
		}
		for (int z = 0; z < static_cast<int>(print_num) + 4; ++z) {
		}
	};

	std::uint8_t       print_buff[256];
	const int          size      = symbol_size / kBlockBitV;
	const int          mod       = symbol_size % kBlockBitV;
	const std::uint8_t print_num = kBlockBitH * symbol_size;
	const int          bit_h_minus1 = static_cast<int>(kBlockBitH) - 1;
	for (auto i : std::views::iota(0, size)) {
		print_buff[0] = kPrintHeader0;
		print_buff[1] = kPrintHeader1;
		print_buff[2] = print_num;
		print_buff[3] = 0x00;
		emit_block(i, print_buff, print_num, kBlockBitV, bit_h_minus1);
	}
	if (mod != 0) {
		print_buff[0] = kPrintHeader0;
		print_buff[1] = kPrintHeader1;
		print_buff[2] = print_num;
		print_buff[3] = 0x00;
		emit_tail(size * kBlockBitV, print_buff, print_num, bit_h_minus1);
	}
}
} // namespace detail

#undef symbol_size
#undef module_data_buf
#undef data_codeword_bit
#undef data_codewords_buf
#undef data_block_count
#undef all_codeword_count
#undef all_codewords_buf
#undef rs_work_buf
#undef qr_level
#undef qr_version
#undef auto_extent
#undef masking_no

export namespace alg::qr {
// C++ helpers (const-correct wrappers). Minimal surface for callers.
struct EncoderState {
    alignas(detail::State) std::uint8_t storage[sizeof(detail::State)]{};

    EncoderState() noexcept
    {
        std::fill_n(storage, sizeof(storage), std::uint8_t{0});
    }
};

struct Encoder {
    [[nodiscard]] static bool encode(EncoderState& st, const char* text) noexcept
    {
        auto& state_ref = *std::launder(reinterpret_cast<detail::State*>(st.storage));
        auto* prev = detail::set_current(&state_ref);
        const bool ok = detail::encode_data(const_cast<char*>(text));
        detail::restore_current(prev);
        return ok;
    }

    [[nodiscard]] static int size(const EncoderState& st) noexcept
    {
        const auto& state_ref = *std::launder(reinterpret_cast<const detail::State*>(st.storage));
        return state_ref.symbol_size;
    }

    [[nodiscard]] static bool module_on(const EncoderState& st, int x, int y) noexcept
    {
        const auto& state_ref = *std::launder(reinterpret_cast<const detail::State*>(st.storage));
        return state_ref.module_data_buf[x][y] != 0;
    }
};
} // namespace alg::qr

