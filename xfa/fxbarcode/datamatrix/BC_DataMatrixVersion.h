// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXBARCODE_DATAMATRIX_BC_DATAMATRIXVERSION_H_
#define XFA_FXBARCODE_DATAMATRIX_BC_DATAMATRIXVERSION_H_

#include "core/fxcrt/include/fx_basic.h"

class CBC_DataMatrixVersion;

class ECB {
 public:
  ECB(int32_t count, int32_t dataCodewords);

  int32_t GetCount() const { return m_count; }
  int32_t GetDataCodewords() const { return m_dataCodewords; }

 private:
  int32_t m_count;
  int32_t m_dataCodewords;
};

class ECBlocks {
 public:
  ECBlocks(int32_t ecCodewords, ECB* ecBlocks);
  ECBlocks(int32_t ecCodewords, ECB* ecBlocks1, ECB* ecBlocks2);
  ~ECBlocks();

  int32_t GetECCodewords() { return m_ecCodewords; }
  const CFX_ArrayTemplate<ECB*>& GetECBlocks() { return m_ecBlocksArray; }

 private:
  int32_t m_ecCodewords;
  CFX_ArrayTemplate<ECB*> m_ecBlocksArray;
};

class CBC_DataMatrixVersion {
 public:
  CBC_DataMatrixVersion(int32_t versionNumber,
                        int32_t symbolSizeRows,
                        int32_t symbolSizeColumns,
                        int32_t dataRegionSizeRows,
                        int32_t dataRegionSizeColumns,
                        ECBlocks* ecBlocks);
  virtual ~CBC_DataMatrixVersion();

  static void Initialize();
  static void Finalize();
  int32_t GetVersionNumber();
  int32_t GetSymbolSizeRows();
  int32_t GetSymbolSizeColumns();
  int32_t GetDataRegionSizeRows();
  int32_t GetDataRegionSizeColumns();
  int32_t GetTotalCodewords();
  ECBlocks* GetECBlocks();
  static CBC_DataMatrixVersion* GetVersionForDimensions(int32_t numRows,
                                                        int32_t numColumns,
                                                        int32_t& e);
  static void ReleaseAll();

 private:
  int32_t m_versionNumber;
  int32_t m_symbolSizeRows;
  int32_t m_symbolSizeColumns;
  int32_t m_dataRegionSizeRows;
  int32_t m_dataRegionSizeColumns;
  ECBlocks* m_ecBlocks;
  int32_t m_totalCodewords;
  static CFX_ArrayTemplate<CBC_DataMatrixVersion*>* VERSIONS;
};

#endif  // XFA_FXBARCODE_DATAMATRIX_BC_DATAMATRIXVERSION_H_
