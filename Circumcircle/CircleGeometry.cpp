#include "CircleGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace
{
bool IsValidBuffer(const std::uint32_t* pixels, int width, int height)
{
    return pixels != nullptr && width > 0 && height > 0 &&
        static_cast<std::size_t>(height) <=
            (std::numeric_limits<std::size_t>::max)() /
            static_cast<std::size_t>(width);
}
}

namespace geometry
{
bool CalculateCircumcircle(const Point& first, const Point& second,
    const Point& third, Circle& result)
{
    result = {};

    // Translate the first point to the origin to reduce cancellation. Cast
    // before subtraction so even opposite extreme integer coordinates work.
    const double deltaAX = static_cast<double>(second.x) - first.x;
    const double deltaAY = static_cast<double>(second.y) - first.y;
    const double deltaBX = static_cast<double>(third.x) - first.x;
    const double deltaBY = static_cast<double>(third.y) - first.y;
    const double coordinateScale = (std::max)({std::abs(deltaAX),
        std::abs(deltaAY), std::abs(deltaBX), std::abs(deltaBY)});
    if (coordinateScale == 0.0)
    {
        return false;
    }
    const double ax = deltaAX / coordinateScale;
    const double ay = deltaAY / coordinateScale;
    const double bx = deltaBX / coordinateScale;
    const double by = deltaBY / coordinateScale;
    const double aSquared = ax * ax + ay * ay;
    const double bSquared = bx * bx + by * by;
    const double cross = ax * by - ay * bx;
    const double scaleSquared = (std::max)(aSquared, bSquared);
    constexpr double relativeEpsilon = 1.0e-9;

    // Scale the determinant threshold with the input triangle, rather than
    // imposing a maximum circle radius or requiring it to fit the viewport.
    if (scaleSquared == 0.0 ||
        std::abs(cross) <= relativeEpsilon * scaleSquared)
    {
        return false;
    }

    const double denominator = 2.0 * cross;
    const double localCenterX =
        ((aSquared * by - bSquared * ay) / denominator) * coordinateScale;
    const double localCenterY =
        ((ax * bSquared - bx * aSquared) / denominator) * coordinateScale;

    const Circle candidate{
        first.x + localCenterX,
        first.y + localCenterY,
        std::hypot(localCenterX, localCenterY)
    };
    if (!std::isfinite(candidate.centerX) ||
        !std::isfinite(candidate.centerY) ||
        !std::isfinite(candidate.radius) || candidate.radius <= 0.0)
    {
        return false;
    }

    result = candidate;
    return true;
}

void DrawFilledPointCircle(std::uint32_t* pixels, int width, int height,
    const Point& center, int radius, std::uint32_t color)
{
    if (!IsValidBuffer(pixels, width, height) || radius <= 0)
    {
        return;
    }

    // Clip before looping. 64-bit subtraction and squares prevent overflow
    // for arbitrary int centers and radii; clipped offsets cannot exceed r.
    const std::int64_t left = (std::max)(std::int64_t{0},
        static_cast<std::int64_t>(center.x) - radius);
    const std::int64_t top = (std::max)(std::int64_t{0},
        static_cast<std::int64_t>(center.y) - radius);
    const std::int64_t right = (std::min)(static_cast<std::int64_t>(width) - 1,
        static_cast<std::int64_t>(center.x) + radius);
    const std::int64_t bottom = (std::min)(static_cast<std::int64_t>(height) - 1,
        static_cast<std::int64_t>(center.y) + radius);
    if (left > right || top > bottom)
    {
        return;
    }

    const std::int64_t radiusSquared =
        static_cast<std::int64_t>(radius) * radius;
    for (int y = static_cast<int>(top); y <= static_cast<int>(bottom); ++y)
    {
        const std::int64_t dy = static_cast<std::int64_t>(y) - center.y;
        auto* row = pixels + static_cast<std::size_t>(y) * width;
        for (int x = static_cast<int>(left); x <= static_cast<int>(right); ++x)
        {
            const std::int64_t dx = static_cast<std::int64_t>(x) - center.x;
            if (dx * dx + dy * dy <= radiusSquared)
            {
                row[x] = color;
            }
        }
    }
}

void DrawCircleRaster(std::uint32_t* pixels, int width, int height,
    const Circle& circle, int thickness, std::uint32_t color)
{
    if (!IsValidBuffer(pixels, width, height) || thickness <= 0 ||
        !std::isfinite(circle.centerX) || !std::isfinite(circle.centerY) ||
        !std::isfinite(circle.radius) || circle.radius <= 0.0)
    {
        return;
    }

    const double halfThickness = thickness * 0.5;
    // Keep the interior hollow even when the requested thickness exceeds the
    // diameter. In that case shift the extra stroke outward, without moving
    // the mathematical circle or dropping any of its circumference.
    const double minimumInnerRadius = (std::min)(circle.radius, 1.0);
    const double innerOffset = (std::max)(-halfThickness,
        minimumInnerRadius - circle.radius);
    const double outerOffset = innerOffset + thickness;
    const double originDistance = std::hypot(circle.centerX, circle.centerY);
    const double viewportDiagonal = std::hypot(width - 1.0, height - 1.0);
    const double originOffset = originDistance - circle.radius;

    // The reverse triangle inequality quickly rejects wholly invisible
    // strokes. No radius-dependent loop can stall the UI for a huge circle.
    if (!std::isfinite(originDistance) ||
        std::abs(originOffset) > viewportDiagonal + outerOffset)
    {
        return;
    }

    const bool distantCenter = originDistance > 2.0 * viewportDiagonal;
    const double unitX = distantCenter ? circle.centerX / originDistance : 0.0;
    const double unitY = distantCenter ? circle.centerY / originDistance : 0.0;

    for (int y = 0; y < height; ++y)
    {
        auto* row = pixels + static_cast<std::size_t>(y) * width;
        for (int x = 0; x < width; ++x)
        {
            const double distance = std::hypot(
                x - circle.centerX, y - circle.centerY);
            double radialOffset = distance - circle.radius;
            if (distantCenter)
            {
                // For huge circles, distance - radius can lose the entire
                // pixel offset. Rationalize distance - originDistance:
                // (x*x + y*y - 2*cx*x - 2*cy*y) / (distance + originDistance).
                // Dividing numerator and denominator by originDistance also
                // avoids squaring huge centers or overflowing their sum.
                const double numerator =
                    (x / originDistance) * x + (y / originDistance) * y -
                    2.0 * unitX * x - 2.0 * unitY * y;
                radialOffset = originOffset +
                    numerator / (distance / originDistance + 1.0);
            }

            // Pixel centers in this annulus form the requested border.
            if (radialOffset >= innerOffset && radialOffset <= outerOffset)
            {
                row[x] = color;
            }
        }
    }
}
}
