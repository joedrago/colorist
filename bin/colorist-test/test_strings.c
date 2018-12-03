// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "main.h"

// ------------------------------------------------------------------------------------------------
// Image string tests ("generate")
// ------------------------------------------------------------------------------------------------

static void test_basic_hexcodes(void)
{
    clContext * C = clContextCreate(&silentSystem);
    clImage * image;

    image = clImageParseString(C, "#000000", 8, NULL);
    TEST_ASSERT_NOT_NULL(image);
    TEST_ASSERT_EQUAL_INT(image->pixels[0], 0);
    TEST_ASSERT_EQUAL_INT(image->pixels[1], 0);
    TEST_ASSERT_EQUAL_INT(image->pixels[2], 0);
    clImageDestroy(C, image);

    image = clImageParseString(C, "#ffffff", 8, NULL);
    TEST_ASSERT_NOT_NULL(image);
    TEST_ASSERT_EQUAL_INT(image->pixels[0], 255);
    TEST_ASSERT_EQUAL_INT(image->pixels[1], 255);
    TEST_ASSERT_EQUAL_INT(image->pixels[2], 255);
    clImageDestroy(C, image);

    image = clImageParseString(C, "#ff0000", 8, NULL);
    TEST_ASSERT_NOT_NULL(image);
    TEST_ASSERT_EQUAL_INT(image->pixels[0], 255);
    TEST_ASSERT_EQUAL_INT(image->pixels[1], 0);
    TEST_ASSERT_EQUAL_INT(image->pixels[2], 0);
    clImageDestroy(C, image);

    image = clImageParseString(C, "#010203", 8, NULL);
    TEST_ASSERT_NOT_NULL(image);
    TEST_ASSERT_EQUAL_INT(image->pixels[0], 1);
    TEST_ASSERT_EQUAL_INT(image->pixels[1], 2);
    TEST_ASSERT_EQUAL_INT(image->pixels[2], 3);
    TEST_ASSERT_EQUAL_INT(image->pixels[3], 255);
    clImageDestroy(C, image);

    image = clImageParseString(C, "#01020304", 8, NULL);
    TEST_ASSERT_NOT_NULL(image);
    TEST_ASSERT_EQUAL_INT(image->pixels[0], 1);
    TEST_ASSERT_EQUAL_INT(image->pixels[1], 2);
    TEST_ASSERT_EQUAL_INT(image->pixels[2], 3);
    TEST_ASSERT_EQUAL_INT(image->pixels[3], 4);
    clImageDestroy(C, image);

    clContextDestroy(C);
}

static void test_basic_parens_8bit(void)
{
    clContext * C = clContextCreate(&silentSystem);
    clImage * image;

    image = clImageParseString(C, "(0,0,0)", 8, NULL);
    TEST_ASSERT_NOT_NULL(image);
    TEST_ASSERT_EQUAL_INT(0, image->pixels[0]);
    TEST_ASSERT_EQUAL_INT(0, image->pixels[1]);
    TEST_ASSERT_EQUAL_INT(0, image->pixels[2]);
    clImageDestroy(C, image);

    image = clImageParseString(C, "(255,255,255)", 8, NULL);
    TEST_ASSERT_NOT_NULL(image);
    TEST_ASSERT_EQUAL_INT(255, image->pixels[0]);
    TEST_ASSERT_EQUAL_INT(255, image->pixels[1]);
    TEST_ASSERT_EQUAL_INT(255, image->pixels[2]);
    clImageDestroy(C, image);

    image = clImageParseString(C, "(255,0,0)", 8, NULL);
    TEST_ASSERT_NOT_NULL(image);
    TEST_ASSERT_EQUAL_INT(255, image->pixels[0]);
    TEST_ASSERT_EQUAL_INT(0, image->pixels[1]);
    TEST_ASSERT_EQUAL_INT(0, image->pixels[2]);
    clImageDestroy(C, image);

    image = clImageParseString(C, "(1,2,3)", 8, NULL);
    TEST_ASSERT_NOT_NULL(image);
    TEST_ASSERT_EQUAL_INT(1, image->pixels[0]);
    TEST_ASSERT_EQUAL_INT(2, image->pixels[1]);
    TEST_ASSERT_EQUAL_INT(3, image->pixels[2]);
    TEST_ASSERT_EQUAL_INT(255, image->pixels[3]);
    clImageDestroy(C, image);

    image = clImageParseString(C, "(1,2,3,4)", 8, NULL);
    TEST_ASSERT_NOT_NULL(image);
    TEST_ASSERT_EQUAL_INT(1, image->pixels[0]);
    TEST_ASSERT_EQUAL_INT(2, image->pixels[1]);
    TEST_ASSERT_EQUAL_INT(3, image->pixels[2]);
    TEST_ASSERT_EQUAL_INT(4, image->pixels[3]);
    clImageDestroy(C, image);

    // This is a sneaky one
    image = clImageParseString(C, "rgba16(65535,0,0)", 8, NULL);
    TEST_ASSERT_NOT_NULL(image);
    TEST_ASSERT_EQUAL_INT(255, image->pixels[0]);
    TEST_ASSERT_EQUAL_INT(0, image->pixels[1]);
    TEST_ASSERT_EQUAL_INT(0, image->pixels[2]);
    clImageDestroy(C, image);

    clContextDestroy(C);
}

