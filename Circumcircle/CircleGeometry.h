#pragma once

namespace geometry
{
struct Point
{
    int x = 0;
    int y = 0;
};

struct Circle
{
    double dCenterX = 0.0;
    double dCenterY = 0.0;
    double dRadius = 0.0;
};

// On failure, circleOut is reset to an empty circle.
bool calculateCircumcircle(const Point& pt1, const Point& pt2,
    const Point& pt3, Circle& circleOut);

// Integer coordinates denote pixel centers. The boundary is included (<=).
bool isInCircle(int i, int j, int nCenterX, int nCenterY, int nRadius);

// Top-down 8-bit grayscale buffer: 0 = black, 255 = white. nPitch is the
// positive row stride in bytes and must be at least nWidth. Padding is untouched.
void drawCircle(unsigned char* fm, int nWidth, int nHeight, int nPitch,
    const Point& ptCenter, int nRadius, int nGray);

// The stroke is centered on the mathematical circumference where possible.
// A hollow inner radius of min(radius, 1 pixel) is preserved; excess thickness
// extends outward. The center and radius are never rounded or moved to fit.
void drawCircleOutline(unsigned char* fm, int nWidth, int nHeight, int nPitch,
    const Circle& circle, int nThickness, int nGray);
}
