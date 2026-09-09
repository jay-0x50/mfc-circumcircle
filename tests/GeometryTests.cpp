#include "../Circumcircle/CircleGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
constexpr unsigned char background = 0xFF;
constexpr unsigned char ink = 0x35;
int checks = 0;

void Check(bool condition, const char* description)
{
    ++checks;
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool Near(double actual, double expected, double tolerance = 1.0e-10)
{
    return std::abs(actual - expected) <=
        tolerance * (std::max)(1.0, std::abs(expected));
}

void TestCircumcenters()
{
    geometry::Circle circle;
    Check(geometry::calculateCircumcircle({0, 0}, {6, 0}, {0, 8}, circle),
        "right triangle has a circumcircle");
    Check(Near(circle.dCenterX, 3) && Near(circle.dCenterY, 4) &&
        Near(circle.dRadius, 5), "known 3-4-5 circumcircle");

    Check(geometry::calculateCircumcircle({0, 0}, {3, 0}, {0, 3}, circle),
        "fractional circumcenter accepted");
    Check(Near(circle.dCenterX, 1.5) && Near(circle.dCenterY, 1.5) &&
        Near(circle.dRadius, std::sqrt(4.5)), "fractional center is not rounded");

    const std::array<geometry::Point, 3> points{{{120, 150}, {300, 100}, {420, 350}}};
    std::array<int, 3> order{{0, 1, 2}};
    geometry::Circle original;
    Check(geometry::calculateCircumcircle(points[0], points[1], points[2], original),
        "typical input has a circumcircle");
    do
    {
        Check(geometry::calculateCircumcircle(points[order[0]], points[order[1]],
            points[order[2]], circle), "every triangle permutation accepted");
        Check(Near(circle.dCenterX, original.dCenterX) &&
            Near(circle.dCenterY, original.dCenterY) && Near(circle.dRadius, original.dRadius),
            "circumcircle independent of point order");
        for (const auto& point : points)
        {
            Check(Near(std::hypot(point.x - circle.dCenterX,
                point.y - circle.dCenterY), circle.dRadius),
                "circumference passes through every source point");
        }
    } while (std::next_permutation(order.begin(), order.end()));

    Check(geometry::calculateCircumcircle({2000000000, 2000000000},
        {2000000006, 2000000000}, {2000000000, 2000000008}, circle),
        "large translated triangle accepted");
    Check(circle.dCenterX == 2000000003.0 && circle.dCenterY == 2000000004.0 &&
        Near(circle.dRadius, 5), "large translation preserves small triangle");

    const int intMin = (std::numeric_limits<int>::min)();
    const int intMax = (std::numeric_limits<int>::max)();
    Check(geometry::calculateCircumcircle({intMin, intMin}, {intMax, intMin},
        {intMin, intMax}, circle), "extreme integer coordinates do not overflow");
    Check(Near(circle.dCenterX, -0.5) && Near(circle.dCenterY, -0.5),
        "extreme integer center remains fractional");

    for (const auto& triple : std::array<std::array<geometry::Point, 3>, 4>{{
        {{{0, 0}, {0, 0}, {1, 2}}},
        {{{1, 1}, {1, 1}, {1, 1}}},
        {{{1, 2}, {3, 4}, {5, 6}}},
        {{{0, 0}, {1000000000, 0}, {2000000000, 1}}}
    }})
    {
        circle = {10, 20, 30};
        Check(!geometry::calculateCircumcircle(triple[0], triple[1], triple[2], circle),
            "duplicate, collinear, or nearly collinear points rejected");
        Check(circle.dCenterX == 0 && circle.dCenterY == 0 && circle.dRadius == 0,
            "failed circumcircle clears old output");
    }

    Check(geometry::calculateCircumcircle({0, 0}, {1000, 0}, {500, 1}, circle),
        "large but valid circle is not clamped to the viewport");
    Check(circle.dCenterY < -100000 && circle.dRadius > 100000,
        "outside center and radius are preserved");
}

void TestFilledRaster()
{
    constexpr int width = 9;
    constexpr int height = 7;
    std::vector<unsigned char> buffer(width * height + 2, background);
    for (int cy = -5; cy <= height + 5; ++cy)
    {
        for (int cx = -5; cx <= width + 5; ++cx)
        {
            for (int radius = 1; radius <= 10; ++radius)
            {
                std::fill(buffer.begin(), buffer.end(), background);
                geometry::drawCircle(buffer.data() + 1, width, height, width,
                    {cx, cy}, radius, ink);
                for (int y = 0; y < height; ++y)
                {
                    for (int x = 0; x < width; ++x)
                    {
                        const int dx = x - cx;
                        const int dy = y - cy;
                        const bool expected = dx * dx + dy * dy <= radius * radius;
                        Check(buffer[1 + y * width + x] == (expected ? ink : background),
                            "filled raster equals circle equation at every pixel");
                    }
                }
                Check(buffer.front() == background && buffer.back() == background,
                    "filled raster stays within buffer guards");
            }
        }
    }

    std::fill(buffer.begin(), buffer.end(), background);
    const int largest = (std::numeric_limits<int>::max)();
    geometry::drawCircle(buffer.data() + 1, width, height, width,
        {largest, largest}, largest, ink);
    Check(std::all_of(buffer.begin(), buffer.end(),
        [](unsigned char value) { return value == background; }),
        "extreme integer point disk clips without integer overflow");
}

void TestPointPredicate()
{
    Check(geometry::isInCircle(3, 4, 0, 0, 5),
        "isInCircle includes the exact circumference");
    Check(geometry::isInCircle(0, 0, 0, 0, 1),
        "isInCircle includes the center");
    Check(!geometry::isInCircle(4, 4, 0, 0, 5),
        "isInCircle rejects an outside pixel");
    Check(!geometry::isInCircle(0, 0, 0, 0, 0) &&
        !geometry::isInCircle(0, 0, 0, 0, -1),
        "isInCircle rejects nonpositive radius");
    const int largest = (std::numeric_limits<int>::max)();
    const int smallest = (std::numeric_limits<int>::min)();
    Check(geometry::isInCircle(largest, 0, 0, 0, largest),
        "isInCircle includes an extreme integer boundary exactly");
    Check(!geometry::isInCircle(largest, 1, 0, 0, largest),
        "isInCircle rejects one squared unit outside an extreme radius");
    Check(!geometry::isInCircle(smallest, smallest, largest, largest, largest),
        "isInCircle subtracts and squares extreme coordinates safely");
}

void TestPaddedGrayscaleBuffers()
{
    constexpr int width = 7;
    constexpr int height = 5;
    constexpr int pitch = 12;
    constexpr unsigned char guard = 0xA7;
    std::vector<unsigned char> buffer(pitch * height + 2, guard);
    for (int gray : {0, 1, 127, 254, 255})
    {
        for (bool outline : {false, true})
        {
            std::fill(buffer.begin(), buffer.end(), guard);
            if (outline)
                geometry::drawCircleOutline(buffer.data() + 1, width, height, pitch,
                    {3, 2, 2}, 1, gray);
            else
                geometry::drawCircle(buffer.data() + 1, width, height, pitch,
                    {3, 2}, 2, gray);
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < pitch; ++x)
                {
                    const int dx = x - 3;
                    const int dy = y - 2;
                    const int squared = dx * dx + dy * dy;
                    const bool inside = x < width && (outline
                        ? squared >= 2.25 && squared <= 6.25
                        : squared <= 4);
                    Check(buffer[1 + y * pitch + x] ==
                        (inside ? static_cast<unsigned char>(gray) : guard),
                        "padded buffer preserves each row and writes requested grayscale");
                }
            }
            Check(buffer.front() == guard && buffer.back() == guard,
                "grayscale raster leaves leading and trailing guards untouched");
        }
    }

    std::fill(buffer.begin(), buffer.end(), guard);
    for (int invalidPitch : {-1, 0, width - 1})
    {
        geometry::drawCircle(buffer.data() + 1, width, height, invalidPitch,
            {3, 2}, 2, 0);
        geometry::drawCircleOutline(buffer.data() + 1, width, height, invalidPitch,
            {3, 2, 2}, 1, 0);
    }
    for (int invalidGray : {(std::numeric_limits<int>::min)(), -1, 256,
        (std::numeric_limits<int>::max)()})
    {
        geometry::drawCircle(buffer.data() + 1, width, height, pitch,
            {3, 2}, 2, invalidGray);
        geometry::drawCircleOutline(buffer.data() + 1, width, height, pitch,
            {3, 2, 2}, 1, invalidGray);
    }
    Check(std::all_of(buffer.begin(), buffer.end(),
        [](unsigned char value) { return value == guard; }),
        "invalid row strides and grayscale values leave the entire buffer unchanged");
}

