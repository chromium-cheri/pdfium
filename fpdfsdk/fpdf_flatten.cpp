// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "public/fpdf_flatten.h"

#include <limits.h>

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

#include "constants/annotation_common.h"
#include "constants/annotation_flags.h"
#include "constants/page_object.h"
#include "core/fpdfapi/edit/cpdf_contentstream_write_utils.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfapi/parser/fpdf_parser_utility.h"
#include "core/fpdfdoc/cpdf_annot.h"
#include "core/fxcrt/fx_string_wrappers.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "third_party/base/notreached.h"

enum FPDF_TYPE { MAX, MIN };
enum FPDF_VALUE { TOP, LEFT, RIGHT, BOTTOM };

namespace {

bool IsValidRect(const CFX_FloatRect& rect, const CFX_FloatRect& rcPage) {
  constexpr float kMinSize = 0.000001f;
  if (rect.IsEmpty() || rect.Width() < kMinSize || rect.Height() < kMinSize)
    return false;

  if (rcPage.IsEmpty())
    return true;

  constexpr float kMinBorderSize = 10.000001f;
  return rect.left - rcPage.left >= -kMinBorderSize &&
         rect.right - rcPage.right <= kMinBorderSize &&
         rect.top - rcPage.top <= kMinBorderSize &&
         rect.bottom - rcPage.bottom >= -kMinBorderSize;
}

void GetContentsRect(CPDF_Document* pDoc,
                     RetainPtr<CPDF_Dictionary> pDict,
                     std::vector<CFX_FloatRect>* pRectArray) {
  auto pPDFPage = pdfium::MakeRetain<CPDF_Page>(pDoc, pDict);
  pPDFPage->ParseContent();

  for (const auto& pPageObject : *pPDFPage) {
    const CFX_FloatRect& rc = pPageObject->GetRect();
    if (IsValidRect(rc, pDict->GetRectFor(pdfium::page_object::kMediaBox)))
      pRectArray->push_back(rc);
  }
}

void ParserStream(const CPDF_Dictionary* pPageDic,
                  CPDF_Dictionary* pStream,
                  std::vector<CFX_FloatRect>* pRectArray,
                  std::vector<CPDF_Dictionary*>* pObjectArray) {
  if (!pStream)
    return;
  CFX_FloatRect rect;
  if (pStream->KeyExist("Rect"))
    rect = pStream->GetRectFor("Rect");
  else if (pStream->KeyExist("BBox"))
    rect = pStream->GetRectFor("BBox");

  if (IsValidRect(rect, pPageDic->GetRectFor(pdfium::page_object::kMediaBox)))
    pRectArray->push_back(rect);

  pObjectArray->push_back(pStream);
}

int ParserAnnots(CPDF_Document* pSourceDoc,
                 RetainPtr<CPDF_Dictionary> pPageDic,
                 std::vector<CFX_FloatRect>* pRectArray,
                 std::vector<CPDF_Dictionary*>* pObjectArray,
                 int nUsage) {
  if (!pSourceDoc)
    return FLATTEN_FAIL;

  GetContentsRect(pSourceDoc, pPageDic, pRectArray);
  const CPDF_Array* pAnnots = pPageDic->GetArrayFor("Annots");
  if (!pAnnots)
    return FLATTEN_NOTHINGTODO;

  CPDF_ArrayLocker locker(pAnnots);
  for (const auto& pAnnot : locker) {
    RetainPtr<CPDF_Dictionary> pAnnotDict =
        ToDictionary(pAnnot->GetMutableDirect());
    if (!pAnnotDict)
      continue;

    ByteString sSubtype =
        pAnnotDict->GetStringFor(pdfium::annotation::kSubtype);
    if (sSubtype == "Popup")
      continue;

    int nAnnotFlag = pAnnotDict->GetIntegerFor("F");
    if (nAnnotFlag & pdfium::annotation_flags::kHidden)
      continue;

    bool bParseStream;
    if (nUsage == FLAT_NORMALDISPLAY)
      bParseStream = !(nAnnotFlag & pdfium::annotation_flags::kInvisible);
    else
      bParseStream = !!(nAnnotFlag & pdfium::annotation_flags::kPrint);
    if (bParseStream)
      ParserStream(pPageDic.Get(), pAnnotDict.Get(), pRectArray, pObjectArray);
  }
  return FLATTEN_SUCCESS;
}

float GetMinMaxValue(const std::vector<CFX_FloatRect>& array,
                     FPDF_TYPE type,
                     FPDF_VALUE value) {
  if (array.empty())
    return 0.0f;

  size_t nRects = array.size();
  std::vector<float> pArray(nRects);
  switch (value) {
    case LEFT:
      for (size_t i = 0; i < nRects; i++)
        pArray[i] = array[i].left;
      break;
    case TOP:
      for (size_t i = 0; i < nRects; i++)
        pArray[i] = array[i].top;
      break;
    case RIGHT:
      for (size_t i = 0; i < nRects; i++)
        pArray[i] = array[i].right;
      break;
    case BOTTOM:
      for (size_t i = 0; i < nRects; i++)
        pArray[i] = array[i].bottom;
      break;
    default:
      NOTREACHED();
      return 0.0f;
  }

  float fRet = pArray[0];
  if (type == MAX) {
    for (size_t i = 1; i < nRects; i++)
      fRet = std::max(fRet, pArray[i]);
  } else {
    for (size_t i = 1; i < nRects; i++)
      fRet = std::min(fRet, pArray[i]);
  }
  return fRet;
}

CFX_FloatRect CalculateRect(std::vector<CFX_FloatRect>* pRectArray) {
  CFX_FloatRect rcRet;

  rcRet.left = GetMinMaxValue(*pRectArray, MIN, LEFT);
  rcRet.top = GetMinMaxValue(*pRectArray, MAX, TOP);
  rcRet.right = GetMinMaxValue(*pRectArray, MAX, RIGHT);
  rcRet.bottom = GetMinMaxValue(*pRectArray, MIN, BOTTOM);

  return rcRet;
}

ByteString GenerateFlattenedContent(const ByteString& key) {
  return "q 1 0 0 1 0 0 cm /" + key + " Do Q";
}

CPDF_Object* NewIndirectContentsStream(CPDF_Document* pDocument,
                                       const ByteString& contents) {
  CPDF_Stream* pNewContents = pDocument->NewIndirect<CPDF_Stream>(
      nullptr, 0, pDocument->New<CPDF_Dictionary>());
  pNewContents->SetData(contents.raw_span());
  return pNewContents;
}

void SetPageContents(const ByteString& key,
                     CPDF_Dictionary* pPage,
                     CPDF_Document* pDocument) {
  RetainPtr<CPDF_Array> pContentsArray =
      pPage->GetMutableArrayFor(pdfium::page_object::kContents);
  RetainPtr<CPDF_Stream> pContentsStream =
      pPage->GetMutableStreamFor(pdfium::page_object::kContents);
  if (!pContentsStream && !pContentsArray) {
    if (!key.IsEmpty()) {
      pPage->SetFor(
          pdfium::page_object::kContents,
          NewIndirectContentsStream(pDocument, GenerateFlattenedContent(key))
              ->MakeReference(pDocument));
    }
    return;
  }

  pPage->ConvertToIndirectObjectFor(pdfium::page_object::kContents, pDocument);
  if (pContentsArray) {
    pContentsArray->InsertAt(
        0, NewIndirectContentsStream(pDocument, "q")->MakeReference(pDocument));
    pContentsArray->Append(
        NewIndirectContentsStream(pDocument, "Q")->MakeReference(pDocument));
  } else {
    ByteString sStream = "q\n";
    {
      auto pAcc = pdfium::MakeRetain<CPDF_StreamAcc>(pContentsStream);
      pAcc->LoadAllDataFiltered();
      sStream += ByteString(pAcc->GetSpan());
      sStream += "\nQ";
    }
    pContentsStream->SetDataAndRemoveFilter(sStream.raw_span());
    pContentsArray = pDocument->NewIndirect<CPDF_Array>();
    pContentsArray->AppendNew<CPDF_Reference>(pDocument,
                                              pContentsStream->GetObjNum());
    pPage->SetNewFor<CPDF_Reference>(pdfium::page_object::kContents, pDocument,
                                     pContentsArray->GetObjNum());
  }
  if (!key.IsEmpty()) {
    pContentsArray->Append(
        NewIndirectContentsStream(pDocument, GenerateFlattenedContent(key))
            ->MakeReference(pDocument));
  }
}

CFX_Matrix GetMatrix(const CFX_FloatRect& rcAnnot,
                     const CFX_FloatRect& rcStream,
                     const CFX_Matrix& matrix) {
  if (rcStream.IsEmpty())
    return CFX_Matrix();

  CFX_FloatRect rcTransformed = matrix.TransformRect(rcStream);
  rcTransformed.Normalize();

  float a = rcAnnot.Width() / rcTransformed.Width();
  float d = rcAnnot.Height() / rcTransformed.Height();

  float e = rcAnnot.left - rcTransformed.left * a;
  float f = rcAnnot.bottom - rcTransformed.bottom * d;
  return CFX_Matrix(a, 0.0f, 0.0f, d, e, f);
}

}  // namespace

