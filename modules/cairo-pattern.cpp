/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* Copyright 2010 litl, LLC.
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

#include <config.h>

#include <cairo.h>
#include <glib.h>

#include <js/CallArgs.h>
#include <js/Class.h>
#include <js/PropertySpec.h>
#include <js/RootingAPI.h>
#include <js/TypeDecls.h>
#include <jsapi.h>  // for JS_GetPrivate, JS_GetClass, ...

#include "gjs/jsapi-class.h"
#include "gjs/jsapi-util.h"
#include "gjs/macros.h"
#include "modules/cairo-private.h"

const js::ClassSpec CairoPattern::class_spec = {
    nullptr,  // createConstructor
    nullptr,  // createPrototype
    nullptr,  // constructorFunctions
    nullptr,  // constructorProperties
    CairoPattern::proto_funcs,
    nullptr,  // prototypeProperties
    &CairoPattern::define_gtype_prop,
};

const JSClass CairoPattern::klass = {
    "Pattern", JSCLASS_HAS_PRIVATE | JSCLASS_BACKGROUND_FINALIZE,
    &CairoPattern::class_ops};

/**
 * CairoPattern::finalize:
 * @fop: the free op
 * @pattern: pointer to free
 *
 * Destroys the resources associated with a pattern wrapper.
 *
 * This is mainly used for subclasses. FIXME
 */
void CairoPattern::finalize_impl(JSFreeOp*, cairo_pattern_t* pattern) {
    if (!pattern)
        return;
    cairo_pattern_destroy(pattern);
}

/* Methods */

GJS_JSAPI_RETURN_CONVENTION
static bool
getType_func(JSContext *context,
             unsigned   argc,
             JS::Value *vp)
{
    GJS_GET_THIS(context, argc, vp, rec, obj);
    cairo_pattern_type_t type;

    if (argc > 1) {
        gjs_throw(context, "Pattern.getType() takes no arguments");
        return false;
    }

    cairo_pattern_t* pattern = CairoPattern::for_js(context, obj);
    if (!pattern)
        return false;

    type = cairo_pattern_get_type(pattern);

    if (!gjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return false;

    rec.rval().setInt32(type);
    return true;
}

const JSFunctionSpec CairoPattern::proto_funcs[] = {
    // getMatrix
    JS_FN("getType", getType_func, 0, 0),
    // setMatrix
    JS_FS_END};

/* Public API */

/**
 * gjs_cairo_pattern_from_pattern:
 * @context: the context
 * @pattern: cairo_pattern to attach to the object
 *
 * Constructs a pattern wrapper given cairo pattern.
 * A reference to @pattern will be taken.
 *
 */
JSObject *
gjs_cairo_pattern_from_pattern(JSContext       *context,
                               cairo_pattern_t *pattern)
{
    g_return_val_if_fail(context, nullptr);
    g_return_val_if_fail(pattern, nullptr);

    switch (cairo_pattern_get_type(pattern)) {
        case CAIRO_PATTERN_TYPE_SOLID:
            return CairoSolidPattern::from_c_ptr(context, pattern);
        case CAIRO_PATTERN_TYPE_SURFACE:
            return CairoSurfacePattern::from_c_ptr(context, pattern);
        case CAIRO_PATTERN_TYPE_LINEAR:
            return CairoLinearGradient::from_c_ptr(context, pattern);
        case CAIRO_PATTERN_TYPE_RADIAL:
            return CairoRadialGradient::from_c_ptr(context, pattern);
        case CAIRO_PATTERN_TYPE_MESH:
        case CAIRO_PATTERN_TYPE_RASTER_SOURCE:
        default:
            gjs_throw(context,
                      "failed to create pattern, unsupported pattern type %d",
                      cairo_pattern_get_type(pattern));
            return nullptr;
    }
}

/**
 * CairoPattern::for_js:
 * @cx: the context
 * @pattern_wrapper: pattern wrapper
 *
 * Returns: the pattern attached to the wrapper.
 */
cairo_pattern_t* CairoPattern::for_js(JSContext* cx,
                                      JS::HandleObject pattern_wrapper) {
    g_return_val_if_fail(cx, nullptr);
    g_return_val_if_fail(pattern_wrapper, nullptr);

    JS::RootedObject proto(cx, CairoPattern::prototype(cx));

    bool is_pattern_subclass = false;
    if (!gjs_object_in_prototype_chain(cx, proto, pattern_wrapper,
                                       &is_pattern_subclass))
        return nullptr;
    if (!is_pattern_subclass) {
        gjs_throw(cx, "Expected Cairo.Pattern but got %s",
                  JS_GetClass(pattern_wrapper)->name);
        return nullptr;
    }

    return static_cast<cairo_pattern_t*>(JS_GetPrivate(pattern_wrapper));
}
