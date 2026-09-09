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
constexpr std::uint32_t background = 0x00FFFFFF;
constexpr std::uint32_t ink = 0x00123456;
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
    Check(geometry::CalculateCircumcircle({0, 0}, {6, 0}, {0, 8}, circle),
        "right triangle has a circumcircle");
    Check(Near(circle.centerX, 3) && Near(circle.centerY, 4) &&
        Near(circle.radius, 5), "known 3-4-5 circumcircle");

    Check(geometry::CalculateCircumcircle({0, 0}, {3, 0}, {0, 3}, circle),
        "fractional circumcenter accepted");
    Check(Near(circle.centerX, 1.5) && Near(circle.centerY, 1.5) &&
        Near(circle.radius, std::sqrt(4.5)), "fractional center is not rounded");

    const std::array<geometry::Point, 3> points{{{120, 150}, {300, 100}, {420, 350}}};
    std::array<int, 3> order{{0, 1, 2}};
    geometry::Circle original;
    Check(geometry::CalculateCircumcircle(points[0], points[1], points[2], original),
        "typical input has a circumcircle");
    do
    {
        Check(geometry::CalculateCircumcircle(points[order[0]], points[order[1]],
            points[order[2]], circle), "every triangle permutation accepted");
        Check(Near(circle.centerX, original.centerX) &&
            Near(circle.centerY, original.centerY) && Near(circle.radius, original.radius),
            "circumcircle independent of point order");
        for (const auto& point : points)
        {
            Check(Near(std::hypot(point.x - circle.centerX,
                point.y - circle.centerY), circle.radius),
                "circumference passes through every source point");
        }
    } while (std::next_permutation(order.begin(), order.end()));

    Check(geometry::CalculateCircumcircle({2000000000, 2000000000},
        {2000000006, 2000000000}, {2000000000, 2000000008}, circle),
        "large translated triangle accepted");
    Check(circle.centerX == 2000000003.0 && circle.centerY == 2000000004.0 &&
        Near(circle.radius, 5), "large translation preserves small triangle");

    const int intMin = (std::numeric_limits<int>::min)();
    const int intMax = (std::numeric_limits<int>::max)();
    Check(geometry::CalculateCircumcircle({intMin, intMin}, {intMax, intMin},
        {intMin, intMax}, circle), "extreme integer coordinates do not overflow");
    Check(Near(circle.centerX, -0.5) && Near(circle.centerY, -0.5),
        "extreme integer center remains fractional");

    for (const auto& triple : std::array<std::array<geometry::Point, 3>, 4>{{
        {{{0, 0}, {0, 0}, {1, 2}}},
        {{{1, 1}, {1, 1}, {1, 1}}},
        {{{1, 2}, {3, 4}, {5, 6}}},
        {{{0, 0}, {1000000000, 0}, {2000000000, 1}}}
    }})
    {
        circle = {10, 20, 30};
        Check(!geometry::CalculateCircumcircle(triple[0], triple[1], triple[2], circle),
            "duplicate, collinear, or nearly collinear points rejected");
        Check(circle.centerX == 0 && circle.centerY == 0 && circle.radius == 0,
            "failed circumcircle clears old output");
    }

    Check(geometry::CalculateCircumcircle({0, 0}, {1000, 0}, {500, 1}, circle),
        "large but valid circle is not clamped to the viewport");
    Check(circle.centerY < -100000 && circle.radius > 100000,
        "outside center and radius are preserved");
}

