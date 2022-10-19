#include "pymport.h"
#include "pystackobject.h"
#include "values.h"

using namespace Napi;
using namespace pymport;

Value PyObj::String(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  auto raw = NAPI_ARG_STRING(0).Utf16Value();
  auto obj = PyUnicode_DecodeUTF16(reinterpret_cast<const char *>(raw.c_str()), raw.size() * 2, nullptr, nullptr);
  return New(env, obj);
}

Value PyObj::Float(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  auto raw = NAPI_ARG_NUMBER(0).DoubleValue();
  auto obj = PyFloat_FromDouble(raw);
  return New(env, obj);
}

Value PyObj::Integer(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  auto raw = NAPI_ARG_NUMBER(0).Int64Value();
  auto obj = PyLong_FromLong(raw);
  return New(env, obj);
}

// Returns a strong reference
PyObject *PyObj::_Dictionary(Napi::Object object, PyObject *target, PyObjectStore &store) {
  Napi::Env env = object.Env();

  for (auto const &el : object.GetPropertyNames()) {
    auto key = ((Napi::Value)el.second).ToString().Utf8Value();
    auto js = object.Get(key);
    PyStackObject item = _FromJS(js, store);
    THROW_IF_NULL(item);
    // This is the only Py***_Set that does not steal a reference
    PyDict_SetItemString(target, key.c_str(), item);
  }

  return target;
}

Value PyObj::Dictionary(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  auto raw = NAPI_ARG_OBJECT(0);
  auto dict = PyDict_New();
  THROW_IF_NULL(dict);
  PyObjectStore store;
  _Dictionary(raw, dict, store);

  return New(env, dict);
}

// Returns a strong reference
PyObject *PyObj::_List(Napi::Array array, PyObject *target, PyObjectStore &store) {
  size_t len = array.Length();

  for (size_t i = 0; i < len; i++) {
    auto el = _FromJS(array.Get(i), store);
    PyList_SetItem(target, i, el);
  }

  return target;
}

Value PyObj::List(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  auto raw = NAPI_ARG_ARRAY(0);
  auto list = PyList_New(raw.Length());
  THROW_IF_NULL(list);
  PyObjectStore store;
  _List(raw, list, store);

  return New(env, list);
}

// Returns a strong reference
PyObject *PyObj::_Tuple(Napi::Array array, PyObject *target, PyObjectStore &store) {
  size_t len = array.Length();

  for (size_t i = 0; i < len; i++) {
    auto el = _FromJS(array.Get(i), store);
    PyTuple_SetItem(target, i, el);
  }

  return target;
}

Value PyObj::Tuple(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  auto raw = NAPI_ARG_ARRAY(0);
  auto tuple = PyTuple_New(raw.Length());
  THROW_IF_NULL(tuple);
  PyObjectStore store;
  _Tuple(raw, tuple, store);

  return New(env, tuple);
}

Value PyObj::FromJS(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) throw Error::New(env, "Missing argument");
  return New(info.Env(), FromJS(info[0]));
}

// Returns a strong reference
PyObject *PyObj::FromJS(Napi::Value v) {
  PyObjectStore store;
  return _FromJS(v, store);
}

// Returns a strong reference
PyObject *PyObj::_FromJS(Napi::Value v, PyObjectStore &store) {
  Napi::Env env = v.Env();

  // Break recursion on circular references
  // Alas, Napi::Values must be individually compared using
  // the provided operator==...
  for (const auto &i : store) {
    if (i.first == v) {
      // We must return a strong reference
      Py_INCREF(i.second);
      return i.second;
    }
  }

  if (v.IsNumber()) {
    auto raw = v.ToNumber().DoubleValue();
    double integer;
    double fract = fabs(modf(raw, &integer));
    if (fract < std::numeric_limits<float>::epsilon() || fract > 1 - std::numeric_limits<float>::epsilon())
      return PyLong_FromLong(v.ToNumber().Int64Value());
    else
      return PyFloat_FromDouble(raw);
  }
  if (v.IsString()) {
    auto raw = v.ToString().Utf16Value();
    auto py = PyUnicode_DecodeUTF16(reinterpret_cast<const char *>(raw.c_str()), raw.size() * 2, nullptr, nullptr);
    store.push_front({v, py});
    return py;
  }
  if (v.IsArray()) {
    auto array = v.As<Array>();
    auto list = PyList_New(array.Length());
    THROW_IF_NULL(list);
    store.push_front({v, list});
    _List(array, list, store);
    return list;
  }
  if (v.IsObject()) {
    FunctionReference *cons = env.GetInstanceData<EnvContext>()->pyObj;
    auto obj = v.ToObject();
    if (obj.ToObject().InstanceOf(cons->Value())) {
      auto py = ObjectWrap::Unwrap(v.ToObject());
      // We must return a strong reference
      Py_INCREF(py->self);
      return py->self;
    }

    auto dict = PyDict_New();
    THROW_IF_NULL(dict);
    store.push_front({v, dict});
    _Dictionary(obj, dict, store);
    return dict;
  }
  if (v.IsNull() || v.IsUndefined()) { return Py_None; }

  return nullptr;
}
