// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcrt/fx_stream.h"

#include <memory>
#include <utility>

#include "core/fxcrt/fileaccess_iface.h"

namespace {

class CFX_CRTFileStream final : public IFX_SeekableStream {
 public:
  CONSTRUCT_VIA_MAKE_RETAIN;

  // IFX_SeekableStream:
  FX_FILESIZE GetSize() override { return m_pFile->GetSize(); }
  bool IsEOF() override { return GetPosition() >= GetSize(); }
  FX_FILESIZE GetPosition() override { return m_pFile->GetPosition(); }
  bool ReadBlockAtOffset(pdfium::span<uint8_t> buffer,
                         FX_FILESIZE offset) override {
    return m_pFile->ReadPos(buffer.data(), buffer.size(), offset) > 0;
  }
  size_t ReadBlock(void* buffer, size_t size) override {
    return m_pFile->Read(buffer, size);
  }
  bool WriteBlockAtOffset(const void* buffer,
                          FX_FILESIZE offset,
                          size_t size) override {
    return !!m_pFile->WritePos(buffer, size, offset);
  }
  bool Flush() override { return m_pFile->Flush(); }

 private:
  explicit CFX_CRTFileStream(std::unique_ptr<FileAccessIface> pFA)
      : m_pFile(std::move(pFA)) {}
  ~CFX_CRTFileStream() override = default;

  std::unique_ptr<FileAccessIface> m_pFile;
};

}  // namespace

bool IFX_WriteStream::WriteSpan(pdfium::span<const uint8_t> data) {
  return WriteBlock(data.data(), data.size());
}

bool IFX_WriteStream::WriteString(ByteStringView str) {
  return WriteBlock(str.unterminated_c_str(), str.GetLength());
}

bool IFX_WriteStream::WriteByte(uint8_t byte) {
  return WriteBlock(&byte, 1);
}

bool IFX_WriteStream::WriteDWord(uint32_t i) {
  char buf[20] = {};
  FXSYS_itoa(i, buf, 10);
  return WriteBlock(buf, strlen(buf));
}

bool IFX_WriteStream::WriteFilesize(FX_FILESIZE size) {
  char buf[20] = {};
  FXSYS_i64toa(size, buf, 10);
  return WriteBlock(buf, strlen(buf));
}

// static
RetainPtr<IFX_SeekableReadStream> IFX_SeekableReadStream::CreateFromFilename(
    const char* filename) {
  std::unique_ptr<FileAccessIface> pFA = FileAccessIface::Create();
  if (!pFA->Open(filename))
    return nullptr;
  return pdfium::MakeRetain<CFX_CRTFileStream>(std::move(pFA));
}

bool IFX_SeekableWriteStream::WriteBlock(const void* pData, size_t size) {
  return WriteBlockAtOffset(pData, GetSize(), size);
}

bool IFX_SeekableReadStream::IsEOF() {
  return false;
}

FX_FILESIZE IFX_SeekableReadStream::GetPosition() {
  return 0;
}

size_t IFX_SeekableReadStream::ReadBlock(void* buffer, size_t size) {
  return 0;
}

bool IFX_SeekableStream::WriteBlock(const void* buffer, size_t size) {
  return WriteBlockAtOffset(buffer, GetSize(), size);
}
