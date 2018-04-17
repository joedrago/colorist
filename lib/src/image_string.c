// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Notes: (ignore)
// * dimensions first optionally: WxH
// * comma separated color list, evenly distributed throughout entire image
// * color..color makes a gradient (lerp)
// * color.N.color makes a gradient, repeating each lerped color N times
// * xN is "repeat last entry N times"
// * alpha always optional, assumed to be opaque
// * #ffffffff - 8bit
// * parens for direct numbers (), optionally prefixed with rgb
// * float() or f() for floating point values 0-1
// * cw and ccw rotate the image 90 degrees

// Examples:
//
// A single white pixel
// #ffffff
//
// A red 256x256 image
// 256x256,#ff0000
//
// A red 256x256 image with half transparency
// 256x256,#ff000080
//
// A half red, half black 256x256 image
// 256x256,#ff0000,#000000
//
// A black to red gradient (top to bottom)
// 256x256,#000000..#ff0000
//
// A black to red gradient (left to right)
// 256x256,#000000..#ff0000,ccw
//
// A black to red gradient (left to right), less obvious method
// 256x256,#000000..#ff0000,x256
//
// color bars
// 100x600,#ff0000,#00ff00,#0000ff,#ffff00,#ff00ff,#00ffff

typedef struct clColor
{
    union {
        struct { int r, g, b, a; };
        struct { float fr, fg, fb, fa; };
    };
    int depth; // 8 and 16 are unorm, 32 is float
} clColor;

struct clToken;
typedef struct clToken
{
    // WxH
    int width;
    int height;

    // Color: #ffffff, #ffffffff, (0,0,0), rgb(0,0,0), f(0,0,0), float(0,0,0)
    // Range: Color..Color
    // Range with N stutter: Color.N.Color
    clColor start;
    clColor end;
    int stutter;
    int count; // total number of lerped colors (stutter included)

    // xN
    int repeat;

    // cw, ccw
    // -1 is ccw, 1 is cw
    int rotate;

    struct clToken * next;
} clToken;

static unsigned int hexdigit(char hex)
{
    return (hex <= '9') ? hex - '0' : toupper(hex) - 'A' + 10;
}

static unsigned int hexChannel(const char * hex)
{
    return (hexdigit(*hex) << 4) | hexdigit(*(hex + 1));
}

static const char * parseHashColor(struct clContext * C, const char * s, clColor * parsedColor)
{
    const char * end;
    int len;

    if (*s != '#') {
        clContextLogError(C, "hash color does not begin with #");
        return NULL;
    }

    end = s + 1;
    while (*end && (*end != ',') && (*end != '.')) {
        char upper = toupper(*end);
        if (!(
                ((upper >= '0') && (upper <= '9')) ||
                ((upper >= 'A') && (upper <= 'F')))
            )
        {
            clContextLogError(C, "unexpected character in hash color: '%c'", *end);
            return NULL;
        }
        ++end;
    }
    len = end - s;
    parsedColor->depth = 8;
    if (len == 7) {
        // #ff00ff
        parsedColor->r = hexChannel(s + 1);
        parsedColor->g = hexChannel(s + 3);
        parsedColor->b = hexChannel(s + 5);
        parsedColor->a = 255;
        return s + 7;
    } else if (len == 9) {
        // #ff00ff00
        parsedColor->r = hexChannel(s + 1);
        parsedColor->g = hexChannel(s + 3);
        parsedColor->b = hexChannel(s + 5);
        parsedColor->a = hexChannel(s + 7);
        return s + 9;
    }
    clContextLogError(C, "unexpected hash color length [%d] here: %s", len, s);
    return NULL;
}

