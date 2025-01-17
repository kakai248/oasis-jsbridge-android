/*
 * Copyright (C) 2019 ProSiebenSat1.Digital GmbH.
 *
 * Originally based on Duktape Android:
 * Copyright (C) 2015 Square, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "Byte.h"

#include "JsBridgeContext.h"
#include "log.h"
#include "exceptions/JniException.h"
#include "jni-helpers/JArrayLocalRef.h"

#if defined(DUKTAPE)
# include "JsBridgeContext.h"
# include "StackChecker.h"
#endif

namespace JavaTypes {

Byte::Byte(const JsBridgeContext *jsBridgeContext)
 : Primitive(jsBridgeContext, JavaTypeId::Byte, JavaTypeId::BoxedByte) {
}

#if defined(DUKTAPE)

JValue Byte::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!duk_is_number(m_ctx, -1)) {
    const auto message = std::string("Cannot convert return value ") + duk_safe_to_string(m_ctx, -1) + " to byte";
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }
  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }
  auto b = static_cast<jbyte>(duk_require_int(m_ctx, -1));
  duk_pop(m_ctx);
  return JValue(b);
}

JValue Byte::popArray(uint32_t count, bool expanded) const {
  if (!expanded) {
    count = static_cast<uint32_t>(duk_get_length(m_ctx, -1));
    if (!duk_is_array(m_ctx, -1)) {
      const auto message = std::string("Cannot convert JS value ") + duk_safe_to_string(m_ctx, -1) + " to Array<Byte>";
      duk_pop(m_ctx);  // pop the array
      throw std::invalid_argument(message);
    }
  }

  JArrayLocalRef<jbyte> byteArray(m_jniContext, count);
  jbyte *elements = byteArray.isNull() ? nullptr : byteArray.getMutableElements();
  if (elements == nullptr) {
    duk_pop_n(m_ctx, expanded ? count : 1);  // pop the expanded elements or the array
    throw JniException(m_jniContext);
  }

  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, static_cast<duk_uarridx_t>(i));
    }
    try {
      JValue value = pop();
      elements[i] = value.getByte();
    } catch (const std::exception &e) {
      if (!expanded) {
        duk_pop(m_ctx);  // pop the array
      }
      throw;
    }
  }

  if (!expanded) {
    duk_pop(m_ctx);  // pop the array
  }

  return JValue(byteArray);
}

duk_ret_t Byte::push(const JValue &value) const {
  duk_push_int(m_ctx, value.getByte());
  return 1;
}

duk_ret_t Byte::pushArray(const JniLocalRef<jarray> &values, bool expand) const {
  JArrayLocalRef<jbyte> byteArray(values);
  const auto count = byteArray.getLength();

  const jbyte *elements = byteArray.getElements();
  if (elements == nullptr) {
    throw JniException(m_jniContext);
  }

  CHECK_STACK_OFFSET(m_ctx, expand ? count : 1);

  if (!expand) {
    duk_push_array(m_ctx);
  }

  for (jsize i = 0; i < count; ++i) {
    duk_push_int(m_ctx, elements[i]);
    if (!expand) {
      duk_put_prop_index(m_ctx, -2, static_cast<duk_uarridx_t>(i));
    }
  }

  return expand ? count : 1;
}

#elif defined(QUICKJS)

namespace {
  inline jbyte getByte(JSValue v) {
    int tag = JS_VALUE_GET_TAG(v);
    if (tag == JS_TAG_INT) {
      return JS_VALUE_GET_INT(v);
    }

    if (JS_TAG_IS_FLOAT64(tag)) {
      return jbyte(JS_VALUE_GET_FLOAT64(v));
    }

    throw std::invalid_argument("Cannot convert JS value to Java byte");
  }
}

JValue Byte::toJava(JSValueConst v) const {
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  return JValue(getByte(v));
}

JValue Byte::toJavaArray(JSValueConst v) const {
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  if (!JS_IsArray(m_ctx, v)) {
    throw std::invalid_argument("Cannot convert JS value to Java array");
  }

  JSValue lengthValue = JS_GetPropertyStr(m_ctx, v, "length");
  assert(JS_IsNumber(lengthValue));
  uint32_t count = JS_VALUE_GET_INT(lengthValue);
  JS_FreeValue(m_ctx, lengthValue);

  JArrayLocalRef<jbyte> byteArray(m_jniContext, count);
  if (byteArray.isNull()) {
    throw JniException(m_jniContext);
  }

  jbyte *elements = byteArray.getMutableElements();
  if (elements == nullptr) {
    throw JniException(m_jniContext);
  }

  for (uint32_t i = 0; i < count; ++i) {
    JSValue ev = JS_GetPropertyUint32(m_ctx, v, i);
    elements[i] = getByte(ev);
  }

  byteArray.releaseArrayElements();  // copy back elements to Java
  return JValue(byteArray);
}

JSValue Byte::fromJava(const JValue &value) const {
  return JS_NewInt32(m_ctx, value.getByte());
}

JSValue Byte::fromJavaArray(const JniLocalRef<jarray> &values) const {
  JArrayLocalRef<jbyte> byteArray(values);
  const auto count = byteArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  const jbyte *elements = byteArray.getElements();
  if (elements == nullptr) {
    JS_FreeValue(m_ctx, jsArray);
    throw JniException(m_jniContext);
  }

  for (jsize i = 0; i < count; ++i) {
    JSValue elementValue = JS_NewInt32(m_ctx, elements[i]);
    JS_SetPropertyUint32(m_ctx, jsArray, static_cast<uint32_t>(i), elementValue);
  }

  return jsArray;
}

#endif

JValue Byte::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                           const std::vector<JValue> &args) const {
  jbyte returnValue = m_jniContext->callByteMethodA(javaThis, methodId, args);

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  return JValue(returnValue);
}

JValue Byte::box(const JValue &byteValue) const {
  // From byte to Byte
  static thread_local jmethodID boxId = m_jniContext->getStaticMethodID(getBoxedJavaClass(), "valueOf", "(B)Ljava/lang/Byte;");
  return JValue(m_jniContext->callStaticObjectMethod(getBoxedJavaClass(), boxId, byteValue.getByte()));
}

JValue Byte::unbox(const JValue &boxedValue) const {
  // From Byte to byte
  static thread_local jmethodID unboxId = m_jniContext->getMethodID(getBoxedJavaClass(), "byteValue", "()B");
  return JValue(m_jniContext->callByteMethod(boxedValue.getLocalRef(), unboxId));
}

}  // namespace JavaTypes

