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
#include "colorist/transform.h"

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
    int r, g, b, a;
    float fr, fg, fb, fa;
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
    // Range with specified count: Color.N.Color
    clColor start;
    clColor end;
    int count; // total number of lerped colors

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
    size_t len;

    if (*s != '#') {
        clContextLogError(C, "hash color does not begin with #");
        return NULL;
    }

    end = s + 1;
    while (*end && (*end != ',') && (*end != '.')) {
        char upper = (char)toupper(*end);
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

static const char * parseParenColor(struct clContext * C, const char * s, int depth, clColor * parsedColor, clTransform * fromXYZ, int luminance, clBool isXYY)
{
    char * buffer;
    const char * end;
    size_t len;
    char * token;
    int index = 0;
    int ints[4] = { 0, 0, 0, 0 };
    float floats[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

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
            src[0] = parsedColor->fr * (float)luminance;
            src[1] = parsedColor->fg * (float)luminance;
            src[2] = parsedColor->fb * (float)luminance;
            clTransformRun(C, fromXYZ, 1, src, dst, 1);
            parsedColor->fr = dst[0];
            parsedColor->fg = dst[1];
            parsedColor->fb = dst[2];
        }
    } else {
        parsedColor->a = (1 << depth) - 1;
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

static void finishColor(struct clContext * C, clColor * parsedColor)
{
    COLORIST_UNUSED(C);

    if (parsedColor->depth <= 16) {
        int maxChannel = (1 << parsedColor->depth) - 1;
        parsedColor->r = CL_CLAMP(parsedColor->r, 0, maxChannel);
        parsedColor->g = CL_CLAMP(parsedColor->g, 0, maxChannel);
        parsedColor->b = CL_CLAMP(parsedColor->b, 0, maxChannel);
        parsedColor->a = CL_CLAMP(parsedColor->a, 0, maxChannel);
        parsedColor->fr = (float)parsedColor->r / (float)maxChannel;
        parsedColor->fg = (float)parsedColor->g / (float)maxChannel;
        parsedColor->fb = (float)parsedColor->b / (float)maxChannel;
        parsedColor->fa = (float)parsedColor->a / (float)maxChannel;
    } else if (parsedColor->depth != 32) {
        COLORIST_FAILURE1("finishColor: unexpected depth %d", parsedColor->depth);
    }
    parsedColor->fr = CL_CLAMP(parsedColor->fr, 0.0f, 1.0f);
    parsedColor->fg = CL_CLAMP(parsedColor->fg, 0.0f, 1.0f);
    parsedColor->fb = CL_CLAMP(parsedColor->fb, 0.0f, 1.0f);
    parsedColor->fa = CL_CLAMP(parsedColor->fa, 0.0f, 1.0f);
}

static const char * parseColor(struct clContext * C, const char * s, clColor * parsedColor, clTransform * fromXYZ, int luminance)
{
    if (*s == '#') {
        s = parseHashColor(C, s, parsedColor);
        if (s != NULL) {
            finishColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "(", 1)) {
        s = parseParenColor(C, s, 8, parsedColor, NULL, luminance, clFalse);
        if (s != NULL) {
            finishColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "rgb(", 4)) {
        s = parseParenColor(C, s + 3, 8, parsedColor, NULL, luminance, clFalse);
        if (s != NULL) {
            finishColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "rgba(", 5)) {
        s = parseParenColor(C, s + 4, 8, parsedColor, NULL, luminance, clFalse);
        if (s != NULL) {
            finishColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "rgb16(", 6)) {
        s = parseParenColor(C, s + 5, 16, parsedColor, NULL, luminance, clFalse);
        if (s != NULL) {
            finishColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "rgba16(", 7)) {
        s = parseParenColor(C, s + 6, 16, parsedColor, NULL, luminance, clFalse);
        if (s != NULL) {
            finishColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "f(", 2)) {
        s = parseParenColor(C, s + 1, 32, parsedColor, NULL, luminance, clFalse);
        if (s != NULL) {
            finishColor(C, parsedColor);
        }
        return s;
    } else if (!strncmp(s, "float(", 6)) {
        s = parseParenColor(C, s + 5, 32, parsedColor, NULL, luminance, clFalse);
        if (s != NULL) {
            finishColor(C, parsedColor);
        }
        return s;
    } else if ((!strncmp(s, "xyz(", 4))) {
        s = parseParenColor(C, s + 3, 32, parsedColor, fromXYZ, luminance, clFalse);
        if (s != NULL) {
            finishColor(C, parsedColor);
        }
        return s;
    } else if ((!strncmp(s, "xyy(", 4))) {
        s = parseParenColor(C, s + 3, 32, parsedColor, fromXYZ, luminance, clTrue);
        if (s != NULL) {
            finishColor(C, parsedColor);
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
        token->count = 0;
        return s + 2;
    } else {
        char buffer[32];
        size_t len;
        const char * end = ++s;
        while (isdigit(*end)) {
            ++end;
        }
        len = end - s;
        if (len > 31) {
            clContextLogError(C, "range size string too long [%d] here: %s", len, s);
            return NULL;
        }
        memcpy(buffer, s, len);
        buffer[len] = 0;
        token->count = atoi(buffer);
        return end + 1;
    }
}

static clBool finishRange(struct clContext * C, clToken * token)
{
    int diff, maxDiff;

    // If the count isn't specified, use the full range
    if (token->count == 0) {
        if ((token->start.depth == 32) || (token->end.depth == 32)) {
            clContextLogError(C, "range size must be specified when using float colors");
            return clFalse;
        }
        if (token->start.depth != token->end.depth) {
            clContextLogError(C, "range size must be specified when using mismatched depths for start and end");
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
    }
    return clTrue;
}

static const char * parseDimensions(struct clContext * C, const char * s, clToken * token)
{
    char buffer[32];
    size_t len;
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
    size_t len;
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
static const char * parseNext(struct clContext * C, const char * s, clToken * token, clTransform * fromXYZ, int luminance)
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
        s = parseColor(C, s, &token->start, fromXYZ, luminance);
        if (s == NULL) {
            return NULL;
        }
        if (*s == '.') {
            s = parseRange(C, s, token);
            if (s == NULL) {
                return NULL;
            }
            s = parseColor(C, s, &token->end, fromXYZ, luminance);
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
        if ((*src != ' ') && (*src != '\t') && (*src != '\n') && (*src != '\r')) {
            *dst = (char)tolower(*src);
            ++dst;
        }
        ++src;
    }
    *dst = 0;
    return s;
}

static clImage * interpretTokens(struct clContext * C, clToken * tokens, int depth, struct clProfile * profile, int defaultW, int defaultH);

static clImage * clImageParseStripe(struct clContext * C, const char * s, int depth, struct clProfile * profile, int luminance, struct clTransform * fromXYZ, int defaultW, int defaultH)
{
    char * sanitizedString = NULL;
    clImage * image = NULL;
    clToken * tokens = NULL;
    clToken * lastToken = NULL;
    clToken * token;

    if (s[0] == '@') {
        // It's a response file. Read it all in.
        char * tempString;
        int size;
        FILE * f = fopen(s + 1, "rb");
        if (!f) {
            clContextLogError(C, "generate can't open response file: %s", s + 1);
            goto parseCleanup;
        }
        fseek(f, 0, SEEK_END);
        size = (int)ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size < 1) {
            fclose(f);
            clContextLogError(C, "generate can't use a 0 byte response file: %s", s + 1);
            goto parseCleanup;
        }
        tempString = clAllocate(size + 1);
        if (fread(tempString, size, 1, f) != 1) {
            clContextLogError(C, "generate failed to read all %d bytes from response file: %s", size, s + 1);
            clFree(tempString);
            fclose(f);
            goto parseCleanup;
        }
        fclose(f);
        sanitizedString = sanitizeString(tempString); // pass ownership to sanitizedString
    } else {
        sanitizedString = sanitizeString(clContextStrdup(C, s));
    }

    s = sanitizedString;
    for (;;) {
        token = clAllocateStruct(clToken);
        s = parseNext(C, s, token, fromXYZ, luminance);
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

    image = interpretTokens(C, tokens, depth, profile, defaultW, defaultH);

parseCleanup:
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

typedef struct Stripe
{
    char * s;
    clImage * image;
    struct Stripe * next;
} Stripe;

clImage * clImageParseString(struct clContext * C, const char * s, int depth, struct clProfile * profile)
{
    Stripe * stripes = NULL;
    Stripe * lastStripe = NULL;
    Stripe * stripe;
    clImage * image = NULL;
    int stripeCount = 0;
    int stripeIndex = 0;
    int maxStripeWidth = 0;
    int totalStripeHeight = 0;
    int prevW = 0;
    int prevH = 0;
    char * buffer = clContextStrdup(C, s);
    const char * stripeDelims = "|/";
    char * stripeString;
    uint8_t * pixelPos;
    int depthBytes = clDepthToBytes(C, depth);
    clTransform * fromXYZ = clTransformCreate(C, NULL, CL_XF_XYZ, 32, profile, CL_XF_RGB, 32, CL_TONEMAP_OFF);
    int luminance = 0;

    clContextLog(C, "parse", 0, "Parsing image string (%s)...", clTransformCMMName(C, fromXYZ));

    if (profile) {
        clProfileQuery(C, profile, NULL, NULL, &luminance);
    } else {
        luminance = COLORIST_DEFAULT_LUMINANCE;
    }

    for (stripeString = strtok(buffer, stripeDelims); stripeString != NULL; stripeString = strtok(NULL, stripeDelims)) {
        stripe = clAllocateStruct(Stripe);
        stripe->s = clContextStrdup(C, stripeString);
        if (lastStripe == NULL) {
            stripes = lastStripe = stripe;
        } else {
            lastStripe->next = stripe;
            lastStripe = stripe;
        }
        ++stripeCount;
    }

    clContextLog(C, "parse", 0, "Found %d image stripe%s.", stripeCount, (stripeCount == 1) ? "" : "s");

    if (!stripes) {
        clContextLogError(C, "no valid image stripes found");
        goto parseCleanup;
    }

    for (stripe = stripes; stripe != NULL; stripe = stripe->next) {
        clContextLog(C, "parse", 0, "Parsing stripe index: %d", stripeIndex);
        stripe->image = clImageParseStripe(C, stripe->s, depth, profile, luminance, fromXYZ, prevW, prevH);
        if (!stripe->image) {
            goto parseCleanup;
        }
        if (maxStripeWidth < stripe->image->width) {
            maxStripeWidth = stripe->image->width;
        }
        totalStripeHeight += stripe->image->height;
        prevW = stripe->image->width;
        prevH = stripe->image->height;
        ++stripeIndex;
    }

    if (stripeCount > 1) {
        clContextLog(C, "parse", 0, "Compositing final image (stacking vertically): %dx%d", maxStripeWidth, totalStripeHeight);
        image = clImageCreate(C, maxStripeWidth, totalStripeHeight, depth, profile);
        pixelPos = image->pixels;
        for (stripe = stripes; stripe != NULL; stripe = stripe->next) {
            int y;
            for (y = 0; y < stripe->image->height; ++y) {
                memcpy(pixelPos, &stripe->image->pixels[4 * depthBytes * y * stripe->image->width], 4 * depthBytes * stripe->image->width);
                pixelPos += 4 * depthBytes * image->width;
            }
        }
    } else {
        image = stripes->image;
        stripes->image = NULL;
    }
    clContextLog(C, "parse", 1, "Successfully parsed image string.");

parseCleanup:
    while (stripes != NULL) {
        Stripe * deleteme = stripes;
        stripes = stripes->next;
        clFree(deleteme->s);
        if (deleteme->image) {
            clImageDestroy(C, deleteme->image);
        }
        clFree(deleteme);
    }
    clFree(buffer);
    clTransformDestroy(C, fromXYZ);
    return image;
}

static void getColorFromRange(struct clContext * C, clToken * t, int reqIndex, clColor * outColor)
{
    COLORIST_UNUSED(C);

    float p;
    float diff;

    reqIndex = reqIndex % t->count;
    p = (float)reqIndex / (t->count - 1);

    diff = t->end.fr - t->start.fr;
    outColor->fr = t->start.fr + (diff * p);
    diff = t->end.fg - t->start.fg;
    outColor->fg = t->start.fg + (diff * p);
    diff = t->end.fb - t->start.fb;
    outColor->fb = t->start.fb + (diff * p);
    diff = t->end.fa - t->start.fa;
    outColor->fa = t->start.fa + (diff * p);

    outColor->depth = t->start.depth;
    if (outColor->depth < t->end.depth) {
        outColor->depth = t->end.depth;
    }
}

static void getRawColor(struct clContext * C, clToken * tokens, int reqIndex, clColor * outColor)
{
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
    clColor tmpColor;
    int maxChannel;
    float maxChannelf;
    getRawColor(C, tokens, reqIndex, &tmpColor);

    // convert from 32 to destination depth
    maxChannel = ((1 << depth) - 1);
    maxChannelf = (float)maxChannel;
    outColor->r = (uint16_t)clPixelMathRoundf(tmpColor.fr * maxChannelf);
    outColor->r = CL_CLAMP(outColor->r, 0, maxChannel);
    outColor->g = (uint16_t)clPixelMathRoundf(tmpColor.fg * maxChannelf);
    outColor->g = CL_CLAMP(outColor->g, 0, maxChannel);
    outColor->b = (uint16_t)clPixelMathRoundf(tmpColor.fb * maxChannelf);
    outColor->b = CL_CLAMP(outColor->b, 0, maxChannel);
    outColor->a = (uint16_t)clPixelMathRoundf(tmpColor.fa * maxChannelf);
    outColor->a = CL_CLAMP(outColor->a, 0, maxChannel);
    outColor->depth = depth;
}

static clImage * interpretTokens(struct clContext * C, clToken * tokens, int depth, struct clProfile * profile, int defaultW, int defaultH)
{
    clImage * image = NULL;
    int colorCount;
    int imageWidth = defaultW;
    int imageHeight = defaultH;
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

    clContextLog(C, "parse", 1, "Image stripe describes %d color%s.", colorCount, (colorCount != 1) ? "s" : "");
    if (colorCount < 1) {
        clContextLogError(C, "Image stripe specifies no colors, bailing out");
        return NULL;
    }
    if (imageWidth && imageHeight) {
        clContextLog(C, "parse", 1, "Image stripe requests a resolution of %dx%d", imageWidth, imageHeight);
    } else {
        imageWidth = colorCount;
        imageHeight = 1;
        clContextLog(C, "parse", 1, "Image stripe does not specify a resolution, choosing %dx%d", imageWidth, imageHeight);
    }
    pixelCount = imageWidth * imageHeight;

    image = clImageCreate(C, imageWidth, imageHeight, depth, profile);

    if (colorCount < imageWidth) {
        clContextLog(C, "parse", 1, "More width than colors. Spreading colors evenly.");
        every = imageHeight * (imageWidth / colorCount);
    } else {
        clContextLog(C, "parse", 1, "One color per row until no rows are left.");
        every = imageHeight;
    }

    for (pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
        clColor color;
        int x = pixelIndex / imageHeight;
        int y = pixelIndex % imageHeight;
        int verticalPixelIndex = x + (y * imageWidth); // this fills columns down, then to the right
        if (pixelIndex && ((pixelIndex % every) == 0)) {
            ++colorIndex;
        }
        colorIndex = CL_CLAMP(colorIndex, 0, colorCount - 1);
        getColor(C, tokens, colorIndex, depth, &color);
        if (depth > 8) {
            uint16_t * pixels = (uint16_t *)image->pixels;
            uint16_t * pixel = &pixels[4 * verticalPixelIndex];
            pixel[0] = (uint16_t)(color.r);
            pixel[1] = (uint16_t)(color.g);
            pixel[2] = (uint16_t)(color.b);
            pixel[3] = (uint16_t)(color.a);
        } else {
            uint8_t * pixel = &image->pixels[4 * verticalPixelIndex];
            pixel[0] = (uint8_t)(color.r);
            pixel[1] = (uint8_t)(color.g);
            pixel[2] = (uint8_t)(color.b);
            pixel[3] = (uint8_t)(color.a);
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
