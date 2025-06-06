//
// (C) Jan de Vaan 2007-2010, all rights reserved. See the accompanying "License.txt" for licensed use.
//

#include "jpegstreamreader.h"
#include "util.h"
#include "jpegstreamwriter.h"
#include "jpegimagedatasegment.h"
#include "jpegmarkercode.h"
#include "decoderstrategy.h"
#include "encoderstrategy.h"
#include "jlscodecfactory.h"
#include "constants.h"
#include <memory>
#include <iomanip>
#include <algorithm>

using namespace charls;

extern template class JlsCodecFactory<EncoderStrategy>;
extern template class JlsCodecFactory<DecoderStrategy>;

namespace {


// JFIF\0
uint8_t jfifID[] = { 'J', 'F', 'I', 'F', '\0' };


/// <summary>Clamping function as defined by ISO/IEC 14495-1, Figure C.3</summary>
int32_t clamp(int32_t i, int32_t j, int32_t maximumSampleValue) noexcept
{
    if (i > maximumSampleValue || i < j)
        return j;

    return i;
}

 
ApiResult CheckParameterCoherent(const JlsParameters& params) noexcept
{
    if (params.bitsPerSample < 2 || params.bitsPerSample > 16)
        return ApiResult::ParameterValueNotSupported;

    if (params.interleaveMode < InterleaveMode::None || params.interleaveMode > InterleaveMode::Sample)
        return ApiResult::InvalidCompressedData;

    switch (params.components)
    {
        case 4: return params.interleaveMode == InterleaveMode::Sample ? ApiResult::ParameterValueNotSupported : ApiResult::OK;
        case 3: return ApiResult::OK;
        case 0: return ApiResult::InvalidJlsParameters;

        default: return params.interleaveMode != InterleaveMode::None ? ApiResult::ParameterValueNotSupported : ApiResult::OK;
    }
}

} // namespace


JpegLSPresetCodingParameters ComputeDefault(int32_t maximumSampleValue, int32_t allowedLossyError) noexcept
{
    JpegLSPresetCodingParameters preset;

    const int32_t factor = (std::min(maximumSampleValue, 4095) + 128) / 256;
    const int threshold1 = clamp(factor * (DefaultThreshold1 - 2) + 2 + 3 * allowedLossyError, allowedLossyError + 1, maximumSampleValue);
    const int threshold2 = clamp(factor * (DefaultThreshold2 - 3) + 3 + 5 * allowedLossyError, threshold1, maximumSampleValue); //-V537

    preset.Threshold1 = threshold1;
    preset.Threshold2 = threshold2;
    preset.Threshold3 = clamp(factor * (DefaultThreshold3 - 4) + 4 + 7 * allowedLossyError, threshold2, maximumSampleValue);
    preset.MaximumSampleValue = maximumSampleValue;
    preset.ResetValue = DefaultResetValue;
    return preset;
}


void JpegImageDataSegment::Serialize(JpegStreamWriter& streamWriter)
{
    JlsParameters info = _params;
    info.components = _componentCount;
    auto codec = JlsCodecFactory<EncoderStrategy>().CreateCodec(info, _params.custom);
    std::unique_ptr<ProcessLine> processLine(codec->CreateProcess(_rawStreamInfo));
    ByteStreamInfo compressedData = streamWriter.OutputStream();
    const size_t cbyteWritten = codec->EncodeScan(std::move(processLine), compressedData);
    streamWriter.Seek(cbyteWritten);
}


JpegStreamReader::JpegStreamReader(ByteStreamInfo byteStreamInfo) noexcept :
    _byteStream(byteStreamInfo),
    _params(),
    _rect()
{
}


void JpegStreamReader::Read(ByteStreamInfo rawPixels)
{
    ReadHeader();

    const auto result = CheckParameterCoherent(_params);
    if (result != ApiResult::OK)
        throw charls_error(result);

    if (_rect.Width <= 0)
    {
        _rect.Width = _params.width;
        _rect.Height = _params.height;
    }

    const int64_t bytesPerPlane = static_cast<int64_t>(_rect.Width) * _rect.Height * ((_params.bitsPerSample + 7)/8);

    if (rawPixels.rawData && static_cast<int64_t>(rawPixels.count) < bytesPerPlane * _params.components)
        throw charls_error(ApiResult::UncompressedBufferTooSmall);

    int componentIndex = 0;

    while (componentIndex < _params.components)
    {
        ReadStartOfScan(componentIndex == 0);

        std::unique_ptr<DecoderStrategy> qcodec = JlsCodecFactory<DecoderStrategy>().CreateCodec(_params, _params.custom);
        std::unique_ptr<ProcessLine> processLine(qcodec->CreateProcess(rawPixels));
        qcodec->DecodeScan(std::move(processLine), _rect, _byteStream);
        SkipBytes(rawPixels, static_cast<size_t>(bytesPerPlane));

        if (_params.interleaveMode != InterleaveMode::None)
            return;

        componentIndex += 1;
    }
}


