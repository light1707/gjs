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

#ifndef MODULES_CAIRO_PRIVATE_H_
#define MODULES_CAIRO_PRIVATE_H_

#include <config.h>

#include <cairo-features.h>  // for CAIRO_HAS_PDF_SURFACE, CAIRO_HAS_PS_SURFACE
#include <cairo-gobject.h>
#include <cairo.h>
#include <glib-object.h>

#include <js/PropertySpec.h>
#include <js/TypeDecls.h>
#include <jspubtd.h>  // for JSProtoKey

#include "gjs/jsapi-class.h"
#include "gjs/macros.h"

namespace JS {
class CallArgs;
}
namespace js {
struct ClassSpec;
}
struct JSClass;

GJS_JSAPI_RETURN_CONVENTION
bool             gjs_cairo_check_status                 (JSContext       *context,
                                                         cairo_status_t   status,
                                                         const char      *name);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_cairo_region_define_proto(JSContext              *cx,
                                   JS::HandleObject        module,
                                   JS::MutableHandleObject proto);

void gjs_cairo_region_init(void);

class CairoContext;
using CairoContextBase = NativeObject<CairoContext, cairo_t,
                                      GJS_GLOBAL_SLOT_PROTOTYPE_cairo_context>;

class CairoContext : public CairoContextBase {
    friend CairoContextBase;

    CairoContext() = delete;
    CairoContext(CairoContext&) = delete;
    CairoContext(CairoContext&&) = delete;

    static GType gtype() { return CAIRO_GOBJECT_TYPE_CONTEXT; }

    static cairo_t* copy_ptr(cairo_t* cr) { return cairo_reference(cr); }

    GJS_JSAPI_RETURN_CONVENTION
    static cairo_t* constructor_impl(JSContext* cx, const JS::CallArgs& args);

    static void finalize_impl(JSFreeOp* fop, cairo_t* cr);

    static const JSClass klass;
    static const js::ClassSpec class_spec;
    static const JSFunctionSpec proto_funcs[];
    static const JSPropertySpec constructor_props[];
};

void gjs_cairo_context_init(void);
void gjs_cairo_surface_init(void);

/* path */

class CairoPath;
using CairoPathBase =
    NativeObject<CairoPath, cairo_path_t, GJS_GLOBAL_SLOT_PROTOTYPE_cairo_path>;

class CairoPath : public CairoPathBase {
    friend CairoPathBase;

    CairoPath() = delete;
    CairoPath(CairoPath&) = delete;
    CairoPath(CairoPath&&) = delete;

    static cairo_path_t* copy_ptr(cairo_path_t* path) { return path; }

    static void finalize_impl(JSFreeOp* fop, cairo_path_t* path);

    static const js::ClassSpec class_spec;
    static const JSClass klass;
};

/* surface */
GJS_USE
JSObject *gjs_cairo_surface_get_proto(JSContext *cx);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_cairo_surface_define_proto(JSContext              *cx,
                                    JS::HandleObject        module,
                                    JS::MutableHandleObject proto);

void gjs_cairo_surface_construct(JSObject* object, cairo_surface_t* surface);
void             gjs_cairo_surface_finalize_surface     (JSFreeOp        *fop,
                                                         JSObject        *object);
GJS_JSAPI_RETURN_CONVENTION
JSObject *       gjs_cairo_surface_from_surface         (JSContext       *context,
                                                         cairo_surface_t *surface);
GJS_JSAPI_RETURN_CONVENTION
cairo_surface_t* gjs_cairo_surface_get_surface(
    JSContext* cx, JS::HandleObject surface_wrapper);

/* image surface */
GJS_JSAPI_RETURN_CONVENTION
bool gjs_cairo_image_surface_define_proto(JSContext              *cx,
                                          JS::HandleObject        module,
                                          JS::MutableHandleObject proto);

void             gjs_cairo_image_surface_init           (JSContext       *context,
                                                         JS::HandleObject proto);

