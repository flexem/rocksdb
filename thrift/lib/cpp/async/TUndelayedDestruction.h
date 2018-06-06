/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#ifndef THRIFT_ASYNC_TUNDELAYEDDESTRUCTION_H_
#define THRIFT_ASYNC_TUNDELAYEDDESTRUCTION_H_ 1

#include <cstdlib>
#include <type_traits>
#include <utility>
#include <cassert>

namespace apache { namespace thrift { namespace async {

/**
 * A helper class to allow a TDelayedDestruction object to be instantiated on
 * the stack.
 *
 * This class derives from an existing TDelayedDestruction type and makes the
 * destructor public again.  This allows objects of this type to be declared on
 * the stack or directly inside another class.  Normally TDelayedDestruction
 * objects must be dynamically allocated on the heap.
 *
 * However, the trade-off is that you lose some of the protections provided by
 * TDelayedDestruction::destroy().  TDelayedDestruction::destroy() will
 * automatically delay destruction of the object until it is safe to do so.
 * If you use TUndelayedDestruction, you become responsible for ensuring that
 * you only destroy the object where it is safe to do so.  Attempting to
 * destroy a TUndelayedDestruction object while it has a non-zero destructor
 * guard count will abort the program.
 */
template<typename TDD>
class TUndelayedDestruction : public TDD {
 public:
  // We want to expose all constructors provided by the parent class.
  // C++11 adds constructor inheritance to support this.  Unfortunately gcc
  // does not implement constructor inheritance yet, so we have to fake it with
  // variadic templates.
#if THRIFT_HAVE_CONSTRUCTOR_INHERITANCE
  using TDD::TDD;
#else
  // We unfortunately can't simulate constructor inheritance as well as I'd
  // like.
  //
  // Ideally we would use std::enable_if<> and std::is_constructible<> to
  // provide only constructor methods that are valid for our parent class.
  // Unfortunately std::is_constructible<> doesn't work for types that aren't
  // destructible.  In gcc-4.6 it results in a compiler error.  In the latest
  // gcc code it looks like it has been fixed to return false.  (The language
  // in the standard seems to indicate that returning false is the correct
  // behavior for non-destructible types, which is unfortunate.)
  template<typename ...Args>
  explicit TUndelayedDestruction(Args&& ...args)
    : TDD(std::forward<Args>(args)...) {}
#endif

  /**
   * Public destructor.
   *
   * The caller is responsible for ensuring that the object is only destroyed
   * where it is safe to do so.  (i.e., when the destructor guard count is 0).
   *
   * The exact conditions for meeting this may be dependant upon your class
   * semantics.  Typically you are only guaranteed that it is safe to destroy
   * the object directly from the event loop (e.g., directly from a
   * TEventBase::LoopCallback), or when the event loop is stopped.
   */
  virtual ~TUndelayedDestruction() {
    // Crash if the caller is destroying us with outstanding destructor guards.
    if (this->getDestructorGuardCount() != 0) {
      abort();
    }
    // Invoke destroy.  This is necessary since our base class may have
    // implemented custom behavior in destroy().
    this->destroy();
  }

 protected:
  /**
   * Override our parent's destroy() method to make it protected.
   * Callers should use the normal destructor instead of destroy
   */
  virtual void destroy() {
    this->TDD::destroy();
  }

  virtual void destroyNow(bool delayed) {
    // Do nothing.  This will always be invoked from the call to destroy inside
    // our destructor.
    assert(!delayed);
    // prevent unused variable warnings when asserts are compiled out.
    (void)delayed;
  }

 private:
  // Forbidden copy constructor and assignment operator
  TUndelayedDestruction(TUndelayedDestruction const &) = delete;
  TUndelayedDestruction& operator=(TUndelayedDestruction const &) = delete;
};

}}} // apache::thrift::async

#endif // THRIFT_ASYNC_TUNDELAYEDDESTRUCTION_H_