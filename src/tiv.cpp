/*
 * Copyright (c) 2017-2023, Stefan Haustein, Aaron Liu
 *
 * Despite what git blame might say, most of the code was made by Stefan
 * Haustein. The file history got messed up by my awful decision to move and
 * format a file in the same commit. View the history of src/cpp/tiv.cpp for the
 * original history.
 *
 *     This file is free software: you may copy, redistribute and/or modify it
 *     under the terms of the GNU General Public License as published by the
 *     Free Software Foundation, either version 3 of the License, or (at your
 *     option) any later version.
 *
 *     This file is distributed in the hope that it will be useful, but
 *     WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *     General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Alternatively, you may copy, redistribute and/or modify this file under
 * the terms of the Apache License, version 2.0:
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         https://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <array>
#include <bitset>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// This #define tells CImg that we use the library without any display options,
// just for loading images.
#define cimg_display 0
#include "CImg.h"
// CImg defines its own min and max macros to compile, so we need to undef them
#undef min
#undef max

#ifdef _POSIX_VERSION
// Console output size detection
#include <sys/ioctl.h>
// Error explanation, for some reason
#include <cstring>
// Exit codes
#include <sysexits.h>
#endif

#ifdef _WIN32
// Console output size detection
#include <windows.h>
// Error explanation
#include <system_error>

// Following codes copied from /usr/include/sysexits.h,
// license: https://opensource.org/license/BSD-3-clause/
#define EX_OK 0         /* successful termination */
#define EX__BASE 64     /* base value for error messages */
#define EX_USAGE 64     /* command line usage error */
#define EX_DATAERR 65   /* data format error */
#define EX_NOINPUT 66   /* cannot open input */
#define EX_SOFTWARE 70  /* internal software error */
#define EX_CANTCREAT 73 /* can't create (user) output file */
#define EX_IOERR 74     /* input/output error */
#define EX_TEMPFAIL 75  /* temp failure; user is invited to retry */
#define EX_NOPERM 77    /* permission denied */
#define EX_CONFIG 78    /* configuration error */
#endif

// @TODO: Convert to bitset
// Implementation of flag representation for flags in the main() method
constexpr int FLAG_FG = 1;         // emit fg color
constexpr int FLAG_BG = 2;         // emit bg color
constexpr int FLAG_MODE_256 = 4;   // Limit colors to 256-color mode
constexpr int FLAG_24BIT = 8;      // 24-bit color mode
constexpr int FLAG_NOOPT = 16;     // Only use the same half-block character
constexpr int FLAG_TELETEXT = 32;  // Use teletext characters

// Color saturation value steps from 0 to 255
constexpr int COLOR_STEP_COUNT = 6;
constexpr int COLOR_STEPS[COLOR_STEP_COUNT] = {0, 0x5f, 0x87, 0xaf, 0xd7, 0xff};

// Grayscale saturation value steps from 0 to 255
constexpr int GRAYSCALE_STEP_COUNT = 24;
constexpr int GRAYSCALE_STEPS[GRAYSCALE_STEP_COUNT] = {
    0x08, 0x12, 0x1c, 0x26, 0x30, 0x3a, 0x44, 0x4e, 0x58, 0x62, 0x6c, 0x76,
    0x80, 0x8a, 0x94, 0x9e, 0xa8, 0xb2, 0xbc, 0xc6, 0xd0, 0xda, 0xe4, 0xee};

