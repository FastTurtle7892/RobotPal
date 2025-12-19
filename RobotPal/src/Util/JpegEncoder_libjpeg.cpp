/**
 * @file JpegEncoder_libjpeg.cpp
 * @author Hong YoonPyo (cgantro@gmail.com)
 * @brief JPEG encoder implementation using libjpeg
 * @version 0.1
 * @date 2025-12-15
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "RobotPal/Util/JpegEncoder.h"
#include <stdio.h>

extern "C" {
#include <jpeglib.h>
}

#include <cstdlib>

namespace {

inline boolean jpeg_bool(bool v) {
    return static_cast<boolean>(v ? 1 : 0);
}

class LibJpegEncoder final : public JpegEncoder {
public:
    bool EncodeRGB(
        const uint8_t* rgb,
        int w, int h,
        int quality,
        std::vector<uint8_t>& outJpeg
    ) override {
        if (!rgb || w <= 0 || h <= 0) return false;

        jpeg_compress_struct cinfo{};
        jpeg_error_mgr jerr{};

        unsigned char* jpegBuf = nullptr;
        unsigned long jpegSize = 0;

        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&cinfo);
        jpeg_mem_dest(&cinfo, &jpegBuf, &jpegSize);

        cinfo.image_width = w;
        cinfo.image_height = h;
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;

        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, quality, jpeg_bool(true));
        jpeg_start_compress(&cinfo, jpeg_bool(true));

        JSAMPROW row[1];
        while (cinfo.next_scanline < cinfo.image_height) {
            row[0] = const_cast<JSAMPROW>(
                rgb + cinfo.next_scanline * w * 3
            );
            jpeg_write_scanlines(&cinfo, row, 1);
        }

        jpeg_finish_compress(&cinfo);

        outJpeg.assign(jpegBuf, jpegBuf + jpegSize);

        jpeg_destroy_compress(&cinfo);
        free(jpegBuf);

        return !outJpeg.empty();
    }
};

} // namespace

JpegEncoder* CreateJpegEncoder() {
    static LibJpegEncoder encoder;
    return &encoder;
}
