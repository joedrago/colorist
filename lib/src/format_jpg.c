// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"
#include "colorist/raw.h"

#include "lcms2.h"

#include "jpeglib.h"

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

struct my_error_mgr
{
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};
typedef struct my_error_mgr * my_error_ptr;
static void my_error_exit(j_common_ptr cinfo)
{
    my_error_ptr myerr = (my_error_ptr)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}

static void setup_read_icc_profile(j_decompress_ptr cinfo);
static boolean read_icc_profile(struct clContext * C, j_decompress_ptr cinfo, JOCTET ** icc_data_ptr, unsigned int * icc_data_len);
static void write_icc_profile(j_compress_ptr cinfo, const JOCTET * icc_data_ptr, unsigned int icc_data_len);

struct clImage * clFormatReadJPG(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input);
clBool clFormatWriteJPG(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

struct clImage * clFormatReadJPG(struct clContext * C, const char * formatName, struct clProfile * overrideProfile, struct clRaw * input)
{
    COLORIST_UNUSED(formatName);

    clImage * image = NULL;

    struct my_error_mgr jerr;
    struct jpeg_decompress_struct cinfo;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        if (image) {
            clImageDestroy(C, image);
        }
        jpeg_destroy_decompress(&cinfo);
        return 0;
    }

    jpeg_create_decompress(&cinfo);
    setup_read_icc_profile(&cinfo);
    jpeg_mem_src(&cinfo, input->ptr, (unsigned long)input->size);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    int row_stride = cinfo.output_width * cinfo.output_components;
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) & cinfo, JPOOL_IMAGE, row_stride, 1);

    clProfile * profile = NULL;
    if (overrideProfile) {
        profile = clProfileClone(C, overrideProfile);
    } else {
        uint8_t * iccData = NULL;
        unsigned int iccDataLen;
        if (read_icc_profile(C, &cinfo, &iccData, &iccDataLen)) {
            profile = clProfileParse(C, iccData, iccDataLen, NULL);
            if (!profile) {
                clContextLogError(C, "ERROR: can't parse JPEG embedded ICC profile");
                jpeg_destroy_decompress(&cinfo);
                return NULL;
            }
            clFree(iccData);
        }
    }

    clImageLogCreate(C, cinfo.output_width, cinfo.output_height, 8, profile);
    image = clImageCreate(C, cinfo.output_width, cinfo.output_height, 8, profile);

    if (profile) {
        clProfileDestroy(C, profile);
    }

    int row = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        uint16_t * pixelRow = &image->pixels[row * image->width * CL_CHANNELS_PER_PIXEL];
        for (unsigned int i = 0; i < cinfo.output_width; ++i) {
            uint16_t * dst = &pixelRow[i * CL_CHANNELS_PER_PIXEL];
            uint8_t * src = &buffer[0][i * 3];
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = 255;
        }
        ++row;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return image;
}

clBool clFormatWriteJPG(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams)
{
    COLORIST_UNUSED(formatName);

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    JSAMPROW row_pointer[1];
    int row_stride;
    unsigned char * outbuffer = NULL;
    unsigned long outsize = 0;

    clRaw rawProfile = CL_RAW_EMPTY;
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        return clFalse;
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &outbuffer, &outsize);

    uint8_t * jpegPixels = clAllocate(3 * image->width * image->height);
    clImageToRGB8(C, image, jpegPixels);

    cinfo.image_width = image->width;
    cinfo.image_height = image->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, writeParams->quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    if (writeParams->writeProfile) {
        write_icc_profile(&cinfo, rawProfile.ptr, (unsigned int)rawProfile.size);
    }

    row_stride = image->width * 3;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &jpegPixels[cinfo.next_scanline * row_stride];
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);

    if (outbuffer && outsize) {
        clRawSet(C, output, outbuffer, outsize);
    } else {
        clContextLogError(C, "ERROR: JPG compression failed");
        clRawFree(C, output);
    }
    free(outbuffer);

    jpeg_destroy_compress(&cinfo);
    clFree(jpegPixels);
    clRawFree(C, &rawProfile);
    return (output->size > 0) ? clTrue : clFalse;
}

// ----------------------------------------------------------------------------
// Taken from http://www.littlecms.com/1/iccjpeg.c
// Minor adaptations for compilation / formattingv