static const char * parseParenColor(struct clContext * C, const char * s, int depth, clColor * parsedColor, cmsHTRANSFORM fromXYZ, clBool isXYY)
{
    char * buffer;
    const char * end;
    int len;
    char * token;
    int index = 0;
    int ints[4];
    float floats[4];

    if (*s != '(') {
        clContextLogError(C, "paren color does not begin with open paren");
        return NULL;
    }

    end = s + 1;
    while (*end && (*end != ')')) {
        ++end;
    }
    if (*end != ')') {
        clContextLogError(C, "Couldn't find end paren associated with open paren here: %s", s);
        return NULL;
    }

    len = end - s - 1;
    if (len < 1) {
        clContextLogError(C, "empty parenthesized color here: %s", s);
        return NULL;
    }

    buffer = clAllocate(len + 1);
    memcpy(buffer, s + 1, len);
    buffer[len] = 0;
    index = 0;
    for (token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ",")) {
        if (depth == 32) {
            floats[index] = (float)strtod(token, NULL);
        } else {
            ints[index] = atoi(token);
        }
        ++index;
        if (index == 4) {
            break;
        }
    }

    memset(parsedColor, 0, sizeof(clColor));
    parsedColor->depth = depth;
    if (depth == 32) {
        parsedColor->fa = 1.0f;
        if (index > 0)
            parsedColor->fr = floats[0];
        if (index > 1)
            parsedColor->fg = floats[1];
        if (index > 2)
            parsedColor->fb = floats[2];
        if (index > 3)
            parsedColor->fa = floats[3];

        if (isXYY) {
            cmsCIExyY src;
            cmsCIEXYZ dst;
            src.x = parsedColor->fr;
            src.y = parsedColor->fg;
            src.Y = parsedColor->fb;
            cmsxyY2XYZ(&dst, &src);
            parsedColor->fr = (float)dst.X;
            parsedColor->fg = (float)dst.Y;
            parsedColor->fb = (float)dst.Z;
        }
        if (fromXYZ) {
            float src[3];
            float dst[3];
            src[0] = parsedColor->fr;
            src[1] = parsedColor->fg;
            src[2] = parsedColor->fb;
            cmsDoTransform(fromXYZ, src, dst, 1);
            parsedColor->fr = dst[0];
            parsedColor->fg = dst[1];
            parsedColor->fb = dst[2];
        }
    } else {
        parsedColor->a = (depth == 16) ? 65535 : 255;
        if (index > 0)
            parsedColor->r = ints[0];
        if (index > 1)
            parsedColor->g = ints[1];
        if (index > 2)
            parsedColor->b = ints[2];
        if (index > 3)
            parsedColor->a = ints[3];
    }

    clFree(buffer);
    return end + 1;
}

static void clampColor(struct clContext * C, clColor * parsedColor)
{
    switch (parsedColor->depth) {
        case 8:
            parsedColor->r = CL_CLAMP(parsedColor->r, 0, 255);
            parsedColor->g = CL_CLAMP(parsedColor->g, 0, 255);
            parsedColor->b = CL_CLAMP(parsedColor->b, 0, 255);
            parsedColor->a = CL_CLAMP(parsedColor->a, 0, 255);
            break;
        case 16:
            parsedColor->r = CL_CLAMP(parsedColor->r, 0, 65535);
            parsedColor->g = CL_CLAMP(parsedColor->g, 0, 65535);
            parsedColor->b = CL_CLAMP(parsedColor->b, 0, 65535);
            parsedColor->a = CL_CLAMP(parsedColor->a, 0, 65535);
            break;
        case 32:
            parsedColor->fr = CL_CLAMP(parsedColor->fr, 0.0f, 1.0f);
            parsedColor->fg = CL_CLAMP(parsedColor->fg, 0.0f, 1.0f);
            parsedColor->fb = CL_CLAMP(parsedColor->fb, 0.0f, 1.0f);
            parsedColor->fa = CL_CLAMP(parsedColor->fa, 0.0f, 1.0f);
            break;
        default:
            COLORIST_FAILURE1("clampColor: unexpected depth %d", parsedColor->depth);
            break;
    }
}