// An interleaved map of 4x8 bit character bitmaps (each hex digit represents a
// row) to the corresponding unicode character code point.
constexpr unsigned int BITMAPS[] = {
    0x00000000, 0x00a0,

    // Block graphics
    // 0xffff0000, 0x2580,  // upper 1/2; redundant with inverse lower 1/2

    0x0000000f, 0x2581,                      // lower 1/8
    0x000000ff, 0x2582,                      // lower 1/4
    0x00000fff, 0x2583, 0x0000ffff, 0x2584,  // lower 1/2
    0x000fffff, 0x2585, 0x00ffffff, 0x2586,  // lower 3/4
    0x0fffffff, 0x2587,
    // 0xffffffff, 0x2588,  // full; redundant with inverse space

    0xeeeeeeee, 0x258a,  // left 3/4
    0xcccccccc, 0x258c,  // left 1/2
    0x88888888, 0x258e,  // left 1/4

    0x0000cccc, 0x2596,  // quadrant lower left
    0x00003333, 0x2597,  // quadrant lower right
    0xcccc0000,
    0x2598,  // quadrant upper left
             // 0xccccffff, 0x2599,  // 3/4 redundant with inverse 1/4
    0xcccc3333, 0x259a,  // diagonal 1/2
                         // 0xffffcccc, 0x259b,  // 3/4 redundant
    // 0xffff3333, 0x259c,  // 3/4 redundant
    0x33330000, 0x259d,  // quadrant upper right
                         // 0x3333cccc, 0x259e,  // 3/4 redundant
    // 0x3333ffff, 0x259f,  // 3/4 redundant

    // Line drawing subset: no double lines, no complex light lines

    0x000ff000, 0x2501,  // Heavy horizontal
    0x66666666, 0x2503,  // Heavy vertical

    0x00077666, 0x250f,  // Heavy down and right
    0x000ee666, 0x2513,  // Heavy down and left
    0x66677000, 0x2517,  // Heavy up and right
    0x666ee000, 0x251b,  // Heavy up and left

    0x66677666, 0x2523,  // Heavy vertical and right
    0x666ee666, 0x252b,  // Heavy vertical and left
    0x000ff666, 0x2533,  // Heavy down and horizontal
    0x666ff000, 0x253b,  // Heavy up and horizontal
    0x666ff666, 0x254b,  // Heavy cross

    0x000cc000, 0x2578,  // Bold horizontal left
    0x00066000, 0x2579,  // Bold horizontal up
    0x00033000, 0x257a,  // Bold horizontal right
    0x00066000, 0x257b,  // Bold horizontal down

    0x06600660, 0x254f,  // Heavy double dash vertical

    0x000f0000, 0x2500,  // Light horizontal
    0x0000f000, 0x2500,  //
    0x44444444, 0x2502,  // Light vertical
    0x22222222, 0x2502,

    0x000e0000, 0x2574,  // light left
    0x0000e000, 0x2574,  // light left
    0x44440000, 0x2575,  // light up
    0x22220000, 0x2575,  // light up
    0x00030000, 0x2576,  // light right
    0x00003000, 0x2576,  // light right
    0x00004444, 0x2577,  // light down
    0x00002222, 0x2577,  // light down

    // Misc technical

    0x44444444, 0x23a2,  // [ extension
    0x22222222, 0x23a5,  // ] extension

    0x0f000000, 0x23ba,  // Horizontal scanline 1
    0x00f00000, 0x23bb,  // Horizontal scanline 3
    0x00000f00, 0x23bc,  // Horizontal scanline 7
    0x000000f0, 0x23bd,  // Horizontal scanline 9

    // Geometrical shapes. Tricky because some of them are too wide.

    // 0x00ffff00, 0x25fe,  // Black medium small square
    0x00066000, 0x25aa,  // Black small square

    // 0x11224488, 0x2571,  // diagonals
    // 0x88442211, 0x2572,
    // 0x99666699, 0x2573,
    // 0x000137f0, 0x25e2,  // Triangles
    // 0x0008cef0, 0x25e3,
    // 0x000fec80, 0x25e4,
    // 0x000f7310, 0x25e5,

    0, 0,  // End marker for "regular" characters

    // Teletext / legacy graphics 3x2 block character codes.
    // Using a 3-2-3 pattern consistently, perhaps we should create automatic
    // variations....

    0xccc00000, 0xfb00, 0x33300000, 0xfb01, 0xfff00000, 0xfb02, 0x000cc000,
    0xfb03, 0xccccc000, 0xfb04, 0x333cc000, 0xfb05, 0xfffcc000, 0xfb06,
    0x00033000, 0xfb07, 0xccc33000, 0xfb08, 0x33333000, 0xfb09, 0xfff33000,
    0xfb0a, 0x000ff000, 0xfb0b, 0xcccff000, 0xfb0c, 0x333ff000, 0xfb0d,
    0xfffff000, 0xfb0e, 0x00000ccc, 0xfb0f,

    0xccc00ccc, 0xfb10, 0x33300ccc, 0xfb11, 0xfff00ccc, 0xfb12, 0x000ccccc,
    0xfb13, 0x333ccccc, 0xfb14, 0xfffccccc, 0xfb15, 0x00033ccc, 0xfb16,
    0xccc33ccc, 0xfb17, 0x33333ccc, 0xfb18, 0xfff33ccc, 0xfb19, 0x000ffccc,
    0xfb1a, 0xcccffccc, 0xfb1b, 0x333ffccc, 0xfb1c, 0xfffffccc, 0xfb1d,
    0x00000333, 0xfb1e, 0xccc00333, 0xfb1f,

    0x33300333, 0x1b20, 0xfff00333, 0x1b21, 0x000cc333, 0x1b22, 0xccccc333,
    0x1b23, 0x333cc333, 0x1b24, 0xfffcc333, 0x1b25, 0x00033333, 0x1b26,
    0xccc33333, 0x1b27, 0xfff33333, 0x1b28, 0x000ff333, 0x1b29, 0xcccff333,
    0x1b2a, 0x333ff333, 0x1b2b, 0xfffff333, 0x1b2c, 0x00000fff, 0x1b2d,
    0xccc00fff, 0x1b2e, 0x33300fff, 0x1b2f,

    0xfff00fff, 0x1b30, 0x000ccfff, 0x1b31, 0xcccccfff, 0x1b32, 0x333ccfff,
    0x1b33, 0xfffccfff, 0x1b34, 0x00033fff, 0x1b35, 0xccc33fff, 0x1b36,
    0x33333fff, 0x1b37, 0xfff33fff, 0x1b38, 0x000fffff, 0x1b39, 0xcccfffff,
    0x1b3a, 0x333fffff, 0x1b3b,

    0, 1  // End marker for extended TELETEXT mode.
};

