// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../include/fsdk_define.h"
#include "../include/fpdfxfa/fpdfxfa_doc.h"
#include "../include/fpdfxfa/fpdfxfa_util.h"
#include "../include/fsdk_mgr.h"
#include "../include/formfiller/FFL_FormFiller.h"
#include "../include/fsdk_annothandler.h"


CPDFSDK_AnnotHandlerMgr::CPDFSDK_AnnotHandlerMgr(CPDFDoc_Environment* pApp)
{
	m_pApp = pApp;

	CPDFSDK_BFAnnotHandler* pHandler = new CPDFSDK_BFAnnotHandler(m_pApp);
	pHandler->SetFormFiller(m_pApp->GetIFormFiller());
	RegisterAnnotHandler(pHandler);

	CPDFSDK_XFAAnnotHandler* pXFAAnnotHandler = new CPDFSDK_XFAAnnotHandler(m_pApp);
	RegisterAnnotHandler(pXFAAnnotHandler);
}

CPDFSDK_AnnotHandlerMgr::~CPDFSDK_AnnotHandlerMgr()
{
	for(int i=0; i<m_Handlers.GetSize(); i++)
	{
		IPDFSDK_AnnotHandler* pHandler = m_Handlers.GetAt(i);
		delete pHandler;
	}
	m_Handlers.RemoveAll();
	m_mapType2Handler.RemoveAll();
}

void	CPDFSDK_AnnotHandlerMgr::RegisterAnnotHandler(IPDFSDK_AnnotHandler* pAnnotHandler)
{
	ASSERT(pAnnotHandler != NULL);

	ASSERT(GetAnnotHandler(pAnnotHandler->GetType()) == NULL);

	m_Handlers.Add(pAnnotHandler);
	m_mapType2Handler.SetAt(pAnnotHandler->GetType(), (void*)pAnnotHandler);
}

void CPDFSDK_AnnotHandlerMgr::UnRegisterAnnotHandler(IPDFSDK_AnnotHandler* pAnnotHandler)
{
	ASSERT(pAnnotHandler != NULL);

	m_mapType2Handler.RemoveKey(pAnnotHandler->GetType());

	for (int i=0, sz=m_Handlers.GetSize(); i<sz; i++)
	{
		if (m_Handlers.GetAt(i) == pAnnotHandler)
		{
			m_Handlers.RemoveAt(i);
			break;
		}
	}
}

CPDFSDK_Annot* CPDFSDK_AnnotHandlerMgr::NewAnnot(CPDF_Annot * pAnnot, CPDFSDK_PageView *pPageView)
{
	ASSERT(pAnnot != NULL);
	ASSERT(pPageView != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot->GetSubType()))
	{
		return pAnnotHandler->NewAnnot(pAnnot, pPageView);
	}

	return new CPDFSDK_BAAnnot(pAnnot, pPageView);
}

CPDFSDK_Annot* CPDFSDK_AnnotHandlerMgr::NewAnnot(IXFA_Widget* pAnnot, CPDFSDK_PageView* pPageView)
{
	ASSERT(pAnnot != NULL);
	ASSERT(pPageView != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(FSDK_XFAWIDGET_TYPENAME))
	{
		return pAnnotHandler->NewAnnot(pAnnot, pPageView);
	}

	return NULL;
}

void CPDFSDK_AnnotHandlerMgr::ReleaseAnnot(CPDFSDK_Annot* pAnnot)
{
	ASSERT(pAnnot != NULL);

	pAnnot->GetPDFPage();

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		pAnnotHandler->OnRelease(pAnnot);
		pAnnotHandler->ReleaseAnnot(pAnnot);
	}
	else
	{
		delete (CPDFSDK_Annot*)pAnnot;
	}
}

void CPDFSDK_AnnotHandlerMgr::Annot_OnCreate(CPDFSDK_Annot* pAnnot)
{
	ASSERT(pAnnot != NULL);

	CPDF_Annot* pPDFAnnot = pAnnot->GetPDFAnnot();

	CPDFSDK_DateTime curTime;
	pPDFAnnot->GetAnnotDict()->SetAtString("M", curTime.ToPDFDateTimeString());
	pPDFAnnot->GetAnnotDict()->SetAtNumber("F", 0);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		pAnnotHandler->OnCreate(pAnnot);
	}
}

void CPDFSDK_AnnotHandlerMgr::Annot_OnLoad(CPDFSDK_Annot* pAnnot)
{
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		pAnnotHandler->OnLoad(pAnnot);
	}
}

IPDFSDK_AnnotHandler* CPDFSDK_AnnotHandlerMgr::GetAnnotHandler(CPDFSDK_Annot* pAnnot) const
{
	ASSERT(pAnnot != NULL);

	CPDF_Annot* pPDFAnnot = pAnnot->GetPDFAnnot();
	if (pPDFAnnot)
		return GetAnnotHandler(pPDFAnnot->GetSubType());
	else if (pAnnot->GetXFAWidget())
		return GetAnnotHandler(FSDK_XFAWIDGET_TYPENAME);
	return NULL;
}

IPDFSDK_AnnotHandler* CPDFSDK_AnnotHandlerMgr::GetAnnotHandler(const CFX_ByteString& sType) const
{
	void* pRet = NULL;
	m_mapType2Handler.Lookup(sType, pRet);
	return (IPDFSDK_AnnotHandler*)pRet;
}

