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
    afx_msg void OnBnClickedBtnReset();
    afx_msg void OnBnClickedBtnRandom();
    afx_msg void OnEnChangeEditRadius();
    afx_msg void OnEnChangeEditThickness();
    afx_msg void OnEnKillfocusEditRadius();
    afx_msg void OnEnKillfocusEditThickness();
    afx_msg LRESULT OnRandomMove(WPARAM nRunId, LPARAM nFrameIndex);
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
    void ResetAll();
    void StartRandomMovement();
    void StopRandomMovement();
    static void threadProcess(std::shared_ptr<RandomRun> pRun,
        HWND hWnd, UINT_PTR nRunId, CRect rect);
    bool ReadPositiveInteger(int nID, int nMax, int& nValue) const;
    void ApplyInputs();
    void NormalizeInput(int nID, int nMax, int nPrevious);

    CDrawingCanvas m_canvas;
    std::shared_ptr<RandomRun> m_pRandomRun;
    std::thread m_thread;
    UINT_PTR m_nRunId = 0;
    int m_nMoveCount = 0;
    int m_nRadius = 6;
    int m_nThickness = 2;
    bool m_bReady = false;
    bool m_bUpdatingInputs = false;
};