FPDF_EXPORT int FPDF_CALLCONV FPDFPage_Flatten(FPDF_PAGE page, int nFlag) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!page)
    return FLATTEN_FAIL;

  CPDF_Document* pDocument = pPage->GetDocument();
  RetainPtr<CPDF_Dictionary> pPageDict = pPage->GetMutableDict();
  if (!pDocument)
    return FLATTEN_FAIL;

  std::vector<CPDF_Dictionary*> ObjectArray;
  std::vector<CFX_FloatRect> RectArray;
  int iRet =
      ParserAnnots(pDocument, pPageDict, &RectArray, &ObjectArray, nFlag);
  if (iRet == FLATTEN_NOTHINGTODO || iRet == FLATTEN_FAIL)
    return iRet;

  CFX_FloatRect rcMerger = CalculateRect(&RectArray);
  CFX_FloatRect rcOriginalMB =
      pPageDict->GetRectFor(pdfium::page_object::kMediaBox);
  if (pPageDict->KeyExist(pdfium::page_object::kCropBox))
    rcOriginalMB = pPageDict->GetRectFor(pdfium::page_object::kCropBox);

  rcOriginalMB.Normalize();
  if (rcOriginalMB.IsEmpty())
    rcOriginalMB = CFX_FloatRect(0.0f, 0.0f, 612.0f, 792.0f);

  CFX_FloatRect rcOriginalCB;
  if (pPageDict->KeyExist(pdfium::page_object::kCropBox)) {
    rcOriginalCB = pPageDict->GetRectFor(pdfium::page_object::kCropBox);
    rcOriginalCB.Normalize();
  }
  if (rcOriginalCB.IsEmpty())
    rcOriginalCB = rcOriginalMB;

  rcMerger.left = std::max(rcMerger.left, rcOriginalMB.left);
  rcMerger.right = std::min(rcMerger.right, rcOriginalMB.right);
  rcMerger.bottom = std::max(rcMerger.bottom, rcOriginalMB.bottom);
  rcMerger.top = std::min(rcMerger.top, rcOriginalMB.top);

  pPageDict->SetRectFor(pdfium::page_object::kMediaBox, rcOriginalMB);
  pPageDict->SetRectFor(pdfium::page_object::kCropBox, rcOriginalCB);

  RetainPtr<CPDF_Dictionary> pRes =
      pPageDict->GetOrCreateDictFor(pdfium::page_object::kResources);
  CPDF_Stream* pNewXObject = pDocument->NewIndirect<CPDF_Stream>(
      nullptr, 0, pDocument->New<CPDF_Dictionary>());
  RetainPtr<CPDF_Dictionary> pPageXObject = pRes->GetOrCreateDictFor("XObject");

  ByteString key;
  if (!ObjectArray.empty()) {
    int i = 0;
    while (i < INT_MAX) {
      ByteString sKey = ByteString::Format("FFT%d", i);
      if (!pPageXObject->KeyExist(sKey)) {
        key = std::move(sKey);
        break;
      }
      ++i;
    }
  }

  SetPageContents(key, pPageDict.Get(), pDocument);

  CPDF_Dictionary* pNewXORes = nullptr;
  if (!key.IsEmpty()) {
    pPageXObject->SetNewFor<CPDF_Reference>(key, pDocument,
                                            pNewXObject->GetObjNum());

    RetainPtr<CPDF_Dictionary> pNewOXbjectDic = pNewXObject->GetMutableDict();
    pNewXORes = pNewOXbjectDic->SetNewFor<CPDF_Dictionary>("Resources");
    pNewOXbjectDic->SetNewFor<CPDF_Name>("Type", "XObject");
    pNewOXbjectDic->SetNewFor<CPDF_Name>("Subtype", "Form");
    pNewOXbjectDic->SetNewFor<CPDF_Number>("FormType", 1);
    pNewOXbjectDic->SetRectFor("BBox", rcOriginalCB);
  }

  for (size_t i = 0; i < ObjectArray.size(); ++i) {
    CPDF_Dictionary* pAnnotDict = ObjectArray[i];
    if (!pAnnotDict)
      continue;

    CFX_FloatRect rcAnnot = pAnnotDict->GetRectFor(pdfium::annotation::kRect);
    rcAnnot.Normalize();

    ByteString sAnnotState = pAnnotDict->GetStringFor("AS");
    RetainPtr<CPDF_Dictionary> pAnnotAP =
        pAnnotDict->GetMutableDictFor(pdfium::annotation::kAP);
    if (!pAnnotAP)
      continue;

    RetainPtr<CPDF_Stream> pAPStream = pAnnotAP->GetMutableStreamFor("N");
    if (!pAPStream) {
      RetainPtr<CPDF_Dictionary> pAPDict = pAnnotAP->GetMutableDictFor("N");
      if (!pAPDict)
        continue;

      if (!sAnnotState.IsEmpty()) {
        pAPStream = pAPDict->GetMutableStreamFor(sAnnotState);
      } else {
        if (pAPDict->size() > 0) {
          CPDF_DictionaryLocker locker(pAPDict.Get());
          RetainPtr<CPDF_Object> pFirstObj = locker.begin()->second;
          if (pFirstObj) {
            if (pFirstObj->IsReference())
              pFirstObj = pFirstObj->GetMutableDirect();
            if (!pFirstObj->IsStream())
              continue;
            pAPStream = pFirstObj->AsMutableStream();
          }
        }
      }
    }
    if (!pAPStream)
      continue;

    const CPDF_Dictionary* pAPDict = pAPStream->GetDict();
    CFX_FloatRect rcStream;
    if (pAPDict->KeyExist("Rect"))
      rcStream = pAPDict->GetRectFor("Rect");
    else if (pAPDict->KeyExist("BBox"))
      rcStream = pAPDict->GetRectFor("BBox");
    rcStream.Normalize();

    if (rcStream.IsEmpty())
      continue;

    RetainPtr<CPDF_Object> pObj = pAPStream;
    if (pObj->IsInline()) {
      RetainPtr<CPDF_Object> pNew = pObj->Clone();
      pObj = pNew.Get();
      pDocument->AddIndirectObject(std::move(pNew));
    }

    RetainPtr<CPDF_Dictionary> pObjDict = pObj->GetMutableDict();
    if (pObjDict) {
      pObjDict->SetNewFor<CPDF_Name>("Type", "XObject");
      pObjDict->SetNewFor<CPDF_Name>("Subtype", "Form");
    }

    RetainPtr<CPDF_Dictionary> pXObject =
        pNewXORes->GetOrCreateDictFor("XObject");
    ByteString sFormName = ByteString::Format("F%d", i);
    pXObject->SetNewFor<CPDF_Reference>(sFormName, pDocument,
                                        pObj->GetObjNum());

    ByteString sStream;
    {
      auto pAcc =
          pdfium::MakeRetain<CPDF_StreamAcc>(pdfium::WrapRetain(pNewXObject));
      pAcc->LoadAllDataFiltered();
      sStream = ByteString(pAcc->GetSpan());
    }
    CFX_Matrix matrix = pAPDict->GetMatrixFor("Matrix");
    CFX_Matrix m = GetMatrix(rcAnnot, rcStream, matrix);
    m.b = 0;
    m.c = 0;
    fxcrt::ostringstream buf;
    WriteMatrix(buf, m);
    ByteString str(buf);
    sStream += ByteString::Format("q %s cm /%s Do Q\n", str.c_str(),
                                  sFormName.c_str());
    pNewXObject->SetDataAndRemoveFilter(sStream.raw_span());
  }
  pPageDict->RemoveFor("Annots");
  return FLATTEN_SUCCESS;
}
