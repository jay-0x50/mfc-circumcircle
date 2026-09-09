#pragma once
#include <afxwin.h>
#include <array>
#include <functional>
#include <vector>
#include "CircleGeometry.h"

class CDrawingCanvas final : public CStatic
{
public:
    std::function<void()> onChanged;
    std::function<void()> onBeginInteraction;
    const std::array<geometry::Point, 3>& Points() const { return m_points; }
    int PointCount() const { return m_pointCount; }
    int PointRadius() const { return m_pointRadius; }
    bool HasCircumcircle() const { return m_hasCircumcircle; }
    const geometry::Circle& Circumcircle() const { return m_circle; }
    void SetPointRadius(int radius);
    void SetCircleThickness(int thickness);
    void SetPoints(const std::array<geometry::Point, 3>& points);
    void ResetAll();

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT flags, CPoint point);
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg void OnLButtonUp(UINT flags, CPoint point);
    afx_msg void OnCaptureChanged(CWnd* window);
    afx_msg void OnCancelMode();
    DECLARE_MESSAGE_MAP()

private:
    bool IsInsideDrawingArea(const CPoint& point) const;
    int HitTestPoint(const CPoint& point) const;
    void MoveDragPoint(CPoint point);
    void EndDrag();
    void RecalculateAndRedraw();

    std::array<geometry::Point, 3> m_points{};
    int m_pointCount = 0;
    int m_pointRadius = 6;
    int m_circleThickness = 2;
    int m_dragPointIndex = -1;
    CPoint m_dragOffset{0, 0};
    bool m_hasCircumcircle = false;
    geometry::Circle m_circle{};
    std::vector<std::uint32_t> m_pixels;
};