static void test_basic_parens_16bit(void)
{
    clContext * C = clContextCreate(&silentSystem);
    clImage * image;
    uint16_t * pixels;

    image = clImageParseString(C, "(0,0,0)", 16, NULL);
    TEST_ASSERT_NOT_NULL(image);
    pixels = (uint16_t *)image->pixels;
    TEST_ASSERT_EQUAL_INT(0, pixels[0]);
    TEST_ASSERT_EQUAL_INT(0, pixels[1]);
    TEST_ASSERT_EQUAL_INT(0, pixels[2]);
    clImageDestroy(C, image);

    image = clImageParseString(C, "(255,255,255)", 16, NULL);
    TEST_ASSERT_NOT_NULL(image);
    pixels = (uint16_t *)image->pixels;
    TEST_ASSERT_EQUAL_INT(65535, pixels[0]);
    TEST_ASSERT_EQUAL_INT(65535, pixels[1]);
    TEST_ASSERT_EQUAL_INT(65535, pixels[2]);
    clImageDestroy(C, image);

    image = clImageParseString(C, "(255,0,0)", 16, NULL);
    TEST_ASSERT_NOT_NULL(image);
    pixels = (uint16_t *)image->pixels;
    TEST_ASSERT_EQUAL_INT(65535, pixels[0]);
    TEST_ASSERT_EQUAL_INT(0, pixels[1]);
    TEST_ASSERT_EQUAL_INT(0, pixels[2]);
    clImageDestroy(C, image);

    image = clImageParseString(C, "rgb16(1,2,3)", 16, NULL);
    TEST_ASSERT_NOT_NULL(image);
    pixels = (uint16_t *)image->pixels;
    TEST_ASSERT_EQUAL_INT(1, pixels[0]);
    TEST_ASSERT_EQUAL_INT(2, pixels[1]);
    TEST_ASSERT_EQUAL_INT(3, pixels[2]);
    TEST_ASSERT_EQUAL_INT(65535, pixels[3]);
    clImageDestroy(C, image);

    image = clImageParseString(C, "rgba16(1,2,3,4)", 16, NULL);
    TEST_ASSERT_NOT_NULL(image);
    pixels = (uint16_t *)image->pixels;
    TEST_ASSERT_EQUAL_INT(1, pixels[0]);
    TEST_ASSERT_EQUAL_INT(2, pixels[1]);
    TEST_ASSERT_EQUAL_INT(3, pixels[2]);
    TEST_ASSERT_EQUAL_INT(4, pixels[3]);
    clImageDestroy(C, image);

    image = clImageParseString(C, "rgba16(65532,27302,13476)", 16, NULL);
    TEST_ASSERT_NOT_NULL(image);
    pixels = (uint16_t *)image->pixels;
    TEST_ASSERT_EQUAL_INT(65532, pixels[0]);
    TEST_ASSERT_EQUAL_INT(27302, pixels[1]);
    TEST_ASSERT_EQUAL_INT(13476, pixels[2]);
    clImageDestroy(C, image);

    clContextDestroy(C);
}

int test_strings(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_basic_hexcodes);
    RUN_TEST(test_basic_parens_8bit);
    RUN_TEST(test_basic_parens_16bit);

    return UNITY_END();
}