static const char * parseColor(struct clContext * C, const char * s, clColor * parsedColor, cmsHTRANSFORM fromXYZ)
{
    if (*s == '#') {
        s = parseHashColor(C, s, parsedColor);
        if (s != NULL) {
            clampColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "(", 1)) {
        s = parseParenColor(C, s, 8, parsedColor, NULL, clFalse);
        if (s != NULL) {
            clampColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "rgb(", 4)) {
        s = parseParenColor(C, s + 3, 8, parsedColor, NULL, clFalse);
        if (s != NULL) {
            clampColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "rgba(", 5)) {
        s = parseParenColor(C, s + 4, 8, parsedColor, NULL, clFalse);
        if (s != NULL) {
            clampColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "rgb16(", 6)) {
        s = parseParenColor(C, s + 5, 16, parsedColor, NULL, clFalse);
        if (s != NULL) {
            clampColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "rgba16(", 7)) {
        s = parseParenColor(C, s + 6, 16, parsedColor, NULL, clFalse);
        if (s != NULL) {
            clampColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "f(", 2)) {
        s = parseParenColor(C, s + 1, 32, parsedColor, NULL, clFalse);
        if (s != NULL) {
            clampColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "float(", 6)) {
        s = parseParenColor(C, s + 5, 32, parsedColor, NULL, clFalse);
        if (s != NULL) {
            clampColor(C, parsedColor);
        }
        return s;
    } else if ((!strncmp(s, "xyz(", 4))) {
        s = parseParenColor(C, s + 3, 32, parsedColor, fromXYZ, clFalse);
        if (s != NULL) {
            clampColor(C, parsedColor);
        }
        return s;
    } else if ((!strncmp(s, "xyy(", 4))) {
        s = parseParenColor(C, s + 3, 32, parsedColor, fromXYZ, clTrue);
        if (s != NULL) {
            clampColor(C, parsedColor);
        }
        return s;
    }
    clContextLogError(C, "unknown color format here: %s", s);
    return NULL;
}

static const char * parseRange(struct clContext * C, const char * s, clToken * token)
{
    if (*s != '.') {
        clContextLogError(C, "range not begin with .");
        return NULL;
    }
    if (s[1] == '.') {
        token->stutter = 0;
        return s + 2;
    } else {
        char buffer[32];
        int len;
        const char * end = ++s;
        while (isdigit(*end)) {
            ++end;
        }
        len = end - s;
        if (len > 31) {
            clContextLogError(C, "range stutter too long [%d] here: %s", len, s);
            return NULL;
        }
        memcpy(buffer, s, len);
        buffer[len] = 0;
        token->stutter = atoi(buffer);
        return end + 1;
    }
    // Shouldn't be able to get here
    COLORIST_ASSERT(0);
    return NULL;
}

static clBool finishRange(struct clContext * C, clToken * token)
{
    int diff, maxDiff;

    if ((token->start.depth == 32) || (token->end.depth == 32)) {
        clContextLogError(C, "float colors disallowed in ranges");
        return clFalse;
    }
    if (token->start.depth != token->end.depth) {
        clContextLogError(C, "ranges must match in depth (8 or 16)");
        return clFalse;
    }

    maxDiff = abs(token->start.r - token->end.r);
    diff = abs(token->start.g - token->end.g);
    maxDiff = (diff > maxDiff) ? diff : maxDiff;
    diff = abs(token->start.b - token->end.b);
    maxDiff = (diff > maxDiff) ? diff : maxDiff;
    diff = abs(token->start.a - token->end.a);
    maxDiff = (diff > maxDiff) ? diff : maxDiff;
    token->count = 1 + maxDiff; // inclusive
    if (token->stutter > 0) {
        token->count *= token->stutter;
    }
    return clTrue;
}

static const char * parseDimensions(struct clContext * C, const char * s, clToken * token)
{
    char buffer[32];
    int len;
    const char * end;

    // Width
    end = s;
    while (isdigit(*end)) {
        ++end;
    }
    len = end - s;
    if (len == 0) {
        clContextLogError(C, "Invalid width here: %s", s);
        return NULL;
    }
    if (len > 31) {
        clContextLogError(C, "Width number too long [%d] here: %s", len, s);
        return NULL;
    }
    memcpy(buffer, s, len);
    buffer[len] = 0;
    token->width = atoi(buffer);

    s = end;
    if (*s != 'x') {
        clContextLogError(C, "Dimensions expected an 'x' here: %s", s);
    }
    ++s;

    // Height
    end = s;
    while (isdigit(*end)) {
        ++end;
    }
    len = end - s;
    if (len == 0) {
        clContextLogError(C, "Invalid height here: %s", s);
        return NULL;
    }
    if (len > 31) {
        clContextLogError(C, "Height number too long [%d] here: %s", len, s);
        return NULL;
    }
    memcpy(buffer, s, len);
    buffer[len] = 0;
    token->height = atoi(buffer);
    return end;
}

