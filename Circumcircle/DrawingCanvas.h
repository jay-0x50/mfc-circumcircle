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
    const std::array<geometry::Point, 3>& Points() const { return m_ptData; }
    int PointCount() const { return m_nDataCount; }
    int PointRadius() const { return m_nRadius; }
    bool HasCircumcircle() const { return m_bHasCircumcircle; }
    const geometry::Circle& Circumcircle() const { return m_circle; }
    void SetPointRadius(int nRadius);
    void SetCircleThickness(int nThickness);
    void SetPoints(const std::array<geometry::Point, 3>& points);
    void ResetAll();

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    afx_msg void OnCancelMode();
    DECLARE_MESSAGE_MAP()

private:
    BOOL validImgPos(int x, int y) const;
    int InitImage(int nWidth, int nHeight);
    int hitTestPoint(const CPoint& point) const;
    void moveDragPoint(CPoint point);
    void endDrag();
    void UpdateDisplay();

    std::array<geometry::Point, 3> m_ptData{};
    int m_nDataCount = 0;
    int m_nRadius = 6;
    int m_nThickness = 2;
    int m_nDragIndex = -1;
    CPoint m_ptDragOffset{0, 0};
    bool m_bHasCircumcircle = false;
    geometry::Circle m_circle{};
    std::vector<unsigned char> m_image;
};