/**
 * @brief Struct to represent a character to be drawn.
 * @param fgColor RGB
 * @param bgColor RGB
 * @param codePoint The code point of the character to be drawn.
 */
struct CharData {
    std::array<int, 3> fgColor = std::array<int, 3>{0, 0, 0};
    std::array<int, 3> bgColor = std::array<int, 3>{0, 0, 0};
    int codePoint;
};

// Return a CharData struct with the given code point and corresponding
// average fg and bg colors.
CharData createCharData(const cimg_library::CImg<unsigned char> &image, int x0,
                        int y0, int codepoint, int pattern) {
    CharData result;
    result.codePoint = codepoint;
    int fg_count = 0;
    int bg_count = 0;
    unsigned int mask = 0x80000000;

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 4; x++) {
            int *avg;
            if (pattern & mask) {
                avg = result.fgColor.data();
                fg_count++;
            } else {
                avg = result.bgColor.data();
                bg_count++;
            }
            for (int i = 0; i < 3; i++) {
                avg[i] += image(x0 + x, y0 + y, 0, i);
            }
            mask = mask >> 1;
        }
    }

    // Calculate the average color value for each bucket
    for (int i = 0; i < 3; i++) {
        if (bg_count != 0) {
            result.bgColor[i] /= bg_count;
        }
        if (fg_count != 0) {
            result.fgColor[i] /= fg_count;
        }
    }
    return result;
}