void TestOutlineRaster()
{
    constexpr int width = 9;
    constexpr int height = 7;
    std::vector<unsigned char> buffer(width * height + 2, background);
    for (double cy = -4; cy <= height + 4; cy += 0.5)
    {
        for (double cx = -4; cx <= width + 4; cx += 0.5)
        {
            for (double radius : {0.75, 1.0, 2.25, 4.0, 6.25, 10.0})
            {
                for (int thickness : {1, 2, 3, 8})
                {
                    std::fill(buffer.begin(), buffer.end(), background);
                    geometry::drawCircleOutline(buffer.data() + 1, width, height, width,
                        {cx, cy, radius}, thickness, ink);
                    const double inner = (std::max)((std::min)(radius, 1.0),
                        radius - thickness * 0.5);
                    const double outer = inner + thickness;
                    for (int y = 0; y < height; ++y)
                    {
                        for (int x = 0; x < width; ++x)
                        {
                            const double dx = x - cx;
                            const double dy = y - cy;
                            const double squared = dx * dx + dy * dy;
                            const bool expected = squared >= inner * inner &&
                                squared <= outer * outer;
                            Check(buffer[1 + y * width + x] == (expected ? ink : background),
                                "outline raster equals independent squared annulus reference");
                        }
                    }
                    Check(buffer.front() == background && buffer.back() == background,
                        "outline raster stays within buffer guards");
                }
            }
        }
    }

    for (double magnitude : {1.0e12, 1.0e20, 1.0e300})
    {
        std::fill(buffer.begin(), buffer.end(), background);
        geometry::drawCircleOutline(buffer.data() + 1, width, height, width,
            {magnitude, 0, magnitude}, 1, ink);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                Check(buffer[1 + y * width + x] == (x == 0 ? ink : background),
                    "huge tangent circle retains one-pixel border without filling viewport");
            }
        }
    }

    std::fill(buffer.begin(), buffer.end(), background);
    geometry::drawCircleOutline(buffer.data() + 1, width, height, width, {4, 3, 2}, 1, ink);
    Check(buffer[1 + 3 * width + 4] == background, "outline leaves interior empty");

    for (double radius : {0.25, 0.75, 1.0, 2.0, 3.0})
    {
        std::fill(buffer.begin(), buffer.end(), background);
        geometry::drawCircleOutline(buffer.data() + 1, width, height, width,
            {4, 3, radius}, 32, ink);
        Check(buffer[1 + 3 * width + 4] == background,
            "stroke thicker than diameter preserves a hollow center");
        Check(buffer[1 + 3 * width + 5] == ink,
            "thick stroke extends outward from its inner hole");
    }

    std::fill(buffer.begin(), buffer.end(), background);
    geometry::drawCircleOutline(buffer.data() + 1, width, height, width,
        {4.25, 3.25, 2.0}, 32, ink);
    Check(buffer[1 + 3 * width + 4] == background &&
        buffer[1 + 3 * width + 5] == background &&
        buffer[1 + 4 * width + 4] == background,
        "thick stroke preserves fractional-center interior pixels");

    std::fill(buffer.begin(), buffer.end(), background);
    geometry::drawCircleOutline(buffer.data() + 1, width, height, width, {1.0e300, 1.0e300, 1}, 2, ink);
    geometry::drawCircleOutline(buffer.data() + 1, width, height, width, {0, 0, 1.0e300}, 2, ink);
    geometry::drawCircleOutline(buffer.data() + 1, width, height, width,
        {1.7e308, 1.7e308, 1.7e308}, 2, ink);
    Check(std::all_of(buffer.begin(), buffer.end(),
        [](unsigned char value) { return value == background; }),
        "fully outside or enclosing huge circle draws no border");
}

