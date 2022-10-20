#include "pymport.h"
#include "pystackobject.h"
#include "values.h"
#include <map>

using namespace Napi;
using namespace pymport;

// all *_ToJS expect a borrowed reference
Value PyObj::_ToJS(Napi::Env env, PyObject *py, NapiObjectStore &store) {
  // This is a temporary store that breaks recursion, it keeps tracks of the locally
  // created Napi::Objects for each PyObject and if one is encountered multiple times,
  // then it is replaced by the same reference
  // This also means that the recursion must use a single Napi::HandleScope because these
  // are stack-allocated local references
  auto existing = store.find(py);
  if (existing != store.end()) { return existing->second; }

  if (PyLong_Check(py)) { return Number::New(env, PyLong_AsLong(py)); }

  if (PyFloat_Check(py)) { return Number::New(env, PyFloat_AsDouble(py)); }

  if (PyList_Check(py)) { return _ToJS_List(env, py, store); }

  if (PyUnicode_Check(py)) {
    PyStackObject utf16 = PyUnicode_AsUTF16String(py);
    auto raw = PyBytes_AsString(utf16);
    return String::New(env, reinterpret_cast<char16_t *>(raw + 2), PyUnicode_GET_LENGTH(py));
  }

  if (PyDict_Check(py)) { return _ToJS_Dictionary(env, py, store); }

  if (PyTuple_Check(py)) { return _ToJS_Tuple(env, py, store); }

  if (PyModule_Check(py)) {
    auto dict = PyModule_GetDict(py);
    auto r = _ToJS(env, dict, store);
    return r;
  }

  if (PyObject_TypeCheck(py, &PyType_Type)) { return _ToJS_Class(env, py, store); }

  if (PyCallable_Check(py)) {
    // NewCallable expects a strong reference and steals it
    Py_INCREF(py);
    return NewCallable(env, py);
  }

  if (py == Py_None) { return env.Null(); }

  if (py == Py_False) { return Boolean::New(env, false); }
  if (py == Py_True) { return Boolean::New(env, true); }

  // This is our definition of a class object - it has a class and a constructor
  if (PyObject_HasAttrString(py, "__class__") && py->ob_type->tp_new != nullptr) {
    return _ToJS_ClassObject(env, py, store);
  }

  // Everything else is kept as a PyObject
  // (New expects a strong reference and steals it)
  Py_INCREF(py);
  return New(env, py);
}

Value PyObj::ToJS(Napi::Env env, PyObject *py) {
  NapiObjectStore store;
  return _ToJS(env, py, store);
}

Value PyObj::ToJS(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  return PyObj::ToJS(env, self);
}

Value PyObj::_ToJS_List(Napi::Env env, PyObject *py, NapiObjectStore &store) {
  Napi::Array r = Array::New(env);
  size_t len = PyList_Size(py);
  store.insert({py, r});

  for (size_t i = 0; i < len; i++) {
    PyObject *v = PyList_GetItem(py, i);
    Napi::Value js = _ToJS(env, v, store);
    r.Set(i, js);
  }
  return r;
}

Value PyObj::_ToJS_Dictionary(Napi::Env env, PyObject *py, NapiObjectStore &store) {
  auto obj = Object::New(env);

  PyObject *key, *value;
  Py_ssize_t pos = 0;
  store.insert({py, obj});
  while (PyDict_Next(py, &pos, &key, &value)) {
    auto jsKey = _ToJS(env, key, store);
    auto jsValue = _ToJS(env, value, store);
    obj.Set(jsKey, jsValue);
  }
  return obj;
}

Value PyObj::_ToJS_Tuple(Napi::Env env, PyObject *py, NapiObjectStore &store) {
  Napi::Array r = Array::New(env);
  size_t len = PyTuple_Size(py);
  store.insert({py, r});

  for (size_t i = 0; i < len; i++) {
    PyObject *v = PyTuple_GetItem(py, i);
    Napi::Value js = _ToJS(env, v, store);
    r.Set(i, js);
  }
  return r;
}

Value PyObj::_ToJS_Class(Napi::Env env, PyObject *py, NapiObjectStore &store) {
  Napi::Object obj = Object::New(env);
  Napi::Object prototype = Object::New(env);
  obj.Set("prototype", prototype);

  store.insert({py, obj});
  PyStackObject list = PyObject_Dir(py);
  if (list == nullptr) {
    prototype.Set("name", "Python built-in");
    return obj;
  }

  size_t len = PyList_Size(list);
  for (size_t i = 0; i < len; i++) {
    auto key = PyList_GetItem(list, i);
    THROW_IF_NULL(key);

    Py_ssize_t key_len;
    const char *key_text = PyUnicode_AsUTF8AndSize(key, &key_len);
    if (!strncmp(key_text, "__abstractmethods__", strlen("__abstractmethods__"))) {
      prototype.Set("name", "Python Abstract Class");
      return obj;
    }

    PyStackObject attr = PyObject_GetAttr(py, key);
    THROW_IF_NULL(attr);

    Napi::Value js = _ToJS(env, attr, store);
    prototype.Set(key_text, js);
  }

  return obj;
}

Value PyObj::_ToJS_ClassObject(Napi::Env env, PyObject *py, NapiObjectStore &store) {
  Napi::Object obj = Object::New(env);

  PyStackObject klass = PyObject_GetAttrString(py, "__class__");
  THROW_IF_NULL(klass);

  store.insert({py, obj});

  Napi::Value js = _ToJS(env, klass, store);
  obj.Set("__proto__", js.ToObject().Get("prototype"));

  PyStackObject list = PyObject_Dir(py);
  if (list == nullptr) {
    printf("List is null\n");
    return obj;
  }

  size_t len = PyList_Size(list);
  for (size_t i = 0; i < len; i++) {
    auto key = PyList_GetItem(list, i);
    THROW_IF_NULL(key);

    PyStackObject attr = PyObject_GetAttr(py, key);

    if (attr == nullptr || !PyCallable_Check(attr)) continue;

    Napi::Value js_key = _ToJS(env, key, store);
    Napi::Value js_value = _ToJS(env, attr, store);
    obj.Set(js_key, js_value);
  }

  return obj;
}