/**
 * @brief Find the best character and colors for the given 4x8 area of the image
 *
 * @param image The image where the pixels reside
 * @param x0 The x coordinate of the top left pixel of the area
 * @param y0 The y coordinate of the top left pixel of the area
 * @param flags
 * @return The @ref CharData representation of the colors and character best
 * used to render the 4x8 area
 */
CharData findCharData(const cimg_library::CImg<unsigned char> &image, int x0,
                      int y0, const int8_t &flags) {
    int min[3] = {255, 255, 255};
    int max[3] = {0};
    std::map<long, int> count_per_color;

    // Determine the minimum and maximum value for each color channel
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 4; x++) {
            long color = 0;
            for (int i = 0; i < 3; i++) {
                int d = image(x0 + x, y0 + y, 0, i);
                min[i] = std::min(min[i], d);
                max[i] = std::max(max[i], d);

                color = (color << 8) | d;
            }
            count_per_color[color]++;
        }
    }

    std::multimap<int, long> color_per_count;
    for (auto i = count_per_color.begin(); i != count_per_color.end(); ++i) {
        color_per_count.insert(std::pair<int, long>(i->second, i->first));
    }

    auto iter = color_per_count.rbegin();
    int count2 = iter->first;
    long max_count_color_1 = iter->second;
    long max_count_color_2 = max_count_color_1;
    if ((++iter) != color_per_count.rend()) {
        count2 += iter->first;
        max_count_color_2 = iter->second;
    }

    unsigned int bits = 0;
    bool direct = count2 > (8 * 4) / 2;

    if (direct) {
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 4; x++) {
                bits = bits << 1;
                int d1 = 0;
                int d2 = 0;
                for (int i = 0; i < 3; i++) {
                    int shift = 16 - 8 * i;
                    int c1 = (max_count_color_1 >> shift) & 255;
                    int c2 = (max_count_color_2 >> shift) & 255;
                    int c = image(x0 + x, y0 + y, 0, i);
                    d1 += (c1 - c) * (c1 - c);
                    d2 += (c2 - c) * (c2 - c);
                }
                if (d1 > d2) {
                    bits |= 1;
                }
            }
        }
    } else {
        // Determine the color channel with the greatest range.
        int splitIndex = 0;
        int bestSplit = 0;
        for (int i = 0; i < 3; i++) {
            if (max[i] - min[i] > bestSplit) {
                bestSplit = max[i] - min[i];
                splitIndex = i;
            }
        }

        // We just split at the middle of the interval instead of computing the
        // median.
        int splitValue = min[splitIndex] + bestSplit / 2;

        // Compute a bitmap using the given split and sum the color values for
        // both buckets.
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 4; x++) {
                bits = bits << 1;
                if (image(x0 + x, y0 + y, 0, splitIndex) > splitValue) {
                    bits |= 1;
                }
            }
        }
    }

    // Find the best bitmap match by counting the bits that don't match,
    // including the inverted bitmaps.
    int best_diff = 8;
    unsigned int best_pattern = 0x0000ffff;
    int codepoint = 0x2584;
    bool inverted = false;
    unsigned int end_marker = flags & FLAG_TELETEXT ? 1 : 0;
    for (int i = 0; BITMAPS[i + 1] != end_marker; i += 2) {
        // Skip all end markers
        if (BITMAPS[i + 1] < 32) {
            continue;
        }
        unsigned int pattern = BITMAPS[i];
        for (int j = 0; j < 2; j++) {
            int diff = (std::bitset<32>(pattern ^ bits)).count();
            if (diff < best_diff) {
                best_pattern = BITMAPS[i];  // pattern might be inverted.
                codepoint = BITMAPS[i + 1];
                best_diff = diff;
                inverted = best_pattern != pattern;
            }
            pattern = ~pattern;
        }
    }

    if (direct) {
        CharData result;
        if (inverted) {
            long tmp = max_count_color_1;
            max_count_color_1 = max_count_color_2;
            max_count_color_2 = tmp;
        }
        for (int i = 0; i < 3; i++) {
            int shift = 16 - 8 * i;
            result.fgColor[i] = (max_count_color_2 >> shift) & 255;
            result.bgColor[i] = (max_count_color_1 >> shift) & 255;
            result.codePoint = codepoint;
        }
        return result;
    }
    return createCharData(image, x0, y0, codepoint, best_pattern);
}