static const char * parseRepeat(struct clContext * C, const char * s, clToken * token)
{
    char buffer[32];
    int len;
    const char * end;
    if (*s != 'x') {
        clContextLogError(C, "repeat not begin with x");
        return NULL;
    }
    end = ++s;
    while (isdigit(*end)) {
        ++end;
    }
    len = end - s;
    if (len > 31) {
        clContextLogError(C, "repeat too long [%d] here: %s", len, s);
        return NULL;
    }
    memcpy(buffer, s, len);
    buffer[len] = 0;
    token->repeat = atoi(buffer);
    return end;
}
static const char * parseNext(struct clContext * C, const char * s, clToken * token, cmsHTRANSFORM fromXYZ)
{
    memset(token, 0, sizeof(clToken));
    if (!strncmp(s, "ccw", 3)) {
        token->rotate = -1;
        s += 3;
    } else if (!strncmp(s, "cw", 2)) {
        token->rotate = 1;
        s += 2;

    } else if ((*s == '#') ||
               (*s == '(') ||
               (!strncmp(s, "rgb(", 4)) ||
               (!strncmp(s, "rgba(", 5)) ||
               (!strncmp(s, "rgb16(", 6)) ||
               (!strncmp(s, "rgba16(", 7)) ||
               (!strncmp(s, "f(", 2)) ||
               (!strncmp(s, "float(", 6)) ||
               (!strncmp(s, "xyz(", 4)) ||
               (!strncmp(s, "xyy(", 4)))
    {
        s = parseColor(C, s, &token->start, fromXYZ);
        if (s == NULL) {
            return NULL;
        }
        if (*s == '.') {
            s = parseRange(C, s, token);
            if (s == NULL) {
                return NULL;
            }
            s = parseColor(C, s, &token->end, fromXYZ);
            if (s == NULL) {
                return NULL;
            }
            if (!finishRange(C, token)) {
                return NULL;
            }
        } else {
            // Just a single color
            token->count = 1;
        }
    } else if (*s == 'x') {
        s = parseRepeat(C, s, token);
        if (s == NULL) {
            return NULL;
        }
    } else if (isdigit(*s)) {
        s = parseDimensions(C, s, token);
        if (s == NULL) {
            return NULL;
        }
    } else {
        clContextLogError(C, "unexpected next character here: %s", s);
        return NULL;
    }
    return s;
}

static char * sanitizeString(char * s)
{
    char * dst = s;
    char * src = s;

    // Remove whitespace
    while (*src) {
        if ((*src != ' ') && (*src != '\t')) {
            *dst = tolower(*src);
            ++dst;
        }
        ++src;
    }
    *dst = 0;
    return s;
}

static clImage * interpretTokens(struct clContext * C, clToken * tokens, int depth, struct clProfile * profile);

