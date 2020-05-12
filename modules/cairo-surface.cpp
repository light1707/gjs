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
#include <girepository.h>
#include <glib.h>

#include <js/CallArgs.h>
#include <js/Class.h>
#include <js/PropertySpec.h>
#include <js/RootingAPI.h>
#include <js/TypeDecls.h>
#include <js/Value.h>
#include <jsapi.h>  // for JS_GetPrivate, JS_GetClass, ...

#include "gi/arg.h"
#include "gi/foreign.h"
#include "gjs/jsapi-class.h"
#include "gjs/jsapi-util-args.h"
#include "gjs/jsapi-util.h"
#include "gjs/macros.h"
#include "modules/cairo-private.h"

const js::ClassSpec CairoSurface::class_spec = {
    nullptr,  // createConstructor
    nullptr,  // createPrototype
    nullptr,  // constructorFunctions
    nullptr,  // constructorProperties
    CairoSurface::proto_funcs,
    nullptr,  // prototypeProperties
    &CairoSurface::define_gtype_prop,
};

const JSClass CairoSurface::klass = {
    "Surface", JSCLASS_HAS_PRIVATE | JSCLASS_BACKGROUND_FINALIZE,
    &CairoSurface::class_ops};

/**
 * CairoSurface::finalize_impl:
 * @fop: the free op
 * @surface: the pointer to finalize
 *
 * Destroys the resources associated with a surface wrapper.
 *
 * This is mainly used for subclasses. FIXME
 */
void CairoSurface::finalize_impl(JSFreeOp*, cairo_surface_t* surface) {
    if (!surface)
        return;
    cairo_surface_destroy(surface);
}

/* Methods */
GJS_JSAPI_RETURN_CONVENTION
static bool
writeToPNG_func(JSContext *context,
                unsigned   argc,
                JS::Value *vp)
{
    GJS_GET_THIS(context, argc, vp, argv, obj);
    GjsAutoChar filename;

    if (!gjs_parse_call_args(context, "writeToPNG", argv, "F",
                             "filename", &filename))
        return false;

    cairo_surface_t* surface = CairoSurface::for_js(context, obj);
    if (!surface)
        return false;

    cairo_surface_write_to_png(surface, filename);
    if (!gjs_cairo_check_status(context, cairo_surface_status(surface),
                                "surface"))
        return false;
    argv.rval().setUndefined();
    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool
getType_func(JSContext *context,
             unsigned   argc,
             JS::Value *vp)
{
    GJS_GET_THIS(context, argc, vp, rec, obj);
    cairo_surface_type_t type;

    if (argc > 1) {
        gjs_throw(context, "Surface.getType() takes no arguments");
        return false;
    }

    cairo_surface_t* surface = CairoSurface::for_js(context, obj);
    if (!surface)
        return false;

    type = cairo_surface_get_type(surface);
    if (!gjs_cairo_check_status(context, cairo_surface_status(surface),
                                "surface"))
        return false;

    rec.rval().setInt32(type);
    return true;
}

const JSFunctionSpec CairoSurface::proto_funcs[] = {
    // flush
    // getContent
    // getFontOptions
    JS_FN("getType", getType_func, 0, 0),
    // markDirty
    // markDirtyRectangle
    // setDeviceOffset
    // getDeviceOffset
    // setFallbackResolution
    // getFallbackResolution
    // copyPage
    // showPage
    // hasShowTextGlyphs
    JS_FN("writeToPNG", writeToPNG_func, 0, 0), JS_FS_END};

/* Public API */

/**
 * CairoSurface::from_c_ptr:
 * @context: the context
 * @surface: cairo_surface to attach to the object
 *
 * Constructs a surface wrapper given cairo surface.
 * A reference to @surface will be taken.
 *
 */
JSObject* CairoSurface::from_c_ptr(JSContext* context,
                                   cairo_surface_t* surface) {
    g_return_val_if_fail(context, nullptr);
    g_return_val_if_fail(surface, nullptr);

    cairo_surface_type_t type = cairo_surface_get_type(surface);
    if (type == CAIRO_SURFACE_TYPE_IMAGE)
        return CairoImageSurface::from_c_ptr(context, surface);
    if (type == CAIRO_SURFACE_TYPE_PDF)
        return CairoPDFSurface::from_c_ptr(context, surface);
    if (type == CAIRO_SURFACE_TYPE_PS)
        return CairoPSSurface::from_c_ptr(context, surface);
    if (type == CAIRO_SURFACE_TYPE_SVG)
        return CairoSVGSurface::from_c_ptr(context, surface);
    return CairoSurfaceBase::from_c_ptr(context, surface);
}

/**
 * CairoSurface::for_js:
 * @cx: the context
 * @surface_wrapper: surface wrapper
 *
 * Overrides NativeObject::for_js().
 *
 * Returns: the surface attached to the wrapper.
 */
cairo_surface_t* CairoSurface::for_js(JSContext* cx,
                                      JS::HandleObject surface_wrapper) {
    g_return_val_if_fail(cx, nullptr);
    g_return_val_if_fail(surface_wrapper, nullptr);

    JS::RootedObject proto(cx, CairoSurface::prototype(cx));

    bool is_surface_subclass = false;
    if (!gjs_object_in_prototype_chain(cx, proto, surface_wrapper,
                                       &is_surface_subclass))
        return nullptr;
    if (!is_surface_subclass) {
        gjs_throw(cx, "Expected Cairo.Surface but got %s",
                  JS_GetClass(surface_wrapper)->name);
        return nullptr;
    }

    return static_cast<cairo_surface_t*>(JS_GetPrivate(surface_wrapper));
}

GJS_USE
static bool
surface_to_g_argument(JSContext      *context,
                      JS::Value       value,
                      const char     *arg_name,
                      GjsArgumentType argument_type,
                      GITransfer      transfer,
                      bool            may_be_null,
                      GArgument      *arg)
{
    if (value.isNull()) {
        if (!may_be_null) {
            GjsAutoChar display_name =
                gjs_argument_display_name(arg_name, argument_type);
            gjs_throw(context, "%s may not be null", display_name.get());
            return false;
        }

        arg->v_pointer = nullptr;
        return true;
    }

    if (!value.isObject()) {
        GjsAutoChar display_name =
            gjs_argument_display_name(arg_name, argument_type);
        gjs_throw(context, "%s is not a Cairo.Surface", display_name.get());
        return false;
    }

    JS::RootedObject surface_wrapper(context, &value.toObject());
    cairo_surface_t* s = CairoSurface::for_js(context, surface_wrapper);
    if (!s)
        return false;
    if (transfer == GI_TRANSFER_EVERYTHING)
        cairo_surface_destroy(s);

    arg->v_pointer = s;
    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool surface_from_g_argument(JSContext* cx,
                                    JS::MutableHandleValue value_p,
                                    GIArgument* arg) {
    JSObject* obj = CairoSurface::from_c_ptr(
        cx, static_cast<cairo_surface_t*>(arg->v_pointer));
    if (!obj)
        return false;

    value_p.setObject(*obj);
    return true;
}

static bool surface_release_argument(JSContext*, GITransfer transfer,
                                     GIArgument* arg) {
    if (transfer != GI_TRANSFER_NOTHING)
        cairo_surface_destroy(static_cast<cairo_surface_t*>(arg->v_pointer));
    return true;
}

static GjsForeignInfo foreign_info = {
    surface_to_g_argument,
    surface_from_g_argument,
    surface_release_argument
};

void gjs_cairo_surface_init(void) {
    gjs_struct_foreign_register("cairo", "Surface", &foreign_info);
}