void CPDFSDK_AnnotHandlerMgr::Annot_OnDraw(CPDFSDK_PageView* pPageView, CPDFSDK_Annot* pAnnot, CFX_RenderDevice* pDevice, CPDF_Matrix* pUser2Device,FX_DWORD dwFlags)
{
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		pAnnotHandler->OnDraw(pPageView, pAnnot, pDevice, pUser2Device, dwFlags);
	}
	else
	{
		if (!pAnnot->IsXFAField())
			((CPDFSDK_BAAnnot*)pAnnot)->DrawAppearance(pDevice, pUser2Device, CPDF_Annot::Normal, NULL);
	}
}


FX_BOOL CPDFSDK_AnnotHandlerMgr::Annot_OnLButtonDown(CPDFSDK_PageView * pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		return pAnnotHandler->OnLButtonDown(pPageView, pAnnot, nFlags, point);
	}
	return FALSE;
}
FX_BOOL CPDFSDK_AnnotHandlerMgr::Annot_OnLButtonUp(CPDFSDK_PageView * pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
 {
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		return pAnnotHandler->OnLButtonUp(pPageView, pAnnot, nFlags, point);
	}
	return FALSE;
 }
FX_BOOL CPDFSDK_AnnotHandlerMgr::Annot_OnLButtonDblClk(CPDFSDK_PageView * pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		return pAnnotHandler->OnLButtonDblClk(pPageView, pAnnot, nFlags, point);
	}
	return FALSE;
}
FX_BOOL CPDFSDK_AnnotHandlerMgr::Annot_OnMouseMove(CPDFSDK_PageView * pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		return pAnnotHandler->OnMouseMove(pPageView, pAnnot, nFlags, point);
	}
	return FALSE;
}
FX_BOOL CPDFSDK_AnnotHandlerMgr::Annot_OnMouseWheel(CPDFSDK_PageView * pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, short zDelta, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		return pAnnotHandler->OnMouseWheel(pPageView, pAnnot,nFlags,zDelta, point);
	}
	return FALSE;
}
FX_BOOL CPDFSDK_AnnotHandlerMgr::Annot_OnRButtonDown(CPDFSDK_PageView * pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		return pAnnotHandler->OnRButtonDown(pPageView, pAnnot, nFlags, point);
	}
	return FALSE;
}
FX_BOOL CPDFSDK_AnnotHandlerMgr::Annot_OnRButtonUp(CPDFSDK_PageView * pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		return pAnnotHandler->OnRButtonUp(pPageView, pAnnot, nFlags, point);
	}
	return FALSE;
}

void CPDFSDK_AnnotHandlerMgr::Annot_OnMouseEnter(CPDFSDK_PageView * pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlag)
{
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		pAnnotHandler->OnMouseEnter(pPageView, pAnnot, nFlag);
	}
	return ;
}

void CPDFSDK_AnnotHandlerMgr::Annot_OnMouseExit(CPDFSDK_PageView * pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlag)
{
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		pAnnotHandler->OnMouseExit(pPageView, pAnnot, nFlag);
	}
	return;
}

FX_BOOL CPDFSDK_AnnotHandlerMgr::Annot_OnChar(CPDFSDK_Annot* pAnnot, FX_DWORD nChar, FX_DWORD nFlags)
{

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		return pAnnotHandler->OnChar(pAnnot,nChar, nFlags);
	}
	return FALSE;

}

FX_BOOL			CPDFSDK_AnnotHandlerMgr::Annot_OnKeyDown(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag)
{

	if (!m_pApp->FFI_IsCTRLKeyDown(nFlag) && !m_pApp->FFI_IsALTKeyDown(nFlag))
	{
		CPDFSDK_PageView* pPage = pAnnot->GetPageView();
		CPDFSDK_Annot* pFocusAnnot = pPage->GetFocusAnnot();
		if (pFocusAnnot && (nKeyCode == FWL_VKEY_Tab))
		{
			CPDFSDK_Annot* pNext = GetNextAnnot(pFocusAnnot, !m_pApp->FFI_IsSHIFTKeyDown(nFlag));

			if(pNext && pNext != pFocusAnnot)
			{
				CPDFSDK_Document* pDocument = pPage->GetSDKDocument();
				pDocument->SetFocusAnnot(pNext);
				return TRUE;
			}
		}
	}

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		return pAnnotHandler->OnKeyDown(pAnnot,nKeyCode, nFlag);
	}
	return FALSE;
}
FX_BOOL			CPDFSDK_AnnotHandlerMgr::Annot_OnKeyUp(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag)
{
	return FALSE;
}

FX_BOOL			CPDFSDK_AnnotHandlerMgr::Annot_OnSetFocus(CPDFSDK_Annot* pAnnot, FX_DWORD nFlag)
{
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		if (pAnnotHandler->OnSetFocus(pAnnot, nFlag))
		{
			CPDFSDK_PageView* pPage = pAnnot->GetPageView();
			ASSERT(pPage != NULL);

			pPage->GetSDKDocument();
	//		pDocument->SetTopmostAnnot(pAnnot);

			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	return FALSE;
}

FX_BOOL			CPDFSDK_AnnotHandlerMgr::Annot_OnKillFocus(CPDFSDK_Annot* pAnnot, FX_DWORD nFlag)
{
	ASSERT(pAnnot != NULL);

	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		if (pAnnotHandler->OnKillFocus(pAnnot, nFlag))
		{
			return TRUE;
		}
		else
			return FALSE;
	}

	return FALSE;
}