int clamp_byte(int value) {
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

double sqr(double n) { return n * n; }

int best_index(int value, const int STEPS[], int count) {
    int best_diff = std::abs(STEPS[0] - value);
    int result = 0;
    for (int i = 1; i < count; i++) {
        int diff = std::abs(STEPS[i] - value);
        if (diff < best_diff) {
            result = i;
            best_diff = diff;
        }
    }
    return result;
}

std::string emitTermColor(const int8_t &flags, int r, int g, int b) {
    r = clamp_byte(r), g = clamp_byte(g), b = clamp_byte(b);

    const bool bg = (flags & FLAG_BG);

    if (!(flags & FLAG_MODE_256)) {
        // 2 means we output true (RGB) colors
        return std::format("\x1b[{};2;{};{};{}m", bg ? 48 : 38, r, g, b);
    }

    // Compute predefined color index from all 256 colors we should use

    int ri = best_index(r, COLOR_STEPS, COLOR_STEP_COUNT);
    int gi = best_index(g, COLOR_STEPS, COLOR_STEP_COUNT);
    int bi = best_index(b, COLOR_STEPS, COLOR_STEP_COUNT);

    int rq = COLOR_STEPS[ri];
    int gq = COLOR_STEPS[gi];
    int bq = COLOR_STEPS[bi];

    int gray =
        static_cast<int>(std::round(r * 0.2989f + g * 0.5870f + b * 0.1140f));

    int gri = best_index(gray, GRAYSCALE_STEPS, GRAYSCALE_STEP_COUNT);
    int grq = GRAYSCALE_STEPS[gri];

    int color_index;
    if (0.3 * sqr(rq - r) + 0.59 * sqr(gq - g) + 0.11 * sqr(bq - b) <
        0.3 * sqr(grq - r) + 0.59 * sqr(grq - g) + 0.11 * sqr(grq - b)) {
        color_index = 16 + 36 * ri + 6 * gi + bi;
    } else {
        color_index = 232 + gri;  // 1..24 -> 232..255
    }
    // 38 sets the foreground color and 48 sets the background color
    return std::format("\x1b[{};5;{}m", bg ? 48 : 38, color_index);
}

void emitCodepoint(int codepoint) {
    if (codepoint < 128) {
        std::cout << static_cast<char>(codepoint);
    } else if (codepoint < 0x7ff) {
        std::cout << static_cast<char>(0xc0 | (codepoint >> 6));
        std::cout << static_cast<char>(0x80 | (codepoint & 0x3f));
    } else if (codepoint < 0xffff) {
        std::cout << static_cast<char>(0xe0 | (codepoint >> 12));
        std::cout << static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
        std::cout << static_cast<char>(0x80 | (codepoint & 0x3f));
    } else if (codepoint < 0x10ffff) {
        std::cout << static_cast<char>(0xf0 | (codepoint >> 18));
        std::cout << static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
        std::cout << static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
        std::cout << static_cast<char>(0x80 | (codepoint & 0x3f));
    } else {
        std::cerr << "ERROR";
    }
}

std::string emitImage(const cimg_library::CImg<unsigned char> &image,
                      const int8_t &flags) {
    std::string ret;
    CharData lastCharData;
    for (int y = 0; y <= image.height() - 8; y += 8) {
        for (int x = 0; x <= image.width() - 4; x += 4) {
            // Create CharData for the current 4x8 area of the image
            // If only half-block chars are allowed, use predefined codepoint
            CharData charData =
                flags & FLAG_NOOPT
                    ? createCharData(image, x, y, 0x2584, 0x0000ffff)
                    : findCharData(image, x, y, flags);
            if (x == 0 || charData.bgColor != lastCharData.bgColor)
                ret += emitTermColor(flags | FLAG_BG, charData.bgColor[0],
                                     charData.bgColor[1], charData.bgColor[2]);
            if (x == 0 || charData.fgColor != lastCharData.fgColor)
                ret += emitTermColor(flags | FLAG_FG, charData.fgColor[0],
                                     charData.fgColor[1], charData.fgColor[2]);
            ret += (charData.codePoint);
            lastCharData = charData;
        }
        ret += "\x1b[0m\n";  // clear formatting until next batch
    }
    return ret;
}

/**
 * @brief Helper function to print a codepoint in a terminal-friendly way
 *
 * @param codepoint The codepoint to print
 */
void printCodepoint(int codepoint) {
    if (codepoint < 128) {  // ASCII
        std::cout << static_cast<char>(codepoint);
    } else if (codepoint < 0x7ff) {  // 2-byte UTF-8
        std::cout << static_cast<char>(0xc0 | (codepoint >> 6));
        std::cout << static_cast<char>(0x80 | (codepoint & 0x3f));
    } else if (codepoint < 0xffff) {  // 3-byte UTF-8
        std::cout << static_cast<char>(0xe0 | (codepoint >> 12));
        std::cout << static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
        std::cout << static_cast<char>(0x80 | (codepoint & 0x3f));
    } else if (codepoint < 0x10ffff) {  // 4-byte UTF-8
        std::cout << static_cast<char>(0xf0 | (codepoint >> 18));
        std::cout << static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
        std::cout << static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
        std::cout << static_cast<char>(0x80 | (codepoint & 0x3f));
    } else {  //???
        std::cerr << std::format(
            "Error: Codepoint 0x{:08x} is out of range, skipping this pixel",
            codepoint);
    }
}

/**
 * @brief Outputs the given image.
 *
 * @param image The image to output.
 * @param flags
 */
void printImage(const cimg_library::CImg<unsigned char> &image,
                const int8_t &flags) {
    CharData lastCharData;
    for (int y = 0; y <= image.height() - 8; y += 8) {
        for (int x = 0; x <= image.width() - 4; x += 4) {
            // Create CharData for the current 4x8 area of the image
            // If only half-block chars are allowed, use predefined codepoint
            CharData charData =
                flags & FLAG_NOOPT
                    ? createCharData(image, x, y, 0x2584, 0x0000ffff)
                    : findCharData(image, x, y, flags);
            if (x == 0 || charData.bgColor != lastCharData.bgColor)
                std::cout << emitTermColor(flags | FLAG_BG, charData.bgColor[0],
                                           charData.bgColor[1],
                                           charData.bgColor[2]);
            if (x == 0 || charData.fgColor != lastCharData.fgColor)
                std::cout << emitTermColor(flags | FLAG_FG, charData.fgColor[0],
                                           charData.fgColor[1],
                                           charData.fgColor[2]);
            printCodepoint(charData.codePoint);
            lastCharData = charData;
        }
        std::cout << "\x1b[0m\n";  // clear formatting until next batch
    }
}

struct size {
    size(unsigned int in_width, unsigned int in_height)
        : width(in_width), height(in_height) {}
    explicit size(cimg_library::CImg<unsigned int> img)
        : width(img.width()), height(img.height()) {}
    unsigned int width;
    unsigned int height;
    size scaled(double scale) { return size(width * scale, height * scale); }
    size fitted_within(size container) {
        double scale = std::min(container.width / static_cast<double>(width),
                                container.height / static_cast<double>(height));
        return scaled(scale);
    }
};
std::ostream &operator<<(std::ostream &stream, size sz) {
    stream << sz.width << "x" << sz.height;
    return stream;
}

/**
 * @brief Wrapper around CImg<T>(const char*) constructor
 * that always returns a CImg image with 3 channels (RGB)
 *
 * @param filename The file to construct a CImg object on
 * @return cimg_library::CImg<unsigned char> Constructed CImg RGB image
 */
cimg_library::CImg<unsigned char> load_rgb_CImg(const char *const &filename) {
    cimg_library::CImg<unsigned char> image(filename);
    if (image.spectrum() == 1) {
        // Greyscale. Just copy greyscale data to all channels
        cimg_library::CImg<unsigned char> rgb_image(
            image.width(), image.height(), image.depth(), 3);
        for (unsigned int chn = 0; chn < 3; chn++) {
            rgb_image.draw_image(0, 0, 0, chn, image);
        }
        return rgb_image;
    }
    return image;
}

// Implements --help
void printUsage() {
    std::cerr << R"(
Terminal Image Viewer v1.3
usage: tiv [options] <image> [<image>...]
-0        : No block character adjustment, always use top half block char.
-2, --256 : Use 256-bit colors. Needed to display properly on macOS Terminal.
-c <num>  : Number of thumbnail columns in 'dir' mode (3 by default).
-d, --dir : Force 'dir' mode. Automatially selected for more than one input.
-f, --full: Force 'full' mode. Automatically selected for one input.
--help    : Display this help text.
-h <num>  : Set the maximum output height to <num> lines.
-w <num>  : Set the maximum output width to <num> characters.
-x        : Use new Unicode Teletext/legacy characters (experimental).)"
              << std::endl;
}

