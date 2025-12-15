#pragma once
#include <vector>
#include <cstdint>

class JpegEncoder {
public:
    virtual ~JpegEncoder() = default;

    virtual bool EncodeRGB(
        const uint8_t* rgb,
        int w, int h,
        int quality,
        std::vector<uint8_t>& outJpeg
    ) = 0;
};

// factory
JpegEncoder* CreateJpegEncoder();