FX_BOOL CPDFSDK_AnnotHandlerMgr::Annot_OnChangeFocus(CPDFSDK_Annot* pSetAnnot,CPDFSDK_Annot* pKillAnnot)
{
	FX_BOOL bXFA = (pSetAnnot && pSetAnnot->GetXFAWidget()) ||
		(pKillAnnot && pKillAnnot->GetXFAWidget());

	if (bXFA)
	{
		if (IPDFSDK_AnnotHandler* pXFAAnnotHandler = GetAnnotHandler(FSDK_XFAWIDGET_TYPENAME))
			return pXFAAnnotHandler->OnXFAChangedFocus(pKillAnnot, pSetAnnot);
	}

	return TRUE;
}

CPDF_Rect	CPDFSDK_AnnotHandlerMgr::Annot_OnGetViewBBox(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot)
{
	ASSERT(pAnnot);
	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		return pAnnotHandler->GetViewBBox(pPageView, pAnnot);
	}
	return pAnnot->GetRect();
}

FX_BOOL	CPDFSDK_AnnotHandlerMgr::Annot_OnHitTest(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, const CPDF_Point& point)
{
	ASSERT(pAnnot);
	if (IPDFSDK_AnnotHandler* pAnnotHandler = GetAnnotHandler(pAnnot))
	{
		if(pAnnotHandler->CanAnswer(pAnnot))
			return pAnnotHandler->HitTest(pPageView, pAnnot, point);
	}
	return FALSE;
}

CPDFSDK_Annot*	CPDFSDK_AnnotHandlerMgr::GetNextAnnot(CPDFSDK_Annot* pSDKAnnot,FX_BOOL bNext)
{
	CPDFSDK_PageView* pPageView =  pSDKAnnot->GetPageView();
	CPDFXFA_Page* pPage = pPageView->GetPDFXFAPage();
	if (pPage == NULL)
		return NULL;
	if (pPage->GetPDFPage()) { // for pdf annots.
		CBA_AnnotIterator ai(pSDKAnnot->GetPageView(), pSDKAnnot->GetType(), "");
		CPDFSDK_Annot* pNext = bNext ?
		ai.GetNextAnnot(pSDKAnnot) :
		ai.GetPrevAnnot(pSDKAnnot);
		return pNext;
	}
	// for xfa annots
	IXFA_WidgetIterator* pWidgetIterator = pPage->GetXFAPageView()->CreateWidgetIterator(XFA_TRAVERSEWAY_Tranvalse, XFA_WIDGETFILTER_Visible | XFA_WIDGETFILTER_Viewable | XFA_WIDGETFILTER_Field);
	if (pWidgetIterator == NULL)
		return  NULL;
	if (pWidgetIterator->GetCurrentWidget() != pSDKAnnot->GetXFAWidget())
		pWidgetIterator->SetCurrentWidget(pSDKAnnot->GetXFAWidget());
	IXFA_Widget* hNextFocus = NULL;
	hNextFocus = bNext?pWidgetIterator->MoveToNext():pWidgetIterator->MoveToPrevious();
	if (hNextFocus == NULL && pSDKAnnot != NULL)
		hNextFocus = pWidgetIterator->MoveToFirst();

	pWidgetIterator->Release();
	return pPageView->GetAnnotByXFAWidget(hNextFocus);
}

FX_BOOL CPDFSDK_BFAnnotHandler::CanAnswer(CPDFSDK_Annot* pAnnot)
{
	ASSERT(pAnnot);
	ASSERT(pAnnot->GetType() == "Widget");
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		CPDFSDK_Widget* pWidget = (CPDFSDK_Widget*)pAnnot;
		if (!pWidget->IsVisible()) return FALSE;

		int nFieldFlags = pWidget->GetFieldFlags();
		if ((nFieldFlags & FIELDFLAG_READONLY) == FIELDFLAG_READONLY) return FALSE;
		if (pWidget->GetFieldType() == FIELDTYPE_PUSHBUTTON)
			return TRUE;
		else
		{
			CPDF_Page* pPage = pWidget->GetPDFPage();
			ASSERT(pPage != NULL);

			CPDF_Document* pDocument = pPage->m_pDocument;
			ASSERT(pDocument != NULL);

			FX_DWORD dwPermissions = pDocument->GetUserPermissions();
			return (dwPermissions&FPDFPERM_FILL_FORM) ||
				(dwPermissions&FPDFPERM_ANNOT_FORM) ||
 			(dwPermissions&FPDFPERM_ANNOT_FORM);
		}
	}

	return FALSE;
}

CPDFSDK_Annot*		CPDFSDK_BFAnnotHandler::NewAnnot(CPDF_Annot* pAnnot, CPDFSDK_PageView* pPage)
{
	ASSERT(pPage != NULL);
	pPage->GetPDFDocument();

	CPDFSDK_Document* pSDKDoc  = m_pApp->GetCurrentDoc();
	ASSERT(pSDKDoc);
	CPDFSDK_InterForm* pInterForm = (CPDFSDK_InterForm*)pSDKDoc->GetInterForm();
	ASSERT(pInterForm != NULL);

	CPDFSDK_Widget* pWidget = NULL;
	if (CPDF_FormControl* pCtrl = CPDFSDK_Widget::GetFormControl(pInterForm->GetInterForm(), pAnnot->GetAnnotDict()))
	{
		pWidget = new CPDFSDK_Widget(pAnnot, pPage, pInterForm);
		pInterForm->AddMap(pCtrl, pWidget);
		CPDF_InterForm* pPDFInterForm = pInterForm->GetInterForm();
		if(pPDFInterForm && pPDFInterForm->NeedConstructAP())
			pWidget->ResetAppearance(NULL,FALSE);
	}

	return pWidget;
}

