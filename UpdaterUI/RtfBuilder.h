#ifndef RTF_BUILDER_H__
#define RTF_BUILDER_H__

enum RtfLinkId {
	  LINK_CHANGE_ACCOUNT
	, LINK_CHANGE_NETWORK
	, LINK_SELECT_NETWORK
	, LINK_CONFIGURE_NETWORKS
	, LINK_SETUP_OPENDNS
	, LINK_TOGGLE_DEV_PRODUCTION
	, LINK_SEND_IP_UPDATE
	, LINK_CRASH_ME
	, LINK_DOWNLOAD_NEW_VERSION
	, LINK_LEARN_MORE_IP_MISMATCH
	, LINK_LEARN_MORE_IP_TAKEN
};

class RtfLinkInfo {
public:
	RtfLinkInfo *next;
	RtfLinkId id;
	LONG start, end;

	RtfLinkInfo(RtfLinkId id, LONG start, LONG end)
	{
		this->next = NULL;
		this->id = id;
		this->start = start;
		this->end = end;
	}
};

static inline void RtfLinkInfoFreeList(RtfLinkInfo *head)
{
	while (head) {
		RtfLinkInfo *next = head->next;
		delete head;
		head = next;
	}
}

class RtfTextInfo
{
	int m_currPos;
	int m_styleNesting;
public:
	CString text;
	RtfLinkInfo *firstLink;

	enum RtfCol {
		ColFirst = 1
		,ColRed = ColFirst
		,ColAfterLast
	};

	RtfTextInfo() {
		firstLink = NULL;
		m_currPos = 0;
		m_styleNesting = 0;
	}

	void FreeLinks() {
		RtfLinkInfoFreeList(firstLink);
		firstLink = NULL;
	}

	~RtfTextInfo() {
		FreeLinks();
	}

	RtfTextInfo& AddCol(int r, int g, int b)
	{
		CString colTxt;
		colTxt.Format(_T("\\red%d\\green%d\\blue%d;"), r, g, b);
		text += colTxt;
		return *this;
	}

	void Init(const TCHAR *defaultFontName, int fontSize) {
		m_currPos = 0;
		m_styleNesting = 0;
		FreeLinks();

		text = _T("{\\rtf1");

		// define font table
		text += "\\deff0{\\fonttbl{\\f0 ";
		text += defaultFontName;
		text += ";}";
		text += "}";

		// define color table
		text += "{\\colortbl;";
		//AddCol(255,0,0).AddCol(0,0,0).AddCol(255,255,255);
		AddCol(0xf7,0x0c,0x08).AddCol(0,0,0).AddCol(255,255,255);
		text += "}";
		StartFontSize(fontSize * 2);
	}

	void End() {
		EndFontSize();
		text += "}";
		assert(0 == m_styleNesting);
		while (m_styleNesting > 0) {
			text += "}";
			--m_styleNesting;
		}
	}

	void StartBoldStyle()
	{
		++m_styleNesting;
		text += "{\\b ";
	}

	void StartFontSize(int n)
	{
		CString s;
		s.Format(_T("{\\fs%d "), n);
		text += s;
		++m_styleNesting;
	}

	void EndFontSize()
	{
		EndStyle();
	}

	void StartFgCol(RtfCol col)
	{
		assert((col >= ColFirst) && (col < ColAfterLast));
		++m_styleNesting;
		CString s;
		s.Format(_T("{\\cf%d "), col);
		text += s;
	}

	void EndCol()
	{
		EndStyle();
	}

	void EndStyle()
	{
		assert(m_styleNesting > 0);
		text += "}";
		--m_styleNesting;
	}

	void AddTxt(const char *s) {
		text += s;
		m_currPos += strlen(s);
	}

#ifdef UNICODE
	void AddTxt(const TCHAR *s) {
		text += s;
		m_currPos += tstrlen(s);
	}
#endif

	void AddPara() {
		text += "\\par ";
		m_currPos += 1;
	}

#ifdef UNICODE
	void AddLink(const TCHAR *s, RtfLinkId id)
	{
		int linkStart = m_currPos;
		AddTxt(s);
		int linkEnd = m_currPos;
		RtfLinkInfo *rli = new RtfLinkInfo(id, linkStart, linkEnd);
		rli->next = firstLink;
		firstLink = rli;
	}
#endif

	void AddLink(const char *s, RtfLinkId id)
	{
		int linkStart = m_currPos;
		AddTxt(s);
		int linkEnd = m_currPos;
		RtfLinkInfo *rli = new RtfLinkInfo(id, linkStart, linkEnd);
		rli->next = firstLink;
		firstLink = rli;
	}

	bool FindLinkRange(RtfLinkId id, LONG& start, LONG& end)
	{
		RtfLinkInfo *curr = firstLink;
		while (curr) {
			if (curr->id == id) {
				start = curr->start;
				end = curr->end;
				return true;
			}
			curr = curr->next;
		}
		return false;
	}

	bool FindLinkFromRange(LONG start, LONG end, RtfLinkId& id)
	{
		RtfLinkInfo *curr = firstLink;
		while (curr) {
			if ((curr->start == start) && (curr->end == end)) {
				id = curr->id;
				return true;
			}
			curr = curr->next;
		}
		return false;
	}
};

#endif
