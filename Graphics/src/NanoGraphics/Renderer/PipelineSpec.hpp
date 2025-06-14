#pragma once

#include <cstdint>

namespace Nano::Graphics
{

    ////////////////////////////////////////////////////////////////////////////////////
    // Flags
    ////////////////////////////////////////////////////////////////////////////////////
    enum class PrimitiveType : uint8_t
    {
        PointList = 0,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        TriangleFan,
        TriangleListWithAdjacency,
        TriangleStripWithAdjacency,
        PatchList
    };

    ////////////////////////////////////////////////////////////////////////////////////
    // GraphicsPipelineSpecification
    ////////////////////////////////////////////////////////////////////////////////////
    struct GraphicsPipelineSpecification
    {
    public:
    };

}