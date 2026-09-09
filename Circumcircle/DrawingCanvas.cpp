#include "DrawingCanvas.h"
#include <algorithm>

BEGIN_MESSAGE_MAP(CDrawingCanvas, CStatic)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_CAPTURECHANGED()
    ON_WM_CANCELMODE()
END_MESSAGE_MAP()

void CDrawingCanvas::OnPaint()
{
    CPaintDC dc(this);
    CRect area;
    GetClientRect(&area);
    const int width = area.Width();
    const int height = area.Height();
    if (width <= 0 || height <= 0)
        return;

    // Compose all pixels in memory, then present the completed frame once.
    m_pixels.assign(static_cast<size_t>(width) * height, 0x00FFFFFF);
    for (int i = 0; i < m_pointCount; ++i)
        geometry::DrawFilledPointCircle(m_pixels.data(), width, height,
            m_points[i], m_pointRadius, 0x00000000);
    if (m_hasCircumcircle)
        geometry::DrawCircleRaster(m_pixels.data(), width, height,
            m_circle, m_circleThickness, 0x00000000);

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    ::SetDIBitsToDevice(dc.GetSafeHdc(), 0, 0, width, height, 0, 0,
        0, height, m_pixels.data(), &info, DIB_RGB_COLORS);
}

BOOL CDrawingCanvas::OnEraseBkgnd(CDC*) { return TRUE; }

bool CDrawingCanvas::IsInsideDrawingArea(const CPoint& point) const
{
    CRect area;
    GetClientRect(&area);
    return area.PtInRect(point) != FALSE;
}

int CDrawingCanvas::HitTestPoint(const CPoint& point) const
{
    int nearest = -1;
    double nearestDistance = static_cast<double>(m_pointRadius) * m_pointRadius + 1;
    for (int i = 0; i < m_pointCount; ++i)
    {
        const double dx = static_cast<double>(point.x) - m_points[i].x;
        const double dy = static_cast<double>(point.y) - m_points[i].y;
        const double distance = dx * dx + dy * dy;
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = i;
        }
    }
    return nearest;
}

void CDrawingCanvas::OnLButtonDown(UINT, CPoint point)
{
    if (!IsInsideDrawingArea(point))
        return;
    const int hit = HitTestPoint(point);
    if (hit < 0 && m_pointCount == 3)
        return;
    if (onBeginInteraction)
        onBeginInteraction();
    SetFocus();
    if (hit >= 0)
    {
        m_dragPointIndex = hit;
        m_dragOffset = point - CPoint(m_points[hit].x, m_points[hit].y);
        SetCapture();
    }
    else
    {
        m_points[m_pointCount++] = {static_cast<int>(point.x), static_cast<int>(point.y)};
        RecalculateAndRedraw();
    }
}

void CDrawingCanvas::OnLButtonDblClk(UINT flags, CPoint point)
{
    OnLButtonDown(flags, point);
}

void CDrawingCanvas::MoveDragPoint(CPoint point)
{
    if (m_dragPointIndex < 0)
        return;
    CRect area;
    GetClientRect(&area);
    if (area.IsRectEmpty())
        return;
    point -= m_dragOffset;
    m_points[m_dragPointIndex] = {
        static_cast<int>(std::clamp(point.x, 0L, area.right - 1)),
        static_cast<int>(std::clamp(point.y, 0L, area.bottom - 1))};
    RecalculateAndRedraw();
}

void CDrawingCanvas::OnMouseMove(UINT flags, CPoint point)
{
    if (m_dragPointIndex < 0)
        return;
    if ((flags & MK_LBUTTON) == 0)
        EndDrag();
    else
        MoveDragPoint(point);
}

void CDrawingCanvas::OnLButtonUp(UINT, CPoint point)
{
    MoveDragPoint(point);
    EndDrag();
}

void CDrawingCanvas::EndDrag()
{
    m_dragPointIndex = -1;
    m_dragOffset = CPoint(0, 0);
    if (::GetCapture() == GetSafeHwnd())
        ::ReleaseCapture();
}

void CDrawingCanvas::OnCaptureChanged(CWnd* window)
{
    m_dragPointIndex = -1;
    m_dragOffset = CPoint(0, 0);
    CStatic::OnCaptureChanged(window);
}

void CDrawingCanvas::OnCancelMode()
{
    EndDrag();
    CStatic::OnCancelMode();
}

void CDrawingCanvas::RecalculateAndRedraw()
{
    m_circle = {};
    m_hasCircumcircle = m_pointCount == 3 && geometry::CalculateCircumcircle(
        m_points[0], m_points[1], m_points[2], m_circle);
    if (onChanged)
        onChanged();
    // RDW_UPDATENOW also repaints during each captured mouse move.
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
}

void CDrawingCanvas::SetPointRadius(int radius)
{
    m_pointRadius = radius;
    Invalidate(FALSE);
}

void CDrawingCanvas::SetCircleThickness(int thickness)
{
    m_circleThickness = thickness;
    Invalidate(FALSE);
}

void CDrawingCanvas::SetPoints(const std::array<geometry::Point, 3>& points)
{
    m_points = points;
    m_pointCount = 3;
    RecalculateAndRedraw();
}

void CDrawingCanvas::ResetAll()
{
    EndDrag();
    m_points = {};
    m_pointCount = 0;
    RecalculateAndRedraw();
}
