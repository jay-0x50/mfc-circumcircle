#include "CircleGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
bool isValidBuffer(const unsigned char* fm, int nWidth, int nHeight,
    int nPitch, int nGray)
{
    return fm != nullptr && nWidth > 0 && nHeight > 0 && nPitch >= nWidth &&
        nGray >= 0 && nGray <= 255 &&
        static_cast<std::size_t>(nHeight) <=
            (std::numeric_limits<std::size_t>::max)() /
            static_cast<std::size_t>(nPitch);
}
}

namespace geometry
{
bool calculateCircumcircle(const Point& pt1, const Point& pt2,
    const Point& pt3, Circle& circleOut)
{
    circleOut = {};

    // Translate pt1 to the origin and normalize lengths to reduce cancellation.
    // Cast before subtraction so opposite extreme int coordinates also work.
    const double dDeltaAX = static_cast<double>(pt2.x) - pt1.x;
    const double dDeltaAY = static_cast<double>(pt2.y) - pt1.y;
    const double dDeltaBX = static_cast<double>(pt3.x) - pt1.x;
    const double dDeltaBY = static_cast<double>(pt3.y) - pt1.y;
    const double dCoordinateScale = (std::max)({std::abs(dDeltaAX),
        std::abs(dDeltaAY), std::abs(dDeltaBX), std::abs(dDeltaBY)});
    if (dCoordinateScale == 0.0)
    {
        return false;
    }
    const double dAX = dDeltaAX / dCoordinateScale;
    const double dAY = dDeltaAY / dCoordinateScale;
    const double dBX = dDeltaBX / dCoordinateScale;
    const double dBY = dDeltaBY / dCoordinateScale;
    const double dASquared = dAX * dAX + dAY * dAY;
    const double dBSquared = dBX * dBX + dBY * dBY;
    const double dCross = dAX * dBY - dAY * dBX;
    const double dScaleSquared = (std::max)(dASquared, dBSquared);
    constexpr double dRelativeEpsilon = 1.0e-9;

    // This dimensionless determinant threshold rejects nearly straight
    // triangles without imposing a screen bound or a maximum circle radius.
    if (dScaleSquared == 0.0 ||
        std::abs(dCross) <= dRelativeEpsilon * dScaleSquared)
    {
        return false;
    }

    const double dDenominator = 2.0 * dCross;
    const double dLocalCenterX =
        ((dASquared * dBY - dBSquared * dAY) / dDenominator) * dCoordinateScale;
    const double dLocalCenterY =
        ((dAX * dBSquared - dBX * dASquared) / dDenominator) * dCoordinateScale;

    const Circle circleCandidate{
        pt1.x + dLocalCenterX,
        pt1.y + dLocalCenterY,
        std::hypot(dLocalCenterX, dLocalCenterY)
    };
    if (!std::isfinite(circleCandidate.dCenterX) ||
        !std::isfinite(circleCandidate.dCenterY) ||
        !std::isfinite(circleCandidate.dRadius) || circleCandidate.dRadius <= 0.0)
    {
        return false;
    }

    circleOut = circleCandidate;
    return true;
}

bool isInCircle(int i, int j, int nCenterX, int nCenterY, int nRadius)
{
    if (nRadius <= 0)
    {
        return false;
    }
    const std::int64_t nDeltaX = static_cast<std::int64_t>(i) - nCenterX;
    const std::int64_t nDeltaY = static_cast<std::int64_t>(j) - nCenterY;
    if (nDeltaX < -static_cast<std::int64_t>(nRadius) || nDeltaX > nRadius ||
        nDeltaY < -static_cast<std::int64_t>(nRadius) || nDeltaY > nRadius)
    {
        return false;
    }

    // The lecture's circle equation, including the exact boundary. Widened
    // integer arithmetic keeps the result exact even near the int limits.
    return nDeltaX * nDeltaX + nDeltaY * nDeltaY <=
        static_cast<std::int64_t>(nRadius) * nRadius;
}

void drawCircle(unsigned char* fm, int nWidth, int nHeight, int nPitch,
    const Point& ptCenter, int nRadius, int nGray)
{
    if (!isValidBuffer(fm, nWidth, nHeight, nPitch, nGray) || nRadius <= 0)
    {
        return;
    }

    const int nCenterX = ptCenter.x;
    const int nCenterY = ptCenter.y;
    // Clip before looping. Widen before subtracting so arbitrary int centers
    // and radii are safe, including disks completely outside the drawing area.
    const std::int64_t nLeft = (std::max)(std::int64_t{0},
        static_cast<std::int64_t>(nCenterX) - nRadius);
    const std::int64_t nTop = (std::max)(std::int64_t{0},
        static_cast<std::int64_t>(nCenterY) - nRadius);
    const std::int64_t nRight = (std::min)(static_cast<std::int64_t>(nWidth) - 1,
        static_cast<std::int64_t>(nCenterX) + nRadius);
    const std::int64_t nBottom = (std::min)(static_cast<std::int64_t>(nHeight) - 1,
        static_cast<std::int64_t>(nCenterY) + nRadius);
    if (nLeft > nRight || nTop > nBottom)
    {
        return;
    }

    // size_t row/column indices keep j * nPitch safe for large valid buffers.
    for (std::size_t j = static_cast<std::size_t>(nTop);
        j <= static_cast<std::size_t>(nBottom); ++j)
    {
        for (std::size_t i = static_cast<std::size_t>(nLeft);
            i <= static_cast<std::size_t>(nRight); ++i)
        {
            if (isInCircle(static_cast<int>(i), static_cast<int>(j),
                nCenterX, nCenterY, nRadius))
            {
                fm[j * nPitch + i] = static_cast<unsigned char>(nGray);
            }
        }
    }
}

void drawCircleOutline(unsigned char* fm, int nWidth, int nHeight, int nPitch,
    const Circle& circle, int nThickness, int nGray)
{
    if (!isValidBuffer(fm, nWidth, nHeight, nPitch, nGray) || nThickness <= 0 ||
        !std::isfinite(circle.dCenterX) || !std::isfinite(circle.dCenterY) ||
        !std::isfinite(circle.dRadius) || circle.dRadius <= 0.0)
    {
        return;
    }

    const double dHalfThickness = nThickness * 0.5;
    // Preserve a hollow interior when the stroke exceeds the diameter by
    // shifting the extra thickness outward. Keep the true circumference.
    const double dMinimumInnerRadius = (std::min)(circle.dRadius, 1.0);
    const double dInnerOffset = (std::max)(-dHalfThickness,
        dMinimumInnerRadius - circle.dRadius);
    const double dOuterOffset = dInnerOffset + nThickness;
    const double dOriginDistance = std::hypot(circle.dCenterX, circle.dCenterY);
    const double dViewportDiagonal = std::hypot(nWidth - 1.0, nHeight - 1.0);
    const double dOriginOffset = dOriginDistance - circle.dRadius;

    // Reverse triangle inequality rejects fully invisible borders quickly.
    // Work is bounded by the viewport even for enormous off-screen circles.
    if (!std::isfinite(dOriginDistance) ||
        std::abs(dOriginOffset) > dViewportDiagonal + dOuterOffset)
    {
        return;
    }

    const bool bDistantCenter = dOriginDistance > 2.0 * dViewportDiagonal;
    const double dUnitX = bDistantCenter ? circle.dCenterX / dOriginDistance : 0.0;
    const double dUnitY = bDistantCenter ? circle.dCenterY / dOriginDistance : 0.0;

    for (std::size_t j = 0; j < static_cast<std::size_t>(nHeight); ++j)
    {
        for (std::size_t i = 0; i < static_cast<std::size_t>(nWidth); ++i)
        {
            const double dX = static_cast<double>(i);
            const double dY = static_cast<double>(j);
            const double dDist = std::hypot(dX - circle.dCenterX, dY - circle.dCenterY);
            double dRadialOffset = dDist - circle.dRadius;
            if (bDistantCenter)
            {
                // Rationalize distance - originDistance to preserve pixel
                // offsets for huge circles. Divide by originDistance first
                // to avoid squaring enormous centers or overflowing their sum.
                const double dNumerator =
                    (dX / dOriginDistance) * dX + (dY / dOriginDistance) * dY -
                    2.0 * dUnitX * dX - 2.0 * dUnitY * dY;
                dRadialOffset = dOriginOffset +
                    dNumerator / (dDist / dOriginDistance + 1.0);
            }

            if (dRadialOffset >= dInnerOffset && dRadialOffset <= dOuterOffset)
            {
                fm[j * nPitch + i] = static_cast<unsigned char>(nGray);
            }
        }
    }
}
}
