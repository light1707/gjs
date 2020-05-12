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

#include <cairo-features.h>  // for CAIRO_HAS_PDF_SURFACE
#include <cairo.h>

#include <js/TypeDecls.h>

#include "gjs/jsapi-util.h"

#if CAIRO_HAS_PDF_SURFACE
#    include <cairo-pdf.h>
#    include <glib.h>

#    include <js/Class.h>
#    include <js/RootingAPI.h>
#    include <jsapi.h>  // for JS_NewObjectWithGivenProto
#    include <jspubtd.h>  // for JSProtoKey

#    include "gjs/jsapi-class.h"
#    include "gjs/jsapi-util-args.h"
#    include "modules/cairo-private.h"

namespace JS {
class CallArgs;
}

JSObject* CairoPDFSurface::new_proto(JSContext* cx, JSProtoKey) {
    JS::RootedObject parent_proto(cx, CairoSurface::prototype(cx));
    return JS_NewObjectWithGivenProto(cx, nullptr, parent_proto);
}

const js::ClassSpec CairoPDFSurface::class_spec = {
    nullptr,  // createConstructor,
    &CairoPDFSurface::new_proto,
    nullptr,  // constructorFunctions
    nullptr,  // constructorProperties
    nullptr,  // prototypeFunctions
    nullptr,  // prototypeProperties
    &CairoSurface::define_gtype_prop,
};

const JSClass CairoPDFSurface::klass = {
    "PDFSurface", JSCLASS_HAS_PRIVATE | JSCLASS_BACKGROUND_FINALIZE,
    &CairoSurface::class_ops};

cairo_surface_t* CairoPDFSurface::constructor_impl(JSContext* context,
                                                   const JS::CallArgs& argv) {
    GjsAutoChar filename;
    double width, height;
    cairo_surface_t *surface;
    if (!gjs_parse_call_args(context, "PDFSurface", argv, "Fff",
                             "filename", &filename,
                             "width", &width,
                             "height", &height))
        return nullptr;

    surface = cairo_pdf_surface_create(filename, width, height);

    if (!gjs_cairo_check_status(context, cairo_surface_status(surface),
                                "surface"))
        return nullptr;

    return surface;
}
#else
JSObject* CairoPDFSurface::from_c_ptr(JSContext* context,
                                      cairo_surface_t* surface) {
    gjs_throw(context,
        "could not create PDF surface, recompile cairo and gjs with "
        "PDF support.");
    return nullptr;
}
#endif /* CAIRO_HAS_PDF_SURFACE */
