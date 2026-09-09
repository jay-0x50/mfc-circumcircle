#pragma once
#include <afxwin.h>
#include <afxdialogex.h>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include "DrawingCanvas.h"
#include "resource.h"

class CCircumcircleDlg final : public CDialogEx
{
public:
    CCircumcircleDlg() : CDialogEx(IDD_CIRCUMCIRCLE_DIALOG) {}
    ~CCircumcircleDlg() override;

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
    void OnCancel() override;
    afx_msg void OnDestroy();
    afx_msg void ResetAll();
    afx_msg void StartRandomMovement();
    afx_msg void OnRadiusChanged();
    afx_msg void OnThicknessChanged();
    afx_msg void OnRadiusKillFocus();
    afx_msg void OnThicknessKillFocus();
    afx_msg LRESULT OnRandomMove(WPARAM generation, LPARAM frameIndex);
    DECLARE_MESSAGE_MAP()

private:
    using Frame = std::array<geometry::Point, 3>;
    struct RandomRun
    {
        std::mutex mutex;
        std::condition_variable wake;
        bool cancelled = false;
        std::array<Frame, 10> frames{};
    };

    void UpdatePointCoordinateUI();
    void UpdateProgressUI();
    void StopRandomMovement();
    bool ReadPositiveInteger(int control, int maximum, int& value) const;
    void ApplyInputs();
    void NormalizeInput(int control, int maximum, int previous);

    CDrawingCanvas m_canvas;
    std::shared_ptr<RandomRun> m_randomRun;
    std::thread m_worker;
    UINT_PTR m_generation = 0;
    int m_randomMoves = 0;
    int m_pointRadius = 6;
    int m_circleThickness = 2;
    bool m_ready = false;
    bool m_updatingInputs = false;
};
