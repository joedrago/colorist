#include "apg.h"

#include <string.h>

int main(int argc, char * argv[])
{
    int width = 256;
    int height = 256;
    int depth = 8;
    int pixelCount = width * height;

    // Encode an orange, 8-bit, full opacity image
    apgImage * image = apgImageCreate(width, height, depth);
    for (int i = 0; i < pixelCount; ++i) {
        uint16_t * pixel = &image->pixels[4 * i];
        pixel[0] = 255; // R
        pixel[1] = 128; // G
        pixel[2] = 0;   // B
        pixel[3] = 255; // A
    }

    uint8_t * fakeICC = "abcdefg";
    uint32_t fakeICCSize = (uint32_t)strlen(fakeICC);
    apgImageSetICC(image, fakeICC, fakeICCSize);

    apgResult res = apgImageEncode(image, 50);
    if ((res == APG_RESULT_OK) && image->encoded && image->encodedSize) {
        // Decode it
        apgResult decodeResult = APG_RESULT_UNKNOWN_ERROR;
        apgImage * decodedImage = apgImageDecode(image->encoded, image->encodedSize, &decodeResult);
        if (decodedImage) {
            apgImageDestroy(decodedImage);
        }
    }

    apgImageDestroy(image);
    return 0;
}