/*
 * iccprofile.c
 *
 * This file provides code to read and write International Color Consortium
 * (ICC) device profiles embedded in JFIF JPEG image files.  The ICC has
 * defined a standard format for including such data in JPEG "APP2" markers.
 * The code given here does not know anything about the internal structure
 * of the ICC profile data; it just knows how to put the profile data into
 * a JPEG file being written, or get it back out when reading.
 *
 * This code depends on new features added to the IJG JPEG library as of
 * IJG release 6b; it will not compile or work with older IJG versions.
 *
 * NOTE: this code would need surgery to work on 16-bit-int machines
 * with ICC profiles exceeding 64K bytes in size.  If you need to do that,
 * change all the "unsigned int" variables to "INT32".  You'll also need
 * to find a malloc() replacement that can allocate more than 64K.
 */

/*
 * Since an ICC profile can be larger than the maximum size of a JPEG marker
 * (64K), we need provisions to split it into multiple markers.  The format
 * defined by the ICC specifies one or more APP2 markers containing the
 * following data:
 *  Identifying string  ASCII "ICC_PROFILE\0"  (12 bytes)
 *  Marker sequence number  1 for first APP2, 2 for next, etc (1 byte)
 *  Number of markers   Total number of APP2's used (1 byte)
 *      Profile data        (remainder of APP2 data)
 * Decoders should use the marker sequence numbers to reassemble the profile,
 * rather than assuming that the APP2 markers appear in the correct sequence.
 */

#define ICC_MARKER (JPEG_APP0 + 2) /* JPEG marker code for ICC */
#define ICC_OVERHEAD_LEN 14        /* size of non-profile data in APP2 */
#define MAX_BYTES_IN_MARKER 65533  /* maximum data len of a JPEG marker */
#define MAX_DATA_BYTES_IN_MARKER (MAX_BYTES_IN_MARKER - ICC_OVERHEAD_LEN)

/*
 * This routine writes the given ICC profile data into a JPEG file.
 * It *must* be called AFTER calling jpeg_start_compress() and BEFORE
 * the first call to jpeg_write_scanlines().
 * (This ordering ensures that the APP2 marker(s) will appear after the
 * SOI and JFIF or Adobe markers, but before all else.)
 */

static void write_icc_profile(j_compress_ptr cinfo, const JOCTET * icc_data_ptr, unsigned int icc_data_len)
{
    unsigned int num_markers; /* total number of markers we'll write */
    int cur_marker = 1;       /* per spec, counting starts at 1 */

    /* Calculate the number of markers we'll need, rounding up of course */
    num_markers = icc_data_len / MAX_DATA_BYTES_IN_MARKER;
    if (num_markers * MAX_DATA_BYTES_IN_MARKER != icc_data_len)
        num_markers++;

    while (icc_data_len > 0) {
        /* length of profile to put in this marker */
        unsigned int length = icc_data_len;
        if (length > MAX_DATA_BYTES_IN_MARKER)
            length = MAX_DATA_BYTES_IN_MARKER;
        icc_data_len -= length;

        /* Write the JPEG marker header (APP2 code and marker length) */
        jpeg_write_m_header(cinfo, ICC_MARKER, length + ICC_OVERHEAD_LEN);

        /* Write the marker identifying string "ICC_PROFILE" (null-terminated).
         * We code it in this less-than-transparent way so that the code works
         * even if the local character set is not ASCII.
         */
        jpeg_write_m_byte(cinfo, 0x49);
        jpeg_write_m_byte(cinfo, 0x43);
        jpeg_write_m_byte(cinfo, 0x43);
        jpeg_write_m_byte(cinfo, 0x5F);
        jpeg_write_m_byte(cinfo, 0x50);
        jpeg_write_m_byte(cinfo, 0x52);
        jpeg_write_m_byte(cinfo, 0x4F);
        jpeg_write_m_byte(cinfo, 0x46);
        jpeg_write_m_byte(cinfo, 0x49);
        jpeg_write_m_byte(cinfo, 0x4C);
        jpeg_write_m_byte(cinfo, 0x45);
        jpeg_write_m_byte(cinfo, 0x0);

        /* Add the sequencing info */
        jpeg_write_m_byte(cinfo, cur_marker);
        jpeg_write_m_byte(cinfo, (int)num_markers);

        /* Add the profile data */
        while (length) {
            jpeg_write_m_byte(cinfo, *icc_data_ptr);
            icc_data_ptr++;
            --length;
        }
        cur_marker++;
    }
}

/*
 * Prepare for reading an ICC profile
 */

static void setup_read_icc_profile(j_decompress_ptr cinfo)
{
    /* Tell the library to keep any APP2 data it may find */
    jpeg_save_markers(cinfo, ICC_MARKER, 0xFFFF);
}

/*
 * Handy subroutine to test whether a saved marker is an ICC profile marker.
 */