void TestFilledRaster()
{
    constexpr int width = 9;
    constexpr int height = 7;
    std::vector<std::uint32_t> buffer(width * height + 2, background);
    for (int cy = -5; cy <= height + 5; ++cy)
    {
        for (int cx = -5; cx <= width + 5; ++cx)
        {
            for (int radius = 1; radius <= 10; ++radius)
            {
                std::fill(buffer.begin(), buffer.end(), background);
                geometry::DrawFilledPointCircle(buffer.data() + 1, width, height,
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
    geometry::DrawFilledPointCircle(buffer.data() + 1, width, height,
        {largest, largest}, largest, ink);
    Check(std::all_of(buffer.begin(), buffer.end(),
        [](std::uint32_t value) { return value == background; }),
        "extreme integer point disk clips without integer overflow");
}

void TestOutlineRaster()
{
    constexpr int width = 9;
    constexpr int height = 7;
    std::vector<std::uint32_t> buffer(width * height + 2, background);
    for (double cy = -4; cy <= height + 4; cy += 0.5)
    {
        for (double cx = -4; cx <= width + 4; cx += 0.5)
        {
            for (double radius : {0.75, 1.0, 2.25, 4.0, 6.25, 10.0})
            {
                for (int thickness : {1, 2, 3, 8})
                {
                    std::fill(buffer.begin(), buffer.end(), background);
                    geometry::DrawCircleRaster(buffer.data() + 1, width, height,
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
        geometry::DrawCircleRaster(buffer.data() + 1, width, height,
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
    geometry::DrawCircleRaster(buffer.data() + 1, width, height, {4, 3, 2}, 1, ink);
    Check(buffer[1 + 3 * width + 4] == background, "outline leaves interior empty");

    for (double radius : {0.25, 0.75, 1.0, 2.0, 3.0})
    {
        std::fill(buffer.begin(), buffer.end(), background);
        geometry::DrawCircleRaster(buffer.data() + 1, width, height,
            {4, 3, radius}, 32, ink);
        Check(buffer[1 + 3 * width + 4] == background,
            "stroke thicker than diameter preserves a hollow center");
        Check(buffer[1 + 3 * width + 5] == ink,
            "thick stroke extends outward from its inner hole");
    }

    std::fill(buffer.begin(), buffer.end(), background);
    geometry::DrawCircleRaster(buffer.data() + 1, width, height,
        {4.25, 3.25, 2.0}, 32, ink);
    Check(buffer[1 + 3 * width + 4] == background &&
        buffer[1 + 3 * width + 5] == background &&
        buffer[1 + 4 * width + 4] == background,
        "thick stroke preserves fractional-center interior pixels");

    std::fill(buffer.begin(), buffer.end(), background);
    geometry::DrawCircleRaster(buffer.data() + 1, width, height, {1.0e300, 1.0e300, 1}, 2, ink);
    geometry::DrawCircleRaster(buffer.data() + 1, width, height, {0, 0, 1.0e300}, 2, ink);
    geometry::DrawCircleRaster(buffer.data() + 1, width, height,
        {1.7e308, 1.7e308, 1.7e308}, 2, ink);
    Check(std::all_of(buffer.begin(), buffer.end(),
        [](std::uint32_t value) { return value == background; }),
        "fully outside or enclosing huge circle draws no border");
}

void TestInvalidInputs()
{
    std::vector<std::uint32_t> buffer(25, background);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    for (const auto& circle : std::array<geometry::Circle, 6>{{
        {nan, 0, 2}, {0, nan, 2}, {0, 0, nan},
        {0, 0, infinity}, {0, 0, 0}, {0, 0, -1}
    }})
    {
        geometry::DrawCircleRaster(buffer.data(), 5, 5, circle, 2, ink);
    }
    geometry::DrawCircleRaster(nullptr, 5, 5, {2, 2, 2}, 2, ink);
    geometry::DrawCircleRaster(buffer.data(), -1, 5, {2, 2, 2}, 2, ink);
    geometry::DrawCircleRaster(buffer.data(), 5, 0, {2, 2, 2}, 2, ink);
    geometry::DrawCircleRaster(buffer.data(), 5, 5, {2, 2, 2}, 0, ink);
    geometry::DrawFilledPointCircle(nullptr, 5, 5, {2, 2}, 2, ink);
    geometry::DrawFilledPointCircle(buffer.data(), 0, 5, {2, 2}, 2, ink);
    geometry::DrawFilledPointCircle(buffer.data(), 5, -1, {2, 2}, 2, ink);
    geometry::DrawFilledPointCircle(buffer.data(), 5, 5, {2, 2}, -1, ink);
    geometry::DrawFilledPointCircle(buffer.data(), 5, 5, {2, 2}, 0, ink);
    Check(std::all_of(buffer.begin(), buffer.end(),
        [](std::uint32_t value) { return value == background; }),
        "invalid inputs are harmless and leave existing pixels unchanged");
}
}

int main()
{
    TestCircumcenters();
    TestFilledRaster();
    TestOutlineRaster();
    TestInvalidInputs();
    std::cout << "Geometry tests passed (" << checks << " checks).\n";
    return EXIT_SUCCESS;
}
