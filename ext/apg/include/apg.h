// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2019.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef APG_H
#define APG_H

// ---------------------------------------------------------------------------
// APG File Contents
//
// Offset   Size     Type      Description           Notes
// ------   ----     ----      --------------------- -------------------------
//      0      4   fourcc      Magic: APG!
//      4      4      u32      Version               (network order, always 1)
//      8      4      u32      Width                 (network order)
//     12      4      u32      Height                (network order)
//     16      2      u16      Depth                 (network order)
//     18      2      u16      YUV Coeff: Red        (network order, 65535x)
//     20      2      u16      YUV Coeff: Green      (network order, 65535x)
//     22      2      u16      YUV Coeff: Blue       (network order, 65535x)
//     24      4      u32      ICC Payload Size      (network order)
//     28                      -- end of header --
//     28      x    bytes      ICC Payload           (may be 0 bytes)
//   28+x   rest    bytes      AV1 OBU Payload
//
// ---------------------------------------------------------------------------

#define APG_HEADER_SIZE_V1 28 // see above

#include <stdint.h>

typedef int apgBool;
#define apgTrue 1
#define apgFalse 0

typedef enum apgResult
{
    APG_RESULT_OK = 0,
    APG_RESULT_UNKNOWN_ERROR,
    APG_RESULT_TRUNCATED,
    APG_RESULT_INVALID_HEADER,
    APG_RESULT_UNSUPPORTED_VERSION,
    APG_RESULT_CODEC_INIT_FAILURE,
    APG_RESULT_INVALID_AV1_PAYLOAD,
    APG_RESULT_DECODE_FAILURE,
    APG_RESULT_INCONSISTENT_SIZES,
    APG_RESULT_UNSUPPORTED_FORMAT
} apgResult;

typedef struct apgImage
{
    int width;
    int height;
    int depth;
    uint16_t * pixels; // RGBA

    // ICC profile chunk; memory owned by apgImage
    uint8_t * icc;
    uint32_t iccSize;

    // YUV coefficients used for RGB <-> YUV conversion, stored * 65535 into uint16_t
    uint16_t yuvKR;
    uint16_t yuvKG;
    uint16_t yuvKB;

    // Encoded chunk (valid after apgImageEncode() returns true); memory owned by apgImage
    uint8_t * encoded;
    uint32_t encodedSize;
} apgImage;

apgImage * apgImageDecode(uint8_t * encoded, uint32_t encodedSize, apgResult * result);
apgImage * apgImageCreate(int width, int height, int depth);
void apgImageDestroy(apgImage * image);
void apgImageSetICC(apgImage * image, uint8_t * icc, uint32_t iccSize);
apgResult apgImageEncode(apgImage * image, int quality); // quality is [0-100]; 0 and 100 are lossless, 1 is worst quality

// NOTE: By default, the YUV coefficients are for BT.709 (sRGB's gamut)
//       You should change these to the proper coefficients for the gamut you're actually using.
//       It will probably mostly work without doing this, but you'd be giving "incorrect"
//       (but reversible) YUV data to the AV1 encoder.
// Usage: takes [0-1] coefficients, converts internally to uint16
void apgImageSetYUVCoefficients(apgImage * image, float yuvKR, float yuvKG, float yuvKB);

// TODO: Implement so that endusers of this library don't need to do this math
// void apgImageSetYUVCoefficientsFromPrimaries(apgImage * image, float xr, float yr, float xg, float yg, float xb, float yb, float xw, float yw);

#endif // ifndef APG_H