GJS_JSAPI_RETURN_CONVENTION
JSObject *       gjs_cairo_image_surface_from_surface   (JSContext       *context,
                                                         cairo_surface_t *surface);

/* postscript surface */
#ifdef CAIRO_HAS_PS_SURFACE
GJS_JSAPI_RETURN_CONVENTION
bool gjs_cairo_ps_surface_define_proto(JSContext              *cx,
                                       JS::HandleObject        module,
                                       JS::MutableHandleObject proto);
#endif
GJS_JSAPI_RETURN_CONVENTION
JSObject *       gjs_cairo_ps_surface_from_surface       (JSContext       *context,
                                                          cairo_surface_t *surface);

/* pdf surface */
#ifdef CAIRO_HAS_PDF_SURFACE
GJS_JSAPI_RETURN_CONVENTION
bool gjs_cairo_pdf_surface_define_proto(JSContext              *cx,
                                        JS::HandleObject        module,
                                        JS::MutableHandleObject proto);
#endif
GJS_JSAPI_RETURN_CONVENTION
JSObject *       gjs_cairo_pdf_surface_from_surface     (JSContext       *context,
                                                         cairo_surface_t *surface);

/* svg surface */
#ifdef CAIRO_HAS_SVG_SURFACE
GJS_JSAPI_RETURN_CONVENTION
bool gjs_cairo_svg_surface_define_proto(JSContext              *cx,
                                        JS::HandleObject        module,
                                        JS::MutableHandleObject proto);
#endif
GJS_JSAPI_RETURN_CONVENTION
JSObject *       gjs_cairo_svg_surface_from_surface     (JSContext       *context,
                                                         cairo_surface_t *surface);

/* pattern */

class CairoPattern;
using CairoPatternBase = NativeObject<CairoPattern, cairo_pattern_t,
                                      GJS_GLOBAL_SLOT_PROTOTYPE_cairo_pattern>;

class CairoPattern : public CairoPatternBase {
    friend CairoPatternBase;
    friend class CairoGradient;  // "inherits" from CairoPattern
    friend class CairoLinearGradient;
    friend class CairoRadialGradient;
    friend class CairoSurfacePattern;
    friend class CairoSolidPattern;

    CairoPattern() = delete;
    CairoPattern(CairoPattern&) = delete;
    CairoPattern(CairoPattern&&) = delete;

    static const JSFunctionSpec proto_funcs[];
    static const js::ClassSpec class_spec;
    static const JSClass klass;

    static GType gtype() { return CAIRO_GOBJECT_TYPE_PATTERN; }

    static cairo_pattern_t* copy_ptr(cairo_pattern_t* pattern) {
        return cairo_pattern_reference(pattern);
    }

 protected:
    static void finalize_impl(JSFreeOp* fop, cairo_pattern_t* pattern);

 public:
    static cairo_pattern_t* for_js(JSContext* cx,
                                   JS::HandleObject pattern_wrapper);
};

GJS_JSAPI_RETURN_CONVENTION
JSObject*        gjs_cairo_pattern_from_pattern         (JSContext       *context,
                                                         cairo_pattern_t *pattern);

class CairoGradient;
using CairoGradientBase =
    NativeObject<CairoGradient, cairo_pattern_t,
                 GJS_GLOBAL_SLOT_PROTOTYPE_cairo_gradient>;

class CairoGradient : public CairoGradientBase {
    friend CairoGradientBase;
    friend class CairoLinearGradient;  // "inherits" from CairoGradient
    friend class CairoRadialGradient;

    static const JSFunctionSpec proto_funcs[];
    static const js::ClassSpec class_spec;
    static const JSClass klass;

    static void finalize_impl(JSFreeOp*, cairo_pattern_t*) {}

    GJS_JSAPI_RETURN_CONVENTION
    static JSObject* new_proto(JSContext* cx, JSProtoKey);
};