clImage * clImageParseString(struct clContext * C, const char * s, int depth, struct clProfile * profile)
{
    char * sanitizedString;
    clImage * image = NULL;
    clToken * tokens = NULL;
    clToken * lastToken = NULL;
    clToken * token;

    cmsHPROFILE xyzProfile = cmsCreateXYZProfileTHR(C->lcms);
    cmsHTRANSFORM fromXYZ = cmsCreateTransformTHR(C->lcms, xyzProfile, TYPE_XYZ_FLT, profile->handle, TYPE_RGB_FLT, INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_NOOPTIMIZE);

    clContextLog(C, "parse", 0, "Parsing image string...");

    sanitizedString = sanitizeString(clContextStrdup(C, s));
    s = sanitizedString;

    for (;;) {
        token = clAllocateStruct(clToken);
        s = parseNext(C, s, token, fromXYZ);
        if (s == NULL) {
            clFree(token);
            goto parseCleanup;
        }

        if (token->repeat) {
            // Parsed a repeat token, just merge it with the last token
            if (lastToken == NULL) {
                clContextLogError(C, "attempting to repeat nothing");
                clFree(token);
                goto parseCleanup;
            } else {
                lastToken->repeat = token->repeat;
                clFree(token);
            }
        } else {
            if (lastToken == NULL) {
                tokens = token;
            } else {
                lastToken->next = token;
            }
            lastToken = token;
        }

        if (*s == 0) {
            // Successful parse
            break;
        }
        if (*s != ',') {
            clContextLogError(C, "unexpected next character here: %s", s);
            goto parseCleanup;
        }

        ++s; // advance past a single comma
    }

    clContextLog(C, "parse", 1, "Successfully parsed image string.");

    image = interpretTokens(C, tokens, depth, profile);

parseCleanup:
    cmsDeleteTransform(fromXYZ);
    cmsCloseProfile(xyzProfile);
    clFree(sanitizedString);
    if (tokens) {
        clToken * t = tokens;
        while (t->next != NULL) {
            clToken * freeMe = t;
            t = t->next;
            clFree(freeMe);
        }
    }
    return image;
}

static void getColorFromRange(struct clContext * C, clToken * t, int reqIndex, clColor * outColor)
{
    float p;
    int diff;
    if (t->repeat)
        reqIndex = reqIndex % t->repeat;
    p = (float)reqIndex / (t->count - 1);

    diff = t->end.r - t->start.r;
    outColor->r = t->start.r + (int)((float)diff * p);
    diff = t->end.g - t->start.g;
    outColor->g = t->start.g + (int)((float)diff * p);
    diff = t->end.b - t->start.b;
    outColor->b = t->start.b + (int)((float)diff * p);
    diff = t->end.a - t->start.a;
    outColor->a = t->start.a + (int)((float)diff * p);
    outColor->depth = t->start.depth;
}

static void getRawColor(struct clContext * C, clToken * tokens, int reqIndex, clColor * outColor)
{
    int index = 0;
    int colorStart = 0;
    int colorEnd = 0;
    clToken * t;
    if (reqIndex == 7) {
        reqIndex++;
        reqIndex--;
    }
    for (t = tokens; t != NULL; t = t->next) {
        colorStart = colorEnd;
        if (t->repeat) {
            colorEnd += t->count * t->repeat;
        } else {
            colorEnd += t->count;
        }
        if ((reqIndex >= colorStart) && (reqIndex < colorEnd)) {
            int internalIndex = reqIndex - colorStart;
            if (t->count == 1) {
                memcpy(outColor, &t->start, sizeof(clColor));
                return;
            }
            getColorFromRange(C, t, internalIndex, outColor);
            return;
        }
    }
    COLORIST_ASSERT(0); // what to do here?
}

static void getColor(struct clContext * C, clToken * tokens, int reqIndex, int depth, clColor * outColor)
{
    getRawColor(C, tokens, reqIndex, outColor);
    if (depth == 16) {
        switch (outColor->depth) {
            case 8:
                outColor->r = (uint16_t)((float)outColor->r / 255.0f * 65535.0f);
                outColor->g = (uint16_t)((float)outColor->g / 255.0f * 65535.0f);
                outColor->b = (uint16_t)((float)outColor->b / 255.0f * 65535.0f);
                outColor->a = (uint16_t)((float)outColor->a / 255.0f * 65535.0f);
                break;
            case 16:
                break;
            case 32:
                outColor->r = (uint16_t)clPixelMathRoundf(outColor->fr * 65535.0f);
                outColor->g = (uint16_t)clPixelMathRoundf(outColor->fg * 65535.0f);
                outColor->b = (uint16_t)clPixelMathRoundf(outColor->fb * 65535.0f);
                outColor->a = (uint16_t)clPixelMathRoundf(outColor->fa * 65535.0f);
                break;
            default:
                COLORIST_ASSERT(0);
        }
    } else {
        switch (outColor->depth) {
            case 8:
                break;
            case 16:
                outColor->r = outColor->r >> 8;
                outColor->g = outColor->g >> 8;
                outColor->b = outColor->b >> 8;
                outColor->a = outColor->a >> 8;
                break;
            case 32:
                outColor->r = (uint16_t)clPixelMathRoundf(outColor->fr * 255.0f);
                outColor->g = (uint16_t)clPixelMathRoundf(outColor->fg * 255.0f);
                outColor->b = (uint16_t)clPixelMathRoundf(outColor->fb * 255.0f);
                outColor->a = (uint16_t)clPixelMathRoundf(outColor->fa * 255.0f);
                break;
            default:
                COLORIST_ASSERT(0);
        }
    }
}