CPDFSDK_Annot*		CPDFSDK_BFAnnotHandler::NewAnnot(IXFA_Widget* hWidget, CPDFSDK_PageView* pPage)
{
	return NULL;
}

void CPDFSDK_BFAnnotHandler::ReleaseAnnot(CPDFSDK_Annot* pAnnot)
{
	ASSERT(pAnnot != NULL);

	if (m_pFormFiller)
		m_pFormFiller->OnDelete(pAnnot);

	CPDFSDK_Widget* pWidget = (CPDFSDK_Widget*)pAnnot;
	CPDFSDK_InterForm* pInterForm = pWidget->GetInterForm();
	ASSERT(pInterForm != NULL);

	CPDF_FormControl* pCtrol = pWidget->GetFormControl();
	pInterForm->RemoveMap(pCtrol);


	delete pWidget;
}


void CPDFSDK_BFAnnotHandler::OnDraw(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, CFX_RenderDevice* pDevice, CPDF_Matrix* pUser2Device,  FX_DWORD dwFlags)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
		((CPDFSDK_BAAnnot*)pAnnot)->DrawAppearance(pDevice, pUser2Device, CPDF_Annot::Normal, NULL);
	}
	else
	{
		if (m_pFormFiller)
		{
			m_pFormFiller->OnDraw(pPageView, pAnnot, pDevice, pUser2Device, dwFlags);
		}
	}
}

void CPDFSDK_BFAnnotHandler::OnMouseEnter(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlag)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			 m_pFormFiller->OnMouseEnter(pPageView, pAnnot, nFlag);
	}


}
void CPDFSDK_BFAnnotHandler::OnMouseExit(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlag)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			 m_pFormFiller->OnMouseExit(pPageView, pAnnot, nFlag);
	}

}
FX_BOOL CPDFSDK_BFAnnotHandler::OnLButtonDown(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			return m_pFormFiller->OnLButtonDown(pPageView, pAnnot, nFlags, point);
	}

	return FALSE;
}

FX_BOOL CPDFSDK_BFAnnotHandler::OnLButtonUp(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			return m_pFormFiller->OnLButtonUp(pPageView, pAnnot, nFlags, point);
	}

	return FALSE;
}

FX_BOOL CPDFSDK_BFAnnotHandler::OnLButtonDblClk(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			return m_pFormFiller->OnLButtonDblClk(pPageView, pAnnot, nFlags, point);
	}

	return FALSE;
}

FX_BOOL CPDFSDK_BFAnnotHandler::OnMouseMove(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			return m_pFormFiller->OnMouseMove(pPageView, pAnnot, nFlags, point);
	}

	return FALSE;

}


FX_BOOL CPDFSDK_BFAnnotHandler::OnMouseWheel(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, short zDelta, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			return m_pFormFiller->OnMouseWheel(pPageView, pAnnot, nFlags, zDelta,point);
	}

	return FALSE;
}

FX_BOOL CPDFSDK_BFAnnotHandler::OnRButtonDown(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			return m_pFormFiller->OnRButtonDown(pPageView, pAnnot, nFlags, point);
	}

	return FALSE;
}
FX_BOOL CPDFSDK_BFAnnotHandler::OnRButtonUp(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			return m_pFormFiller->OnRButtonUp(pPageView, pAnnot, nFlags, point);
	}

	return FALSE;
}

FX_BOOL CPDFSDK_BFAnnotHandler::OnChar(CPDFSDK_Annot* pAnnot, FX_DWORD nChar, FX_DWORD nFlags)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			return m_pFormFiller->OnChar(pAnnot,nChar, nFlags);
	}

	return FALSE;
}

FX_BOOL CPDFSDK_BFAnnotHandler::OnKeyDown(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			return m_pFormFiller->OnKeyDown(pAnnot,nKeyCode, nFlag);
	}

	return FALSE;
}

FX_BOOL CPDFSDK_BFAnnotHandler::OnKeyUp(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag)
{

	return FALSE;
}
void	CPDFSDK_BFAnnotHandler::OnCreate(CPDFSDK_Annot* pAnnot)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			m_pFormFiller->OnCreate(pAnnot);
	}
}

void CPDFSDK_BFAnnotHandler::OnLoad(CPDFSDK_Annot* pAnnot)
{
	ASSERT(pAnnot != NULL);

	CPDFSDK_PageView* pPageView = pAnnot->GetPageView();
	ASSERT(pPageView != NULL);

	CPDFSDK_Document* pSDKDoc = pPageView->GetSDKDocument();
	ASSERT(pSDKDoc != NULL);

	CPDFXFA_Document* pDoc = pSDKDoc->GetDocument();
	ASSERT(pDoc != NULL);

	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		CPDFSDK_Widget* pWidget = (CPDFSDK_Widget*)pAnnot;
		if (!pWidget->IsAppearanceValid())
			pWidget->ResetAppearance(NULL, FALSE);

		int nFieldType = pWidget->GetFieldType();
		if (nFieldType == FIELDTYPE_TEXTFIELD || nFieldType == FIELDTYPE_COMBOBOX)
		{
			FX_BOOL bFormated = FALSE;
			CFX_WideString sValue = pWidget->OnFormat(bFormated);
			if (bFormated && nFieldType == FIELDTYPE_COMBOBOX)
			{
				pWidget->ResetAppearance(sValue.c_str(), FALSE);
			}
		}

		if (pDoc->GetDocType() == DOCTYPE_STATIC_XFA)
		{
			if (!pWidget->IsAppearanceValid() && !pWidget->GetValue().IsEmpty())
				pWidget->ResetAppearance(FALSE);
		}


		if (m_pFormFiller)
			m_pFormFiller->OnLoad(pAnnot);
	}
}