enum Mode { AUTO, THUMBNAILS, FULL_SIZE };

int main(int argc, char *argv[]) {
    std::ios::sync_with_stdio(false);  // apparently makes printing faster
    bool detectSize = true;

    // Fallback sizes when unsuccesful. Sizes are actually 1/4th of the actual
    int maxWidth = 80;
    int maxHeight = 24;

    // Reading input
    int8_t flags = 0;  // bitwise representation of flags,
                       // see https://stackoverflow.com/a/14295472
    Mode mode = AUTO;  // either THUMBNAIL or FULL_SIZE
    int columns = 3;

    std::vector<std::string> file_names;
    int ret = EX_OK;  // The return code for the program

    if (argc <= 1) {
        printUsage();
        return EX_USAGE;
    }

    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "-0") {
            flags |= FLAG_NOOPT;
        } else if (arg == "-c") {
            if (i < argc - 1) {
                columns = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: -c requires a number" << std::endl;
                ret = EX_USAGE;
            }
        } else if (arg == "-d" || arg == "--dir") {
            mode = THUMBNAILS;
        } else if (arg == "-f" || arg == "--full") {
            mode = FULL_SIZE;
        } else if (arg == "-w") {
            if (i < argc - 1) {
                maxWidth = 4 * std::stoi(argv[++i]), detectSize = false;
            } else {
                std::cerr << "Error: -w requires a number" << std::endl;
                ret = EX_USAGE;
            }
        } else if (arg == "-h") {
            if (i < argc - 1)
                maxHeight = 8 * std::stoi(argv[++i]), detectSize = false;
            else
                printUsage();  // people might confuse this with help
        } else if (arg == "--256" || arg == "-2" || arg == "-256") {
            flags |= FLAG_MODE_256;
        } else if (arg == "--help" || arg == "-help") {
            printUsage();
        } else if (arg == "-x") {
            flags |= FLAG_TELETEXT;
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unrecognized argument: " << arg << std::endl;
            ret = EX_USAGE;
        } else {
            // Arguments that will be displayed
            if (std::filesystem::is_directory(arg)) {
                for (auto &p : std::filesystem::directory_iterator(arg))
                    if (std::filesystem::is_regular_file(p.path()))
                        file_names.push_back(p.path().string());
            } else {
                // Check if file can be opened, @TODO find better way
                std::ifstream fin(arg.c_str());
                if (fin) {
                    file_names.push_back(arg);
                } else {
                    std::cerr << "Error: Cannot open '" << arg
                              << "', permission issue?" << std::endl;
                    ret = EX_NOINPUT;
                }
            }
        }
    }

    if (detectSize) {
        // Platform-specific implementations for determining console size,
        // better implementations are welcome
#ifdef _POSIX_VERSION
        struct winsize w;
        // If redirecting STDOUT to one file ( col or row == 0, or the previous
        // ioctl call's failed )
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != 0 ||
            (w.ws_col | w.ws_row) == 0) {
            std::cerr << "Warning: failed to determine most reasonable size: "
                      << strerror(errno) << ", defaulting to 20x6" << std::endl;
        } else {
            maxWidth = w.ws_col * 4;
            maxHeight = w.ws_row * 8;
        }
