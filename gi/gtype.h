/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2008  litl, LLC
 * Copyright (c) 2012  Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef GI_GTYPE_H_
#define GI_GTYPE_H_

#include <config.h>

#include <glib-object.h>
#include <glib.h>  // for GPOINTER_TO_SIZE

#include <js/PropertySpec.h>
#include <js/RootingAPI.h>
#include <js/TypeDecls.h>

#include "gjs/jsapi-class.h"
#include "gjs/macros.h"

class GjsAtoms;
namespace JS {
class CallArgs;
}
namespace js {
struct ClassSpec;
}
struct JSClass;

// Unfortunately, named "Type" because "GType" is already taken
class Type;
using TypeBase = NativeObject<Type, void, GJS_GLOBAL_SLOT_PROTOTYPE_gtype>;

class Type : public TypeBase {
    friend TypeBase;

    // No private data is allocated, it's stuffed directly in the private field
    // of JSObject, so nothing to free
    static void finalize_impl(JSFreeOp*, void*) {}

    static const JSClass klass;
    static const JSPropertySpec proto_props[];
    static const JSFunctionSpec proto_funcs[];
    static const js::ClassSpec class_spec;

    GJS_JSAPI_RETURN_CONVENTION
    static GType value(JSContext* cx, JS::HandleObject obj,
                       JS::CallArgs& args) {
        return GPOINTER_TO_SIZE(Type::for_js(cx, obj, args));
    }

    GJS_JSAPI_RETURN_CONVENTION
    static GType value(JSContext* cx, JS::HandleObject obj) {
        return GPOINTER_TO_SIZE(Type::for_js(cx, obj));
    }

    GJS_JSAPI_RETURN_CONVENTION
    static bool to_string(JSContext* cx, unsigned argc, JS::Value* vp);

    GJS_JSAPI_RETURN_CONVENTION
    static bool get_name(JSContext* cx, unsigned argc, JS::Value* vp);

    GJS_JSAPI_RETURN_CONVENTION
    static bool _get_actual_gtype(JSContext* cx, const GjsAtoms& atoms,
                                  JS::HandleObject object, GType* gtype_out,
                                  int recurse);

 public:
    GJS_JSAPI_RETURN_CONVENTION
    static JSObject* create(JSContext* cx, GType gtype);

    GJS_JSAPI_RETURN_CONVENTION
    static bool get_actual_gtype(JSContext* cx, JS::HandleObject object,
                                 GType* gtype_out);
};

#endif  // GI_GTYPE_H_
