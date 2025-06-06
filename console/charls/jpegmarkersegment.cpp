//
// (C) CharLS Team 2014, all rights reserved. See the accompanying "License.txt" for licensed use.
//

#include "jpegmarkersegment.h"
#include "jpegmarkercode.h"
#include "util.h"
#include <vector>
#include <cstdint>

using namespace charls;

std::unique_ptr<JpegMarkerSegment> JpegMarkerSegment::CreateStartOfFrameSegment(int width, int height, int bitsPerSample, int componentCount)
{
    ASSERT(width >= 0 && width <= UINT16_MAX);
    ASSERT(height >= 0 && height <= UINT16_MAX);
    ASSERT(bitsPerSample > 0 && bitsPerSample <= UINT8_MAX);
    ASSERT(componentCount > 0 && componentCount <= (UINT8_MAX - 1));

    // Create a Frame Header as defined in T.87, C.2.2 and T.81, B.2.2
    std::vector<uint8_t> content;
    content.push_back(static_cast<uint8_t>(bitsPerSample)); // P = Sample precision
    push_back(content, static_cast<uint16_t>(height));      // Y = Number of lines
    push_back(content, static_cast<uint16_t>(width));       // X = Number of samples per line

    // Components
    content.push_back(static_cast<uint8_t>(componentCount)); // Nf = Number of image components in frame
    for (auto component = 0; component < componentCount; ++component)
    {
        // Component Specification parameters
        content.push_back(static_cast<uint8_t>(component + 1)); // Ci = Component identifier
        content.push_back(0x11);                                // Hi + Vi = Horizontal sampling factor + Vertical sampling factor
        content.push_back(0);                                   // Tqi = Quantization table destination selector (reserved for JPEG-LS, should be set to 0)
    }

    return std::make_unique<JpegMarkerSegment>(JpegMarkerCode::StartOfFrameJpegLS, std::move(content));
}


std::unique_ptr<JpegMarkerSegment> JpegMarkerSegment::CreateJpegFileInterchangeFormatSegment(const JfifParameters& params)
{
    ASSERT(params.units == 0 || params.units == 1 || params.units == 2);
    ASSERT(params.Xdensity > 0);
    ASSERT(params.Ydensity > 0);
    ASSERT(params.Xthumbnail >= 0 && params.Xthumbnail < 256);
    ASSERT(params.Ythumbnail >= 0 && params.Ythumbnail < 256);

    // Create a JPEG APP0 segment in the JPEG File Interchange Format (JFIF), v1.02
    std::vector<uint8_t> content { 'J', 'F', 'I', 'F', '\0' };
    push_back(content, static_cast<uint16_t>(params.version));
    content.push_back(static_cast<uint8_t>(params.units));
    push_back(content, static_cast<uint16_t>(params.Xdensity));
    push_back(content, static_cast<uint16_t>(params.Ydensity));

    // thumbnail
    content.push_back(static_cast<uint8_t>(params.Xthumbnail));
    content.push_back(static_cast<uint8_t>(params.Ythumbnail));
    if (params.Xthumbnail > 0)
    {
        if (params.thumbnail)
            throw charls_error(ApiResult::InvalidJlsParameters, "params.Xthumbnail is > 0 but params.thumbnail == null_ptr");

        content.insert(content.end(), static_cast<uint8_t*>(params.thumbnail),
            static_cast<uint8_t*>(params.thumbnail) + static_cast<size_t>(3) * params.Xthumbnail * params.Ythumbnail);
    }

    return std::make_unique<JpegMarkerSegment>(JpegMarkerCode::ApplicationData0, std::move(content));
}


std::unique_ptr<JpegMarkerSegment> JpegMarkerSegment::CreateJpegLSPresetParametersSegment(const JpegLSPresetCodingParameters& params)
{
    std::vector<uint8_t> content;

    // Parameter ID. 0x01 = JPEG-LS preset coding parameters.
    content.push_back(1);

    push_back(content, static_cast<uint16_t>(params.MaximumSampleValue));
    push_back(content, static_cast<uint16_t>(params.Threshold1));
    push_back(content, static_cast<uint16_t>(params.Threshold2));
    push_back(content, static_cast<uint16_t>(params.Threshold3));
    push_back(content, static_cast<uint16_t>(params.ResetValue));

    return std::make_unique<JpegMarkerSegment>(JpegMarkerCode::JpegLSPresetParameters, std::move(content));
}


std::unique_ptr<JpegMarkerSegment> JpegMarkerSegment::CreateColorTransformSegment(ColorTransformation transformation)
{
    return std::make_unique<JpegMarkerSegment>(
        JpegMarkerCode::ApplicationData8,
        std::vector<uint8_t> { 'm', 'r', 'f', 'x', static_cast<uint8_t>(transformation) });
}


std::unique_ptr<JpegMarkerSegment> JpegMarkerSegment::CreateStartOfScanSegment(int componentIndex, int componentCount, int allowedLossyError, InterleaveMode interleaveMode)
{
    ASSERT(componentIndex >= 0);
    ASSERT(componentCount > 0);

    // Create a Scan Header as defined in T.87, C.2.3 and T.81, B.2.3
    std::vector<uint8_t> content;

    content.push_back(static_cast<uint8_t>(componentCount));
    for (auto i = 0; i < componentCount; ++i)
    {
        content.push_back(static_cast<uint8_t>(componentIndex + i));
        content.push_back(0);  // Mapping table selector (0 = no table)
    }

    content.push_back(static_cast<uint8_t>(allowedLossyError)); // NEAR parameter
    content.push_back(static_cast<uint8_t>(interleaveMode)); // ILV parameter
    content.push_back(0); // transformation

    return std::make_unique<JpegMarkerSegment>(JpegMarkerCode::StartOfScan, std::move(content));
}