void TestInvalidInputs()
{
    std::vector<unsigned char> buffer(25, background);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    for (const auto& circle : std::array<geometry::Circle, 6>{{
        {nan, 0, 2}, {0, nan, 2}, {0, 0, nan},
        {0, 0, infinity}, {0, 0, 0}, {0, 0, -1}
    }})
    {
        geometry::drawCircleOutline(buffer.data(), 5, 5, 5, circle, 2, ink);
    }
    geometry::drawCircleOutline(nullptr, 5, 5, 5, {2, 2, 2}, 2, ink);
    geometry::drawCircleOutline(buffer.data(), -1, 5, 5, {2, 2, 2}, 2, ink);
    geometry::drawCircleOutline(buffer.data(), 5, 0, 5, {2, 2, 2}, 2, ink);
    geometry::drawCircleOutline(buffer.data(), 5, 5, 5, {2, 2, 2}, 0, ink);
    geometry::drawCircle(nullptr, 5, 5, 5, {2, 2}, 2, ink);
    geometry::drawCircle(buffer.data(), 0, 5, 5, {2, 2}, 2, ink);
    geometry::drawCircle(buffer.data(), 5, -1, 5, {2, 2}, 2, ink);
    geometry::drawCircle(buffer.data(), 5, 5, 5, {2, 2}, -1, ink);
    geometry::drawCircle(buffer.data(), 5, 5, 5, {2, 2}, 0, ink);
    Check(std::all_of(buffer.begin(), buffer.end(),
        [](unsigned char value) { return value == background; }),
        "invalid inputs are harmless and leave existing pixels unchanged");
}
}

int main()
{
    TestCircumcenters();
    TestFilledRaster();
    TestPointPredicate();
    TestPaddedGrayscaleBuffers();
    TestOutlineRaster();
    TestInvalidInputs();
    std::cout << "Geometry tests passed (" << checks << " checks).\n";
    return EXIT_SUCCESS;
}