FX_BOOL	CPDFSDK_BFAnnotHandler::OnSetFocus(CPDFSDK_Annot* pAnnot, FX_DWORD nFlag)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			return m_pFormFiller->OnSetFocus(pAnnot,nFlag);
	}

	return TRUE;
}
FX_BOOL	CPDFSDK_BFAnnotHandler::OnKillFocus(CPDFSDK_Annot* pAnnot, FX_DWORD nFlag)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			return m_pFormFiller->OnKillFocus(pAnnot,nFlag);
	}

	return TRUE;
}

CPDF_Rect CPDFSDK_BFAnnotHandler::GetViewBBox(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot)
{
	ASSERT(pAnnot != NULL);
	CFX_ByteString sSubType = pAnnot->GetSubType();

	if (sSubType == BFFT_SIGNATURE)
	{
	}
	else
	{
		if (m_pFormFiller)
			return m_pFormFiller->GetViewBBox(pPageView, pAnnot);

	}

	return CPDF_Rect(0,0,0,0);
}

FX_BOOL	CPDFSDK_BFAnnotHandler::HitTest(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, const CPDF_Point& point)
{
	ASSERT(pPageView);
	ASSERT(pAnnot);

	CPDF_Rect rect = GetViewBBox(pPageView, pAnnot);
	return rect.Contains(point.x, point.y);
}

//CPDFSDK_XFAAnnotHandler

#define FWL_WGTHITTEST_Unknown			0
#define FWL_WGTHITTEST_Client			1 //arrow
#define FWL_WGTHITTEST_Titlebar			11 //caption
#define FWL_WGTHITTEST_HScrollBar		15
#define FWL_WGTHITTEST_VScrollBar		16
#define FWL_WGTHITTEST_Border			17
#define FWL_WGTHITTEST_Edit				19
#define FWL_WGTHITTEST_HyperLink        20

CPDFSDK_XFAAnnotHandler::CPDFSDK_XFAAnnotHandler(CPDFDoc_Environment* pApp) :
	m_pApp(pApp)
{

}