static boolean marker_is_icc(jpeg_saved_marker_ptr marker)
{
    return marker->marker == ICC_MARKER && marker->data_length >= ICC_OVERHEAD_LEN &&
           /* verify the identifying string */
           GETJOCTET(marker->data[0]) == 0x49 && GETJOCTET(marker->data[1]) == 0x43 && GETJOCTET(marker->data[2]) == 0x43 &&
           GETJOCTET(marker->data[3]) == 0x5F && GETJOCTET(marker->data[4]) == 0x50 && GETJOCTET(marker->data[5]) == 0x52 &&
           GETJOCTET(marker->data[6]) == 0x4F && GETJOCTET(marker->data[7]) == 0x46 && GETJOCTET(marker->data[8]) == 0x49 &&
           GETJOCTET(marker->data[9]) == 0x4C && GETJOCTET(marker->data[10]) == 0x45 && GETJOCTET(marker->data[11]) == 0x0;
}

/*
 * See if there was an ICC profile in the JPEG file being read;
 * if so, reassemble and return the profile data.
 *
 * TRUE is returned if an ICC profile was found, FALSE if not.
 * If TRUE is returned, *icc_data_ptr is set to point to the
 * returned data, and *icc_data_len is set to its length.
 *
 * IMPORTANT: the data at **icc_data_ptr has been allocated with malloc()
 * and must be freed by the caller with clFree() when the caller no longer
 * needs it.  (Alternatively, we could write this routine to use the
 * IJG library's memory allocator, so that the data would be freed implicitly
 * at jpeg_finish_decompress() time.  But it seems likely that many apps
 * will prefer to have the data stick around after decompression finishes.)
 *
 * NOTE: if the file contains invalid ICC APP2 markers, we just silently
 * return FALSE.  You might want to issue an error message instead.
 */

static boolean read_icc_profile(struct clContext * C, j_decompress_ptr cinfo, JOCTET ** icc_data_ptr, unsigned int * icc_data_len)
{
    jpeg_saved_marker_ptr marker;
    int num_markers = 0;
    int seq_no;
    JOCTET * icc_data;
    unsigned int total_length;
#define MAX_SEQ_NO 255                        /* sufficient since marker numbers are bytes */
    char marker_present[MAX_SEQ_NO + 1];      /* 1 if marker found */
    unsigned int data_length[MAX_SEQ_NO + 1]; /* size of profile data in marker */
    unsigned int data_offset[MAX_SEQ_NO + 1]; /* offset for data in marker */

    *icc_data_ptr = NULL; /* avoid confusion if FALSE return */
    *icc_data_len = 0;

    /* This first pass over the saved markers discovers whether there are
     * any ICC markers and verifies the consistency of the marker numbering.
     */

    for (seq_no = 1; seq_no <= MAX_SEQ_NO; seq_no++) {
        marker_present[seq_no] = 0;
        data_length[seq_no] = 0;
    }

    for (marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
        if (marker_is_icc(marker)) {
            if (num_markers == 0)
                num_markers = GETJOCTET(marker->data[13]);
            else if (num_markers != GETJOCTET(marker->data[13]))
                return FALSE; /* inconsistent num_markers fields */
            seq_no = GETJOCTET(marker->data[12]);
            if ((seq_no <= 0) || (seq_no > num_markers))
                return FALSE; /* bogus sequence number */
            if (marker_present[seq_no])
                return FALSE; /* duplicate sequence numbers */
            marker_present[seq_no] = 1;
            data_length[seq_no] = marker->data_length - ICC_OVERHEAD_LEN;
        }
    }

    if (num_markers == 0)
        return FALSE;

    /* Check for missing markers, count total space needed,
     * compute offset of each marker's part of the data.
     */

    total_length = 0;
    for (seq_no = 1; seq_no <= num_markers; seq_no++) {
        if (marker_present[seq_no] == 0)
            return FALSE; /* missing sequence number */
        data_offset[seq_no] = total_length;
        total_length += data_length[seq_no];
    }

    if (!total_length)
        return FALSE; /* found only empty markers? */

    /* Allocate space for assembled data */
    icc_data = (JOCTET *)clAllocate(total_length * sizeof(JOCTET));
    if (icc_data == NULL)
        return FALSE; /* oops, out of memory */

    /* and fill it in */
    for (marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
        if (marker_is_icc(marker)) {
            JOCTET FAR * src_ptr;
            JOCTET * dst_ptr;
            unsigned int length;
            seq_no = GETJOCTET(marker->data[12]);
            dst_ptr = icc_data + data_offset[seq_no];
            src_ptr = marker->data + ICC_OVERHEAD_LEN;
            length = data_length[seq_no];
            while (length--) {
                *dst_ptr++ = *src_ptr++;
            }
        }
    }

    *icc_data_ptr = icc_data;
    *icc_data_len = total_length;

    return TRUE;
}