static clImage * interpretTokens(struct clContext * C, clToken * tokens, int depth, struct clProfile * profile)
{
    clImage * image = NULL;
    int colorCount;
    int imageWidth = 0;
    int imageHeight = 0;
    int pixelCount = 0;
    int pixelIndex;
    int rotate = 0;
    int every = 0;
    int colorIndex = 0;
    clToken * t;

    colorCount = 0;
    for (t = tokens; t != NULL; t = t->next) {
        if (t->repeat) {
            colorCount += t->count * t->repeat;
        } else {
            colorCount += t->count;
        }
        if (t->width) {
            imageWidth = t->width;
        }
        if (t->width) {
            imageHeight = t->height;
        }
        rotate += t->rotate;
    }
    while (rotate < 0) {
        rotate += 4;
    }
    rotate = rotate % 4;

    clContextLog(C, "parse", 1, "Image string describes %d color%s.", colorCount, (colorCount > 1) ? "s" : "");
    if (imageWidth && imageHeight) {
        clContextLog(C, "parse", 1, "Image string requests a resolution of %dx%d", imageWidth, imageHeight);
    } else {
        if (colorCount < 1) {
            clContextLogError(C, "Image string specifies no colors and no resolution, bailing out");
            return NULL;
        }
        imageWidth = colorCount;
        imageHeight = 1;
        clContextLog(C, "parse", 1, "Image string does not specify a resolution, choosing %dx%d", imageWidth, imageHeight);
    }
    pixelCount = imageWidth * imageHeight;

    image = clImageCreate(C, imageWidth, imageHeight, depth, profile);

    if (colorCount < pixelCount) {
        clContextLog(C, "parse", 1, "More pixels than colors. Spreading colors evenly.");
        every = pixelCount / colorCount;
    } else {
        clContextLog(C, "parse", 1, "One color per pixel until no pixels are left.");
        every = 1;
    }

    for (pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
        clColor color;
        if (pixelIndex && ((pixelIndex % every) == 0)) {
            ++colorIndex;
        }
        colorIndex = CL_CLAMP(colorIndex, 0, colorCount - 1);
        // clContextLog(C, "debug", 0, "pixel %d gets color %d", pixelIndex, colorIndex);
        getColor(C, tokens, colorIndex, depth, &color);
        if (depth == 16) {
            uint16_t * pixels = (uint16_t *)image->pixels;
            uint16_t * pixel = &pixels[4 * pixelIndex];
            pixel[0] = color.r;
            pixel[1] = color.g;
            pixel[2] = color.b;
            pixel[3] = color.a;
        } else {
            uint8_t * pixel = &image->pixels[4 * pixelIndex];
            pixel[0] = color.r;
            pixel[1] = color.g;
            pixel[2] = color.b;
            pixel[3] = color.a;
        }
    }

    if (rotate != 0) {
        clImage * rotated;
        clContextLog(C, "parse", 1, "Rotating image %d turn%s clockwise", rotate, (rotate > 1) ? "s" : "");
        rotated = clImageRotate(C, image, rotate);
        clImageDestroy(C, image);
        image = rotated;
        clContextLog(C, "parse", 1, "Final resolution after rotation: %dx%d", image->width, image->height);
    }
    return image;
}