void JpegStreamReader::ReadNBytes(std::vector<char>& dst, int byteCount)
{
    for (int i = 0; i < byteCount; ++i)
    {
        dst.push_back(static_cast<char>(ReadByte()));
    }
}


void JpegStreamReader::ReadHeader()
{
    if (ReadNextMarker() != JpegMarkerCode::StartOfImage)
        throw charls_error(ApiResult::InvalidCompressedData);

    for (;;)
    {
        const JpegMarkerCode marker = ReadNextMarker();
        if (marker == JpegMarkerCode::StartOfScan)
            return;

        const int32_t cbyteMarker = ReadWord();
        const int bytesRead = ReadMarker(marker) + 2;

        const int paddingToRead = cbyteMarker - bytesRead;
        if (paddingToRead < 0)
            throw charls_error(ApiResult::InvalidCompressedData);

        for (int i = 0; i < paddingToRead; ++i)
        {
            ReadByte();
        }
    }
}


JpegMarkerCode JpegStreamReader::ReadNextMarker()
{
    auto byte = ReadByte();
    if (byte != 0xFF)
    {
        std::ostringstream message;
        message << std::setfill('0');
        message << "Expected JPEG Marker start byte 0xFF but the byte value was 0x" << std::hex << std::uppercase
                << std::setw(2) << static_cast<unsigned int>(byte);
        throw charls_error(ApiResult::MissingJpegMarkerStart, message.str());
    }

    // Read all preceding 0xFF fill values until a non 0xFF value has been found. (see T.81, B.1.1.2)
    do
    {
        byte = ReadByte();
    } while (byte == 0xFF);

    return static_cast<JpegMarkerCode>(byte);
}


int JpegStreamReader::ReadMarker(JpegMarkerCode marker)
{
    // ISO/IEC 14495-1, ITU-T Recommendation T.87, C.1.1. defines the following markers valid for a JPEG-LS byte stream:
    // SOF55, LSE, SOI, EOI, SOS, DNL, DRI, RSTm, APPn, COM.
    // All other markers shall not be present.
    switch (marker)
    {
        case JpegMarkerCode::StartOfFrameJpegLS:
            return ReadStartOfFrame();

        case JpegMarkerCode::Comment:
            return ReadComment();

        case JpegMarkerCode::JpegLSPresetParameters:
            return ReadPresetParameters();

        case JpegMarkerCode::ApplicationData0:
            return 0;

        case JpegMarkerCode::ApplicationData7:
            return ReadColorSpace();

        case JpegMarkerCode::ApplicationData8:
            return ReadColorXForm();

        case JpegMarkerCode::StartOfFrameBaselineJpeg:
        case JpegMarkerCode::StartOfFrameExtendedSequential:
        case JpegMarkerCode::StartOfFrameProgressive:
        case JpegMarkerCode::StartOfFrameLossless:
        case JpegMarkerCode::StartOfFrameDifferentialSequential:
        case JpegMarkerCode::StartOfFrameDifferentialProgressive:
        case JpegMarkerCode::StartOfFrameDifferentialLossless:
        case JpegMarkerCode::StartOfFrameExtendedArithemtic:
        case JpegMarkerCode::StartOfFrameProgressiveArithemtic:
        case JpegMarkerCode::StartOfFrameLosslessArithemtic:
            {
                std::ostringstream message;
                message << "JPEG encoding with marker " << static_cast<unsigned int>(marker) << " is not supported.";
                throw charls_error(ApiResult::UnsupportedEncoding, message.str());
            }

        // Other tags not supported (among which DNL DRI)
        default:
            {
                std::ostringstream message;
                message << "Unknown JPEG marker " << static_cast<unsigned int>(marker) << " encountered.";
                throw charls_error(ApiResult::UnknownJpegMarker, message.str());
            }
    }
}


int JpegStreamReader::ReadPresetParameters()
{
    const int type = ReadByte();

    switch (type)
    {
    case 1:
        {
            _params.custom.MaximumSampleValue = ReadWord();
            _params.custom.Threshold1 = ReadWord();
            _params.custom.Threshold2 = ReadWord();
            _params.custom.Threshold3 = ReadWord();
            _params.custom.ResetValue = ReadWord();
            return 11;
        }

    case 2: // mapping table specification
    case 3: // mapping table continuation
    case 4: // X and Y parameters greater than 16 bits are defined.
        {
            std::ostringstream message;
            message << "JPEG-LS preset parameters with type " << static_cast<unsigned int>(type) << " are not supported.";
            throw charls_error(ApiResult::UnsupportedEncoding, message.str());
        }
    default:
        {
            std::ostringstream message;
            message << "JPEG-LS preset parameters with invalid type " << static_cast<unsigned int>(type) << " encountered.";
            throw charls_error(ApiResult::InvalidJlsParameters, message.str());
        }
    }
}


