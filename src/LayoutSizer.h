#ifndef LAYOUT_SIZER_H__
#define LAYOUT_SIZER_H__

#include "StrUtil.h"

class CUISizer {
public:
	virtual void GetIdealSize(int& dx, int &dy) = 0;

	SIZE GetIdealSize2() {
		int dx, dy;
		GetIdealSize(dx, dy);
		SIZE s = {dx, dy};
		return s;
	}

	int GetIdealSizeDx() {
		int dx, dy;
		GetIdealSize(dx, dy);
		return dx;
	}

	int GetIdealSizeDy() {
		int dx, dy;
		GetIdealSize(dx, dy);
		return dy;
	}
};

// TODO: should this be a member of CUISizer?
static void CalcFixedPositionBottomRight(int clientDx, int clientDy, int rightMargin, int bottomMargin, CUISizer* sizer, RECT& posOut)
{
	int dx, dy;
	sizer->GetIdealSize(dx, dy);
	int x = clientDx - dx - rightMargin;
	int y = clientDy - dy - bottomMargin;
	posOut.left = x;
	posOut.top = y;
	posOut.right = x + dx;
	posOut.bottom = y + dy;
}

class CUITextSizer : public CUISizer {
protected:
	CWindow m_window;
	HFONT m_font;
	const TCHAR *m_txt;
	int m_txtLen;

public:

	CUITextSizer() {
		m_txt = NULL;
		m_font = NULL;
	}

	CUITextSizer(CWindow win) {
		m_window = win;
		m_txt = NULL;
		m_font = NULL;
	}

    void SetWindow(CWindow win)
	{
		m_window = win;
	}

	void FreeText()
	{
		free((void*)m_txt);
		m_txt = NULL;
	}

	void SetText(const TCHAR *txt)
	{
		FreeText();
		m_txt = tstrdup(txt);
		if (m_txt)
			m_txtLen = tstrlen(m_txt);
		else
			m_txtLen = 0;
	}

	void SetFont(HFONT font)
	{
		m_font = font;
	}

	~CUITextSizer() {
		FreeText();
	}

	virtual void GetIdealSize(int& dx, int &dy)
	{
		if (!m_txt) {
			dx = 0;
			dy = 0;
			return;
		}

		CDC dc = m_window.GetDC();
		HFONT prevFont;
		if (m_font)
			prevFont = dc.SelectFont(m_font);
		else
			prevFont = dc.SelectFont(m_window.GetFont());
		SIZE size;
		dc.GetTextExtent(m_txt, m_txtLen, &size);
		dx = size.cx;
		dy = size.cy;
		dc.SelectFont(prevFont);
	}
};

class CUICheckBoxButtonSizer : public CUITextSizer {
	CButton m_button;
	static const int CHECKBOX_DX = 20; // to account for the checkbox
public:
	CUICheckBoxButtonSizer(CButton b) {
		m_button = b;
		SetWindow(b);
	}

	virtual void GetIdealSize(int& dxOut, int& dyOut)
	{
		TCHAR *txt = MyGetWindowText(m_button);
		if (!txt)
			txt = tstrdup(_T(" "));
		SetText(txt);
		free(txt);
		CUITextSizer::GetIdealSize(dxOut, dyOut);
		dxOut += CHECKBOX_DX;
		/*SIZE s;
		m_button.GetIdealSize(&s);
		dxOut = s.cx;
		dyOut = s.cy;*/
	}
};

class CUIButtonSizer : public CUISizer {
	CButton m_button;
public:
	CUIButtonSizer(CButton b) {
		m_button = b;
	}

	virtual void GetIdealSize(int& dxOut, int& dyOut)
	{
		CDC dc = m_button.GetDC();
		TCHAR *txt = MyGetWindowText(m_button);
		if (!txt)
			txt = tstrdup(_T(" "));
		int slen = tstrlen(txt);
		SIZE size;
		dc.GetTextExtent(txt, slen, &size);
		free(txt);
		dxOut = size.cx + 32;
		dyOut = size.cy + 8;
	}
};

class CUIStaticSizer : public CUITextSizer {
public:
	CUIStaticSizer() {}

	CUIStaticSizer(CStatic win) :
	  CUITextSizer(win)
	{
	}

	virtual void GetIdealSize(int& dxOut, int& dyOut)
	{
		TCHAR *s = MyGetWindowText(m_window);
		SetText(s);
		free(s);
		CUITextSizer::GetIdealSize(dxOut, dyOut);
	}
};

class CUILinkSizer : public CUITextSizer {
public:
	CUILinkSizer() {}

	CUILinkSizer(CLinkCtrl win) :
	  CUITextSizer(win)
	{
	}

	virtual void GetIdealSize(int& dxOut, int& dyOut)
	{
		TCHAR *s = MyGetWindowText(m_window);
		TStrRemoveAnchorTags(s);
		SetText(s);
		free(s);
		CUITextSizer::GetIdealSize(dxOut, dyOut);
	}
};

#endif
