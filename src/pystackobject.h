#pragma once
#include <Python.h>
#include "values.h"

// These two classes express the Strong/Weak reference rules
// with C++ semantics

namespace pymport {

class PyStrongRef;
extern size_t active_environments;

class PyWeakRef {
    protected:
  PyObject *self;

    public:
  // Regular constructor from a raw reference returned by Python
  INLINE PyWeakRef(PyObject *v) : self(v) {
    ASSERT(self == nullptr || self->ob_refcnt > 0);
  };

  // Copy constructor from a another reference (including a strong one)
  INLINE PyWeakRef(const PyWeakRef &v) : self(v.self) {
    ASSERT(self == nullptr || self->ob_refcnt > 0);
  };

  // Compare to a raw reference
  // (mostly to check if it is null or one of the const refs such as None, True or False)
  INLINE bool operator==(const PyObject *v) const {
    ASSERT(self == nullptr || self->ob_refcnt > 0);
    return self == v;
  };

  // Compare to a raw reference
  // (mostly to check if it is null or one of the const refs such as None, True or False)
  INLINE bool operator!=(const PyObject *v) const {
    ASSERT(self == nullptr || self->ob_refcnt > 0);
    return self != v;
  };

  // Dereference
  PyObject *operator*() const {
    ASSERT(self == nullptr || self->ob_refcnt > 0);
    return self;
  };
};

class PyStrongRef : public PyWeakRef {
    public:
  // Regular constructor from a raw strong reference returned by Python
  INLINE PyStrongRef(PyObject *v) : PyWeakRef(v) {
    ASSERT(self == nullptr || self->ob_refcnt > 0);
    VERBOSE_PYOBJ(self, "StrongRef create");
  };

  // Copying is allowed only if explicit
  // (this is, in fact, a performance problem)
  explicit PyStrongRef(const PyWeakRef &v) : PyWeakRef(v) {
    VERBOSE_PYOBJ(self, "StrongRef copy/weak");
    ASSERT(self->ob_refcnt > 0);
    Py_INCREF(self);
  }

  // The previous function won't get called for a PyStrongRef
  // because the compiler will generate an implicit deleted copy constructor
  explicit PyStrongRef(const PyStrongRef &v) : PyWeakRef(v.self) {
    VERBOSE_PYOBJ(self, "StrongRef copy/strong");
    ASSERT(self->ob_refcnt > 0);
    Py_INCREF(self);
  }

  // Assignment is forbidden
  PyStrongRef &operator=(const PyStrongRef &) = delete;

  // Destructive move constructor
  INLINE PyStrongRef(PyStrongRef &&v) : PyWeakRef(v.self) {
    ASSERT(self->ob_refcnt > 0);
    v.self = nullptr;
    VERBOSE_PYOBJ(self, "StrongRef move");
  };

  // Destructive move-assignment constructor
  // Only two possible cases are valid:
  // * Assign null reference to an existing reference (unreference)
  //    When evicting dying objects from the store
  // * Assign a reference to a null reference
  //    When initializing in the PyObjectWrap constructor
  PyStrongRef &operator=(PyStrongRef &&v) {
    ASSERT(self == nullptr || v.self == nullptr);
    if (self == nullptr) {
      VERBOSE_PYOBJ(v.self, "StrongRef move-assignment");
      ASSERT(v.self->ob_refcnt > 0);
      self = v.self;
      v.self = nullptr;
    } else if (v.self == nullptr) {
      VERBOSE_PYOBJ(v.self, "StrongRef unreference");
      ASSERT(self->ob_refcnt > 0);
      Py_DECREF(self);
      self = nullptr;
    } else {
      abort();
    }
    return *this;
  };

  // This is a destructive move to plain C (to the Python C-API)
  INLINE PyObject *gift() {
    ASSERT(self->ob_refcnt > 0);
    VERBOSE_PYOBJ(self, "StrongRef gift");
    PyObject *r = self;
    self = nullptr;
    return r;
  };

  virtual ~PyStrongRef() {
#ifdef DEBUG
    if (active_environments == 0) return;
#endif
    VERBOSE_PYOBJ(self, "StrongRef delete");
    if (self != nullptr) {
      ASSERT(self->ob_refcnt > 0);
      Py_DECREF(self);
    }
  };
};

} // namespace pymport