void JpegStreamReader::ReadStartOfScan(bool firstComponent)
{
    if (!firstComponent)
    {
        if (ReadByte() != 0xFF)
            throw charls_error(ApiResult::MissingJpegMarkerStart);
        if (static_cast<JpegMarkerCode>(ReadByte()) != JpegMarkerCode::StartOfScan)
            throw charls_error(ApiResult::InvalidCompressedData);// TODO: throw more specific error code.
    }
    int length = ReadByte();
    length = length * 256 + ReadByte(); // TODO: do something with 'length' or remove it.

    const int componentCount = ReadByte();
    if (componentCount != 1 && componentCount != _params.components)
        throw charls_error(ApiResult::ParameterValueNotSupported);

    for (int i = 0; i < componentCount; ++i)
    {
        ReadByte();
        ReadByte();
    }
    _params.allowedLossyError = ReadByte();
    _params.interleaveMode = static_cast<InterleaveMode>(ReadByte());
    if (!(_params.interleaveMode == InterleaveMode::None || _params.interleaveMode == InterleaveMode::Line || _params.interleaveMode == InterleaveMode::Sample))
        throw charls_error(ApiResult::InvalidCompressedData);// TODO: throw more specific error code.
    if (ReadByte() != 0)
        throw charls_error(ApiResult::InvalidCompressedData);// TODO: throw more specific error code.

    if(_params.stride == 0)
    {
        const int width = _rect.Width != 0 ? _rect.Width : _params.width;
        const int components = _params.interleaveMode == InterleaveMode::None ? 1 : _params.components;
        _params.stride = components * width * ((_params.bitsPerSample + 7) / 8);
    }
}


int JpegStreamReader::ReadComment() noexcept
{
    return 0;
}


void JpegStreamReader::ReadJfif()
{
    for(int i = 0; i < static_cast<int>(sizeof(jfifID)); i++)
    {
        if(jfifID[i] != ReadByte())
            return;
    }
    _params.jfif.version   = ReadWord();

    // DPI or DPcm
    _params.jfif.units = ReadByte();
    _params.jfif.Xdensity = ReadWord();
    _params.jfif.Ydensity = ReadWord();

    // thumbnail
    _params.jfif.Xthumbnail = ReadByte();
    _params.jfif.Ythumbnail = ReadByte();
    if(_params.jfif.Xthumbnail > 0 && _params.jfif.thumbnail)
    {
        std::vector<char> tempbuff(static_cast<char*>(_params.jfif.thumbnail),
            static_cast<char*>(_params.jfif.thumbnail) + static_cast<size_t>(3) * _params.jfif.Xthumbnail * _params.jfif.Ythumbnail);
        ReadNBytes(tempbuff, 3*_params.jfif.Xthumbnail*_params.jfif.Ythumbnail);
    }
}


int JpegStreamReader::ReadStartOfFrame()
{
    _params.bitsPerSample = ReadByte();
    _params.height = ReadWord();
    _params.width = ReadWord();
    _params.components= ReadByte();
    return 6;
}


uint8_t JpegStreamReader::ReadByte()
{
    if (_byteStream.rawStream)
        return static_cast<uint8_t>(_byteStream.rawStream->sbumpc());

    if (_byteStream.count == 0)
        throw charls_error(ApiResult::CompressedBufferTooSmall);

    const uint8_t value = _byteStream.rawData[0];
    SkipBytes(_byteStream, 1);
    return value;
}


int JpegStreamReader::ReadWord()
{
    const int i = ReadByte() * 256;
    return i + ReadByte();
}


int JpegStreamReader::ReadColorSpace() const noexcept
{
    return 0;
}


int JpegStreamReader::ReadColorXForm()
{
    std::vector<char> sourceTag;
    ReadNBytes(sourceTag, 4);

    if (strncmp(sourceTag.data(), "mrfx", 4) != 0)
        return 4;

    const auto xform = ReadByte();
    switch (xform)
    {
        case static_cast<uint8_t>(ColorTransformation::None):
        case static_cast<uint8_t>(ColorTransformation::HP1):
        case static_cast<uint8_t>(ColorTransformation::HP2):
        case static_cast<uint8_t>(ColorTransformation::HP3):
            _params.colorTransformation = static_cast<ColorTransformation>(xform);
            return 5;

        case 4: // RgbAsYuvLossy (The standard lossy RGB to YCbCr transform used in JPEG.)
        case 5: // Matrix (transformation is controlled using a matrix that is also stored in the segment.
            throw charls_error(ApiResult::ImageTypeNotSupported);
        default:
            throw charls_error(ApiResult::InvalidCompressedData);
    }
}
