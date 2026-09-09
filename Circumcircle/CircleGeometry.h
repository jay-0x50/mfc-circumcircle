#pragma once

#include <cstdint>

namespace geometry
{
struct Point
{
    int x = 0;
    int y = 0;
};

struct Circle
{
    double centerX = 0.0;
    double centerY = 0.0;
    double radius = 0.0;
};

// On failure, result is reset to an empty circle.
bool CalculateCircumcircle(const Point& first, const Point& second,
    const Point& third, Circle& result);

// Buffers are top down, tightly packed, with colors encoded as 0x00RRGGBB.
// Integer coordinates denote pixel centers. Drawing is clipped to the buffer.
void DrawFilledPointCircle(std::uint32_t* pixels, int width, int height,
    const Point& center, int radius, std::uint32_t color);

// The stroke is centered on the mathematical circumference where possible.
// A hollow inner radius of min(radius, 1 pixel) is preserved; excess thickness
// extends outward. The center and radius are never rounded or moved to fit.
void DrawCircleRaster(std::uint32_t* pixels, int width, int height,
    const Circle& circle, int thickness, std::uint32_t color);
}