CPDFSDK_Annot* CPDFSDK_XFAAnnotHandler::NewAnnot(IXFA_Widget* pAnnot, CPDFSDK_PageView* pPage)
{
	ASSERT(pPage != NULL);
	ASSERT(pAnnot != NULL);

	CPDFSDK_Document* pSDKDoc  = m_pApp->GetCurrentDoc();
	ASSERT(pSDKDoc);
	CPDFSDK_InterForm* pInterForm = (CPDFSDK_InterForm*)pSDKDoc->GetInterForm();
	ASSERT(pInterForm != NULL);

	CPDFSDK_XFAWidget* pWidget = NULL;
	pWidget = new CPDFSDK_XFAWidget(pAnnot, pPage, pInterForm);
	ASSERT(pWidget != NULL);
	pInterForm->AddXFAMap(pAnnot, pWidget);

	return pWidget;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::CanAnswer(CPDFSDK_Annot* pAnnot)
{
	ASSERT(pAnnot != NULL);

	return pAnnot->GetXFAWidget() != NULL;
}

void CPDFSDK_XFAAnnotHandler::OnDraw(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, CFX_RenderDevice* pDevice, CPDF_Matrix* pUser2Device, FX_DWORD dwFlags)
{
	ASSERT(pPageView != NULL);
	ASSERT(pAnnot != NULL);

	CPDFSDK_Document* pSDKDoc = pPageView->GetSDKDocument();
	ASSERT(pSDKDoc != NULL);

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	CFX_Graphics gs;
	gs.Create(pDevice);

	CFX_Matrix mt;
	mt = *(CFX_Matrix*)pUser2Device;

	IXFA_Widget* hWidget = pAnnot->GetXFAWidget();
	ASSERT(hWidget != NULL);

	FX_BOOL bIsHighlight = FALSE;
	if (pSDKDoc->GetFocusAnnot() != pAnnot)
		bIsHighlight = TRUE;

	pWidgetHandler->RenderWidget(pAnnot->GetXFAWidget(), &gs, &mt, bIsHighlight);

	// to do highlight and shadow

}

void CPDFSDK_XFAAnnotHandler::ReleaseAnnot(CPDFSDK_Annot* pAnnot)
{
	ASSERT(pAnnot != NULL);

	CPDFSDK_XFAWidget* pWidget = (CPDFSDK_XFAWidget*)pAnnot;
	CPDFSDK_InterForm* pInterForm = pWidget->GetInterForm();
	ASSERT(pInterForm != NULL);

	pInterForm->RemoveXFAMap(pWidget->GetXFAWidget());

	delete pWidget;
}

CPDF_Rect CPDFSDK_XFAAnnotHandler::GetViewBBox(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot)
{
	ASSERT(pAnnot != NULL);

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	XFA_ELEMENT eType = pWidgetHandler->GetDataAcc(pAnnot->GetXFAWidget())->GetUIType();
	CFX_RectF rcBBox;
	if (eType == XFA_ELEMENT_Signature)
		pWidgetHandler->GetBBox (pAnnot->GetXFAWidget(), rcBBox, XFA_WIDGETSTATUS_Visible, TRUE);
	else
		pWidgetHandler->GetBBox (pAnnot->GetXFAWidget(), rcBBox, 0);

	CFX_FloatRect rcWidget(rcBBox.left, rcBBox.top, rcBBox.left + rcBBox.width, rcBBox.top+rcBBox.height);
	rcWidget.left -= 1.0f;
	rcWidget.right += 1.0f;
	rcWidget.bottom -= 1.0f;
	rcWidget.top += 1.0f;

	return rcWidget;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::HitTest(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, const CPDF_Point& point)
{
	if (!pPageView || !pAnnot)
		return FALSE;

	CPDFSDK_Document* pSDKDoc = pPageView->GetSDKDocument();
	if (!pSDKDoc)
		return FALSE;

	CPDFXFA_Document* pDoc = pSDKDoc->GetDocument();
	if (!pDoc)
		return FALSE;

	IXFA_DocView* pDocView = pDoc->GetXFADocView();
	if (!pDocView)
		return FALSE;

	IXFA_WidgetHandler* pWidgetHandler = pDocView->GetWidgetHandler();
	if (!pWidgetHandler)
		return FALSE;

	FX_DWORD dwHitTest = pWidgetHandler->OnHitTest(pAnnot->GetXFAWidget(), point.x, point.y);
	return (dwHitTest != FWL_WGTHITTEST_Unknown);
}

void CPDFSDK_XFAAnnotHandler::OnMouseEnter(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlag)
{
	if (!pPageView || !pAnnot)
		return;
	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	pWidgetHandler->OnMouseEnter(pAnnot->GetXFAWidget());
}

void CPDFSDK_XFAAnnotHandler::OnMouseExit(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlag)
{
	if (!pPageView || !pAnnot)
		return;

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	pWidgetHandler->OnMouseExit(pAnnot->GetXFAWidget());
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnLButtonDown(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	if (!pPageView || !pAnnot)
		return FALSE;

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	FX_BOOL bRet = FALSE;
	bRet = pWidgetHandler->OnLButtonDown(pAnnot->GetXFAWidget(), GetFWLFlags(nFlags), point.x, point.y);

	return bRet;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnLButtonUp(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	if (!pPageView || !pAnnot)
		return FALSE;

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	FX_BOOL bRet = FALSE;
	bRet = pWidgetHandler->OnLButtonUp(pAnnot->GetXFAWidget(), GetFWLFlags(nFlags), point.x, point.y);

	return bRet;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnLButtonDblClk(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	if (!pPageView || !pAnnot)
		return FALSE;

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	FX_BOOL bRet = FALSE;
	bRet = pWidgetHandler->OnLButtonDblClk(pAnnot->GetXFAWidget(), GetFWLFlags(nFlags), point.x, point.y);

	return bRet;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnMouseMove(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	if (!pPageView || !pAnnot)
		return FALSE;

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	FX_BOOL bRet = FALSE;
	bRet = pWidgetHandler->OnMouseMove(pAnnot->GetXFAWidget(), GetFWLFlags(nFlags), point.x, point.y);

	return bRet;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnMouseWheel(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, short zDelta, const CPDF_Point& point)
{
	if (!pPageView || !pAnnot)
		return FALSE;

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	FX_BOOL bRet = FALSE;
	bRet = pWidgetHandler->OnMouseWheel(pAnnot->GetXFAWidget(), GetFWLFlags(nFlags), zDelta, point.x, point.y);

	return bRet;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnRButtonDown(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	if (!pPageView || !pAnnot)
		return FALSE;

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	FX_BOOL bRet = FALSE;
	bRet = pWidgetHandler->OnRButtonDown(pAnnot->GetXFAWidget(), GetFWLFlags(nFlags), point.x, point.y);

	return bRet;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnRButtonUp(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	if (!pPageView || !pAnnot)
		return FALSE;

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	FX_BOOL bRet = FALSE;
	bRet = pWidgetHandler->OnRButtonUp(pAnnot->GetXFAWidget(), GetFWLFlags(nFlags), point.x, point.y);

	return bRet;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnRButtonDblClk(CPDFSDK_PageView *pPageView, CPDFSDK_Annot* pAnnot, FX_DWORD nFlags, const CPDF_Point& point)
{
	if (!pPageView || !pAnnot)
		return FALSE;

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	FX_BOOL bRet = FALSE;
	bRet = pWidgetHandler->OnRButtonDblClk(pAnnot->GetXFAWidget(), GetFWLFlags(nFlags), point.x, point.y);

	return bRet;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnChar(CPDFSDK_Annot* pAnnot, FX_DWORD nChar, FX_DWORD nFlags)
{
	if (!pAnnot)
		return FALSE;

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	FX_BOOL bRet = FALSE;
	bRet = pWidgetHandler->OnChar(pAnnot->GetXFAWidget(), nChar, GetFWLFlags(nFlags));

	return bRet;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnKeyDown(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag)
{
	if (!pAnnot)
		return FALSE;

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	FX_BOOL bRet = FALSE;
	bRet = pWidgetHandler->OnKeyDown(pAnnot->GetXFAWidget(), nKeyCode, GetFWLFlags(nFlag));

	return bRet;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnKeyUp(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag)
{
	if (!pAnnot)
		return FALSE;

	IXFA_WidgetHandler* pWidgetHandler = GetXFAWidgetHandler(pAnnot);
	ASSERT(pWidgetHandler != NULL);

	FX_BOOL bRet = FALSE;
	bRet = pWidgetHandler->OnKeyUp(pAnnot->GetXFAWidget(), nKeyCode, GetFWLFlags(nFlag));

	return bRet;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnSetFocus(CPDFSDK_Annot* pAnnot, FX_DWORD nFlag)
{
	return TRUE;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnKillFocus(CPDFSDK_Annot* pAnnot, FX_DWORD nFlag)
{
	return TRUE;
}

FX_BOOL CPDFSDK_XFAAnnotHandler::OnXFAChangedFocus(CPDFSDK_Annot* pOldAnnot, CPDFSDK_Annot* pNewAnnot)
{
	IXFA_WidgetHandler* pWidgetHandler = NULL;

	if (pOldAnnot)
		pWidgetHandler = GetXFAWidgetHandler(pOldAnnot);
	else if (pNewAnnot)
		pWidgetHandler = GetXFAWidgetHandler(pNewAnnot);

	if (pWidgetHandler)
	{
		FX_BOOL bRet = TRUE;
		IXFA_Widget* hWidget = pNewAnnot ? pNewAnnot->GetXFAWidget() : NULL;
		if (hWidget)
		{
			IXFA_PageView* pXFAPageView = pWidgetHandler->GetPageView(hWidget);
			if (pXFAPageView)
			{
				bRet = pXFAPageView->GetDocView()->SetFocus(hWidget);
				if (pXFAPageView->GetDocView()->GetFocusWidget() == hWidget)
					bRet = TRUE;
			}
		}
		return bRet;
	}

	return TRUE;
}

IXFA_WidgetHandler* CPDFSDK_XFAAnnotHandler::GetXFAWidgetHandler(CPDFSDK_Annot* pAnnot)
{
	if (!pAnnot)
		return NULL;

	CPDFSDK_PageView* pPageView = pAnnot->GetPageView();
	if (!pPageView)
		return NULL;

	CPDFSDK_Document* pSDKDoc = pPageView->GetSDKDocument();
	if (!pSDKDoc)
		return NULL;

	CPDFXFA_Document* pDoc = pSDKDoc->GetDocument();
	if (!pDoc)
		return NULL;

	IXFA_DocView* pDocView = pDoc->GetXFADocView();
	if (!pDocView)
		return NULL;

	return pDocView->GetWidgetHandler();
}

#define FWL_KEYFLAG_Ctrl					(1 << 0)
#define FWL_KEYFLAG_Alt						(1 << 1)
#define FWL_KEYFLAG_Shift					(1 << 2)
#define FWL_KEYFLAG_LButton					(1 << 3)
#define FWL_KEYFLAG_RButton					(1 << 4)
#define FWL_KEYFLAG_MButton					(1 << 5)

FX_DWORD CPDFSDK_XFAAnnotHandler::GetFWLFlags(FX_DWORD dwFlag)
{
	FX_DWORD dwFWLFlag = 0;

	if (dwFlag & FWL_EVENTFLAG_ControlKey)
		dwFWLFlag |= FWL_KEYFLAG_Ctrl;
	if (dwFlag & FWL_EVENTFLAG_LeftButtonDown)
		dwFWLFlag |= FWL_KEYFLAG_LButton;
	if (dwFlag & FWL_EVENTFLAG_MiddleButtonDown)
		dwFWLFlag |= FWL_KEYFLAG_MButton;
	if (dwFlag & FWL_EVENTFLAG_RightButtonDown)
		dwFWLFlag |= FWL_KEYFLAG_RButton;
	if (dwFlag & FWL_EVENTFLAG_ShiftKey)
		dwFWLFlag |= FWL_KEYFLAG_Shift;
	if (dwFlag & FWL_EVENTFLAG_AltKey)
		dwFWLFlag |= FWL_KEYFLAG_Alt;

	return dwFWLFlag;
}

//CReader_AnnotIteratorEx

CPDFSDK_AnnotIterator::CPDFSDK_AnnotIterator(CPDFSDK_PageView * pPageView,FX_BOOL bReverse,
												 FX_BOOL bIgnoreTopmost/*=FALSE*/,
												 FX_BOOL bCircle/*=FALSE*/,
												 CFX_PtrArray *pList/*=NULL*/)
{
	ASSERT(pPageView);
	m_bReverse=bReverse;
	m_bIgnoreTopmost= bIgnoreTopmost;
	m_bCircle=bCircle;
	m_pIteratorAnnotList.RemoveAll();
	InitIteratorAnnotList(pPageView,pList);
}

CPDFSDK_Annot*	CPDFSDK_AnnotIterator::NextAnnot (const CPDFSDK_Annot* pCurrent)
{

	int index = -1;
	int nCount = m_pIteratorAnnotList.GetSize();
	if (pCurrent) {
		for(int i=0;i<nCount;i++){
			CPDFSDK_Annot * pReaderAnnot= (CPDFSDK_Annot *)m_pIteratorAnnotList.GetAt(i);
			if(pReaderAnnot ==pCurrent){
				index=i;
				break;
			}
		}
	}
	return NextAnnot(index);
}
CPDFSDK_Annot*	CPDFSDK_AnnotIterator::PrevAnnot (const CPDFSDK_Annot*pCurrent)
{
	int index = -1;
	int nCount = m_pIteratorAnnotList.GetSize();
	if(pCurrent){
		for(int i=0;i<nCount;i++){
			CPDFSDK_Annot * pReaderAnnot= (CPDFSDK_Annot*)m_pIteratorAnnotList.GetAt(i);
			if(pReaderAnnot ==pCurrent){
				index=i;
				break;
			}
		}
	}
	return PrevAnnot(index);
}
CPDFSDK_Annot*	CPDFSDK_AnnotIterator::NextAnnot (int& index)
{

	int nCount=m_pIteratorAnnotList.GetSize();
    if(nCount<=0) index=-1;
    else{
		if(index<0){
			index=0;
		}
		else{
			if(m_bCircle){
				index=( index <nCount-1) ? (index+1) :0;
			}
			else{
				index=( index <nCount-1) ? (index+1) :-1;
			}

		}
	}
	return (index <0) ? NULL : (CPDFSDK_Annot*)m_pIteratorAnnotList.GetAt(index);
}


CPDFSDK_Annot*	CPDFSDK_AnnotIterator::PrevAnnot (int& index)
{

	int nCount=m_pIteratorAnnotList.GetSize();
    if(nCount<=0) index=-1;
	else{
		if(index<0){
			index=nCount-1;
		}
		else{
			if(m_bCircle){
				index = ( index >0) ? (index-1) :nCount-1;
			}
			else{
				index = ( index >0) ? (index-1) :-1;
			}
		}
	}
	return (index <0) ? NULL : (CPDFSDK_Annot*)m_pIteratorAnnotList.GetAt(index);
}


CPDFSDK_Annot*CPDFSDK_AnnotIterator::Next(const CPDFSDK_Annot* pCurrent)
{

	return (m_bReverse) ? PrevAnnot(pCurrent):NextAnnot(pCurrent);

}

CPDFSDK_Annot*	CPDFSDK_AnnotIterator::Prev(const CPDFSDK_Annot* pCurrent)
{

	return (m_bReverse) ? NextAnnot(pCurrent):PrevAnnot(pCurrent);
}

CPDFSDK_Annot*CPDFSDK_AnnotIterator::Next(int& index )
{

	return (m_bReverse) ? PrevAnnot(index):NextAnnot(index);

}

CPDFSDK_Annot*	CPDFSDK_AnnotIterator::Prev(int& index )
{

	return (m_bReverse) ? NextAnnot(index):PrevAnnot(index);
}


void CPDFSDK_AnnotIterator::InsertSort(CFX_PtrArray &arrayList, AI_COMPARE pCompare)
{
	for (int i = 1; i < arrayList.GetSize(); i++)
	{
		if (pCompare((CPDFSDK_Annot*)(arrayList[i]) , (CPDFSDK_Annot*)(arrayList[i-1])) < 0)
		{
			int j = i-1;
			CPDFSDK_Annot* pTemp = (CPDFSDK_Annot*)arrayList[i];

			do
			{
				arrayList[j + 1] = arrayList[j];
			} while (--j >= 0 && pCompare(pTemp, (CPDFSDK_Annot*)arrayList[j]) < 0);

			arrayList[j+1] = pTemp;
		}
	}
}

int LyOrderCompare(CPDFSDK_Annot* p1, CPDFSDK_Annot* p2)
{
	if(p1->GetLayoutOrder() < p2->GetLayoutOrder())
		return -1;
	else if (p1->GetLayoutOrder() == p2->GetLayoutOrder())
		return 0;
	else
		return 1;
}

FX_BOOL CPDFSDK_AnnotIterator::InitIteratorAnnotList(CPDFSDK_PageView* pPageView,CFX_PtrArray * pAnnotList)
{
	ASSERT(pPageView);

	if(pAnnotList==NULL){
		pAnnotList=pPageView->GetAnnotList();
	}

	m_pIteratorAnnotList.RemoveAll();
	if(!pAnnotList) return FALSE;

	CPDFSDK_Annot * pTopMostAnnot= (m_bIgnoreTopmost) ? NULL : pPageView->GetFocusAnnot();


	int nCount =pAnnotList->GetSize();

	for(int i = nCount- 1 ;i >= 0;i--)
	{
		CPDFSDK_Annot * pReaderAnnot= (CPDFSDK_Annot*)pAnnotList->GetAt(i);
		m_pIteratorAnnotList.Add(pReaderAnnot);
	}

	InsertSort(m_pIteratorAnnotList,&LyOrderCompare);

	if(pTopMostAnnot)
	{
		for(int i=0 ;i<nCount;i++)
		{
			CPDFSDK_Annot * pReaderAnnot = (CPDFSDK_Annot*)m_pIteratorAnnotList.GetAt(i);
			if(pReaderAnnot == pTopMostAnnot)
			{
				m_pIteratorAnnotList.RemoveAt(i);
				m_pIteratorAnnotList.InsertAt(0, pReaderAnnot);
				break;
			}
		}
	}

	return TRUE;
}