#elif defined _WIN32
        CONSOLE_SCREEN_BUFFER_INFO w;
        if (GetConsoleScreenBufferInfo(
                GetStdHandle(STD_OUTPUT_HANDLE),
                &w)) {  // just like powershell, but without the hyphens, hooray
            maxWidth = w.dwSize.X * 4;
            maxHeight = w.dwSize.Y * 8;
        } else {
            std::cerr << "Warning: failed to determine most reasonable size: "
                      << std::system_category().message(GetLastError())
                      << ", defaulting to 20x6" << std::endl;
        }
#else
        std::cerr << "Warning: failed to determine most reasonable size: "
                     "unrecognized system, defaulting to 20x6"
                  << std::endl;
#endif
    }

    if (mode == FULL_SIZE || (mode == AUTO && file_names.size() == 1)) {
        for (const auto &filename : file_names) {
            try {
                cimg_library::CImg<unsigned char> image =
                    load_rgb_CImg(filename.c_str());
                if (image.width() > maxWidth || image.height() > maxHeight) {
                    // scale image down to fit terminal size
                    size new_size =
                        size(image).fitted_within(size(maxWidth, maxHeight));
                    image.resize(new_size.width, new_size.height, -100, -100,
                                 5);
                }
                // the actual magick which generates the output
                printImage(image, flags);
                std::cout.flush();  // replaces last endl to make sure we get
                                    // output on screen
            } catch (cimg_library::CImgIOException &e) {
                std::cerr << "Error: '" << filename
                          << "' has an unrecognized file format" << std::endl;
                ret = EX_DATAERR;
            }
        }
    } else {  // Thumbnail mode
        unsigned int index = 0;
        int cw = (((maxWidth / 4) - 2 * (columns - 1)) / columns);
        int tw = cw * 4;
        cimg_library::CImg<unsigned char> image(
            tw * columns + 2 * 4 * (columns - 1), tw, 1, 3);
        size maxThumbSize(tw, tw);

        while (index < file_names.size()) {
            image.fill(0);
            int count = 0;
            std::string sb;
            while (index < file_names.size() && count < columns) {
                std::string name = file_names[index++];
                try {
                    cimg_library::CImg<unsigned char> original =
                        load_rgb_CImg(name.c_str());
                    auto cut = name.find_last_of("/");
                    sb +=
                        cut == std::string::npos ? name : name.substr(cut + 1);
                    size newSize = size(original).fitted_within(maxThumbSize);
                    original.resize(newSize.width, newSize.height, 1, -100, 5);
                    image.draw_image(
                        count * (tw + 8) + (tw - newSize.width) / 2,
                        (tw - newSize.height) / 2, 0, 0, original);
                    count++;
                    unsigned int sl = count * (cw + 2);
                    sb.resize(sl - 2, ' ');
                    sb += "  ";
                } catch (std::exception &e) {
                    // Probably no image; ignore.
                }
            }
            if (count) printImage(image, flags);
            std::cout << sb << '\n' << std::endl;
        }
    }
    return ret;
}
