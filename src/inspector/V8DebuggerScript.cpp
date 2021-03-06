// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/V8DebuggerScript.h"

#include "src/inspector/ProtocolPlatform.h"
#include "src/inspector/StringUtil.h"

namespace v8_inspector {

static const char hexDigits[17] = "0123456789ABCDEF";

static void appendUnsignedAsHex(uint64_t number, String16Builder* destination) {
  for (size_t i = 0; i < 8; ++i) {
    UChar c = hexDigits[number & 0xF];
    destination->append(c);
    number >>= 4;
  }
}

// Hash algorithm for substrings is described in "Über die Komplexität der
// Multiplikation in
// eingeschränkten Branchingprogrammmodellen" by Woelfe.
// http://opendatastructures.org/versions/edition-0.1d/ods-java/node33.html#SECTION00832000000000000000
static String16 calculateHash(const String16& str) {
  static uint64_t prime[] = {0x3FB75161, 0xAB1F4E4F, 0x82675BC5, 0xCD924D35,
                             0x81ABE279};
  static uint64_t random[] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476,
                              0xC3D2E1F0};
  static uint32_t randomOdd[] = {0xB4663807, 0xCC322BF5, 0xD4F91BBD, 0xA7BEA11D,
                                 0x8F462907};

  uint64_t hashes[] = {0, 0, 0, 0, 0};
  uint64_t zi[] = {1, 1, 1, 1, 1};

  const size_t hashesSize = V8_INSPECTOR_ARRAY_LENGTH(hashes);

  size_t current = 0;
  const uint32_t* data = nullptr;
  size_t sizeInBytes = sizeof(UChar) * str.length();
  data = reinterpret_cast<const uint32_t*>(str.characters16());
  for (size_t i = 0; i < sizeInBytes / 4; i += 4) {
    uint32_t v = data[i];
    uint64_t xi = v * randomOdd[current] & 0x7FFFFFFF;
    hashes[current] = (hashes[current] + zi[current] * xi) % prime[current];
    zi[current] = (zi[current] * random[current]) % prime[current];
    current = current == hashesSize - 1 ? 0 : current + 1;
  }
  if (sizeInBytes % 4) {
    uint32_t v = 0;
    for (size_t i = sizeInBytes - sizeInBytes % 4; i < sizeInBytes; ++i) {
      v <<= 8;
      v |= reinterpret_cast<const uint8_t*>(data)[i];
    }
    uint64_t xi = v * randomOdd[current] & 0x7FFFFFFF;
    hashes[current] = (hashes[current] + zi[current] * xi) % prime[current];
    zi[current] = (zi[current] * random[current]) % prime[current];
    current = current == hashesSize - 1 ? 0 : current + 1;
  }

  for (size_t i = 0; i < hashesSize; ++i)
    hashes[i] = (hashes[i] + zi[i] * (prime[i] - 1)) % prime[i];

  String16Builder hash;
  for (size_t i = 0; i < hashesSize; ++i) appendUnsignedAsHex(hashes[i], &hash);
  return hash.toString();
}

V8DebuggerScript::V8DebuggerScript(v8::Isolate* isolate,
                                   v8::Local<v8::Object> object,
                                   bool isLiveEdit) {
  v8::Local<v8::Value> idValue =
      object->Get(toV8StringInternalized(isolate, "id"));
  DCHECK(!idValue.IsEmpty() && idValue->IsInt32());
  m_id = String16::fromInteger(idValue->Int32Value());

  m_url = toProtocolStringWithTypeCheck(
      object->Get(toV8StringInternalized(isolate, "name")));
  m_sourceURL = toProtocolStringWithTypeCheck(
      object->Get(toV8StringInternalized(isolate, "sourceURL")));
  m_sourceMappingURL = toProtocolStringWithTypeCheck(
      object->Get(toV8StringInternalized(isolate, "sourceMappingURL")));
  m_startLine =
      static_cast<int>(object->Get(toV8StringInternalized(isolate, "startLine"))
                           ->ToInteger(isolate)
                           ->Value());
  m_startColumn = static_cast<int>(
      object->Get(toV8StringInternalized(isolate, "startColumn"))
          ->ToInteger(isolate)
          ->Value());
  m_endLine =
      static_cast<int>(object->Get(toV8StringInternalized(isolate, "endLine"))
                           ->ToInteger(isolate)
                           ->Value());
  m_endColumn =
      static_cast<int>(object->Get(toV8StringInternalized(isolate, "endColumn"))
                           ->ToInteger(isolate)
                           ->Value());
  m_executionContextAuxData = toProtocolStringWithTypeCheck(
      object->Get(toV8StringInternalized(isolate, "executionContextAuxData")));
  m_executionContextId = static_cast<int>(
      object->Get(toV8StringInternalized(isolate, "executionContextId"))
          ->ToInteger(isolate)
          ->Value());
  m_isLiveEdit = isLiveEdit;

  v8::Local<v8::Value> sourceValue =
      object->Get(toV8StringInternalized(isolate, "source"));
  if (!sourceValue.IsEmpty() && sourceValue->IsString())
    setSource(isolate, sourceValue.As<v8::String>());
}

V8DebuggerScript::~V8DebuggerScript() {}

const String16& V8DebuggerScript::sourceURL() const {
  return m_sourceURL.isEmpty() ? m_url : m_sourceURL;
}

v8::Local<v8::String> V8DebuggerScript::source(v8::Isolate* isolate) const {
  return m_source.Get(isolate);
}

void V8DebuggerScript::setSourceURL(const String16& sourceURL) {
  m_sourceURL = sourceURL;
}

void V8DebuggerScript::setSourceMappingURL(const String16& sourceMappingURL) {
  m_sourceMappingURL = sourceMappingURL;
}

void V8DebuggerScript::setSource(v8::Isolate* isolate,
                                 v8::Local<v8::String> source) {
  m_source.Reset(isolate, source);
  m_hash = calculateHash(toProtocolString(source));
}

}  // namespace v8_inspector