class CairoLinearGradient;
using CairoLinearGradientBase =
    NativeObject<CairoLinearGradient, cairo_pattern_t,
                 GJS_GLOBAL_SLOT_PROTOTYPE_cairo_linear_gradient>;

class CairoLinearGradient : public CairoLinearGradientBase {
    friend CairoLinearGradientBase;

    static const JSFunctionSpec proto_funcs[];
    static const js::ClassSpec class_spec;
    static const JSClass klass;

    static cairo_pattern_t* copy_ptr(cairo_pattern_t* pattern) {
        return cairo_pattern_reference(pattern);
    }

    GJS_JSAPI_RETURN_CONVENTION
    static JSObject* new_proto(JSContext* cx, JSProtoKey);

    GJS_JSAPI_RETURN_CONVENTION
    static cairo_pattern_t* constructor_impl(JSContext* cx, const JS::CallArgs& args);

    static void finalize_impl(JSFreeOp*, cairo_pattern_t*) {}
};

class CairoRadialGradient;
using CairoRadialGradientBase =
    NativeObject<CairoRadialGradient, cairo_pattern_t,
                 GJS_GLOBAL_SLOT_PROTOTYPE_cairo_radial_gradient>;

class CairoRadialGradient : public CairoRadialGradientBase {
    friend CairoRadialGradientBase;

    static const JSFunctionSpec proto_funcs[];
    static const js::ClassSpec class_spec;
    static const JSClass klass;

    static cairo_pattern_t* copy_ptr(cairo_pattern_t* pattern) {
        return cairo_pattern_reference(pattern);
    }

    GJS_JSAPI_RETURN_CONVENTION
    static JSObject* new_proto(JSContext* cx, JSProtoKey);

    GJS_JSAPI_RETURN_CONVENTION
    static cairo_pattern_t* constructor_impl(JSContext* cx, const JS::CallArgs& args);

    static void finalize_impl(JSFreeOp*, cairo_pattern_t*) {}
};

class CairoSurfacePattern;
using CairoSurfacePatternBase =
    NativeObject<CairoSurfacePattern, cairo_pattern_t,
                 GJS_GLOBAL_SLOT_PROTOTYPE_cairo_surface_pattern>;

class CairoSurfacePattern : public CairoSurfacePatternBase {
    friend CairoSurfacePatternBase;

    static const JSFunctionSpec proto_funcs[];
    static const js::ClassSpec class_spec;
    static const JSClass klass;

    static cairo_pattern_t* copy_ptr(cairo_pattern_t* pattern) {
        return cairo_pattern_reference(pattern);
    }

    GJS_JSAPI_RETURN_CONVENTION
    static JSObject* new_proto(JSContext* cx, JSProtoKey);

    GJS_JSAPI_RETURN_CONVENTION
    static cairo_pattern_t* constructor_impl(JSContext* cx, const JS::CallArgs& args);

    static void finalize_impl(JSFreeOp*, cairo_pattern_t*) {}
};

class CairoSolidPattern;
using CairoSolidPatternBase =
    NativeObject<CairoSolidPattern, cairo_pattern_t,
                 GJS_GLOBAL_SLOT_PROTOTYPE_cairo_solid_pattern>;

class CairoSolidPattern : public CairoSolidPatternBase {
    friend CairoSolidPatternBase;

    static const JSFunctionSpec static_funcs[];
    static const js::ClassSpec class_spec;
    static const JSClass klass;

    static cairo_pattern_t* copy_ptr(cairo_pattern_t* pattern) {
        return cairo_pattern_reference(pattern);
    }

    GJS_JSAPI_RETURN_CONVENTION
    static JSObject* new_proto(JSContext* cx, JSProtoKey);

    static void finalize_impl(JSFreeOp*, cairo_pattern_t*) {}
};

#endif  // MODULES_CAIRO_PRIVATE_H_
