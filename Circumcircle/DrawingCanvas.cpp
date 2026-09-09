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
    CRect rect;
    GetClientRect(&rect);
    const int nWidth = rect.Width();
    const int nHeight = rect.Height();
    const int nBpp = 8;
    if (nWidth <= 0 || nHeight <= 0)
        return;

    const int nPitch = InitImage(nWidth, nHeight);
    unsigned char* fm = m_image.data();
    const int nGray = 0;
    for (int i = 0; i < m_nDataCount; ++i)
        geometry::drawCircle(fm, nWidth, nHeight, nPitch,
            m_ptData[i], m_nRadius, nGray);
    if (m_bHasCircumcircle)
        geometry::drawCircleOutline(fm, nWidth, nHeight, nPitch,
            m_circle, m_nThickness, nGray);

    struct GrayBitmapInfo
    {
        BITMAPINFOHEADER bmiHeader;
        RGBQUAD rgb[256];
    } info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = nWidth;
    info.bmiHeader.biHeight = -nHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = nBpp;
    info.bmiHeader.biCompression = BI_RGB;
    info.bmiHeader.biClrUsed = 256;
    for (int i = 0; i < 256; ++i)
        info.rgb[i].rgbRed = info.rgb[i].rgbGreen = info.rgb[i].rgbBlue = static_cast<BYTE>(i);
    // The completed image is copied once; its circle pixels were calculated above.
    ::SetDIBitsToDevice(dc.GetSafeHdc(), 0, 0, nWidth, nHeight, 0, 0,
        0, nHeight, fm, reinterpret_cast<const BITMAPINFO*>(&info), DIB_RGB_COLORS);
}

int CDrawingCanvas::InitImage(int nWidth, int nHeight)
{
    // Like an 8-bit DIB, each row includes padding up to a four-byte boundary.
    const int nPitch = (nWidth + 3) & ~3;
    m_image.assign(static_cast<size_t>(nPitch) * nHeight, 0xff);
    return nPitch;
}

BOOL CDrawingCanvas::OnEraseBkgnd(CDC*) { return TRUE; }

BOOL CDrawingCanvas::validImgPos(int x, int y) const
{
    CRect rect;
    GetClientRect(&rect);
    return rect.PtInRect(CPoint(x, y));
}

int CDrawingCanvas::hitTestPoint(const CPoint& point) const
{
    int nIndex = -1;
    double dMinDist = static_cast<double>(m_nRadius) * m_nRadius + 1;
    for (int i = 0; i < m_nDataCount; ++i)
    {
        const double dX = static_cast<double>(point.x) - m_ptData[i].x;
        const double dY = static_cast<double>(point.y) - m_ptData[i].y;
        const double dDist = dX * dX + dY * dY;
        if (dDist < dMinDist)
        {
            dMinDist = dDist;
            nIndex = i;
        }
    }
    return nIndex;
}

void CDrawingCanvas::OnLButtonDown(UINT, CPoint point)
{
    if (!validImgPos(point.x, point.y))
        return;
    const int nIndex = hitTestPoint(point);
    if (nIndex < 0 && m_nDataCount == 3)
        return;
    if (onBeginInteraction)
        onBeginInteraction();
    SetFocus();
    if (nIndex >= 0)
    {
        m_nDragIndex = nIndex;
        m_ptDragOffset = point - CPoint(m_ptData[nIndex].x, m_ptData[nIndex].y);
        SetCapture();
    }
    else
    {
        m_ptData[m_nDataCount++] = {static_cast<int>(point.x), static_cast<int>(point.y)};
        UpdateDisplay();
    }
}

void CDrawingCanvas::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    OnLButtonDown(nFlags, point);
}

void CDrawingCanvas::moveDragPoint(CPoint point)
{
    if (m_nDragIndex < 0)
        return;
    CRect rect;
    GetClientRect(&rect);
    if (rect.IsRectEmpty())
        return;
    point -= m_ptDragOffset;
    m_ptData[m_nDragIndex] = {
        static_cast<int>(std::clamp(point.x, 0L, rect.right - 1)),
        static_cast<int>(std::clamp(point.y, 0L, rect.bottom - 1))};
    UpdateDisplay();
}

void CDrawingCanvas::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_nDragIndex < 0)
        return;
    if ((nFlags & MK_LBUTTON) == 0)
        endDrag();
    else
        moveDragPoint(point);
}

void CDrawingCanvas::OnLButtonUp(UINT, CPoint point)
{
    moveDragPoint(point);
    endDrag();
}

void CDrawingCanvas::endDrag()
{
    m_nDragIndex = -1;
    m_ptDragOffset = CPoint(0, 0);
    if (::GetCapture() == GetSafeHwnd())
        ::ReleaseCapture();
}

void CDrawingCanvas::OnCaptureChanged(CWnd* pWnd)
{
    m_nDragIndex = -1;
    m_ptDragOffset = CPoint(0, 0);
    CStatic::OnCaptureChanged(pWnd);
}

void CDrawingCanvas::OnCancelMode()
{
    endDrag();
    CStatic::OnCancelMode();
}

void CDrawingCanvas::UpdateDisplay()
{
    m_circle = {};
    m_bHasCircumcircle = m_nDataCount == 3 && geometry::calculateCircumcircle(
        m_ptData[0], m_ptData[1], m_ptData[2], m_circle);
    if (onChanged)
        onChanged();
    // RDW_UPDATENOW also repaints during each captured mouse move.
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
}

void CDrawingCanvas::SetPointRadius(int nRadius)
{
    m_nRadius = nRadius;
    Invalidate(FALSE);
}

void CDrawingCanvas::SetCircleThickness(int nThickness)
{
    m_nThickness = nThickness;
    Invalidate(FALSE);
}

void CDrawingCanvas::SetPoints(const std::array<geometry::Point, 3>& points)
{
    m_ptData = points;
    m_nDataCount = 3;
    UpdateDisplay();
}

void CDrawingCanvas::ResetAll()
{
    endDrag();
    m_ptData = {};
    m_nDataCount = 0;
    UpdateDisplay();
}
