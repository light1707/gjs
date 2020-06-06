/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2017  Philip Chimento <philip.chimento@gmail.com>
 * Copyright (c) 2020  Evan Welsh <contact@evanwelsh.com>
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

#ifndef GJS_GLOBAL_H_
#define GJS_GLOBAL_H_

#include <config.h>

#include <js/TypeDecls.h>
#include <js/Value.h>

#include "gjs/macros.h"

enum class GjsGlobalType {
    DEFAULT,
    DEBUGGER,
    INTERNAL,
};

enum class GjsGlobalSlot : uint32_t {
    GLOBAL_TYPE = 0,
    IMPORTS,
    ES_MODULE_REGISTRY,
    NATIVE_MODULE_REGISTRY,
    PROTOTYPE_gtype,
    PROTOTYPE_importer,
    PROTOTYPE_function,
    PROTOTYPE_ns,
    PROTOTYPE_repo,
    PROTOTYPE_byte_array,
    PROTOTYPE_cairo_context,
    PROTOTYPE_cairo_gradient,
    PROTOTYPE_cairo_image_surface,
    PROTOTYPE_cairo_linear_gradient,
    PROTOTYPE_cairo_path,
    PROTOTYPE_cairo_pattern,
    PROTOTYPE_cairo_pdf_surface,
    PROTOTYPE_cairo_ps_surface,
    PROTOTYPE_cairo_radial_gradient,
    PROTOTYPE_cairo_region,
    PROTOTYPE_cairo_solid_pattern,
    PROTOTYPE_cairo_surface,
    PROTOTYPE_cairo_surface_pattern,
    PROTOTYPE_cairo_svg_surface,
    LAST,
};

enum class GjsInternalGlobalSlot : uint32_t {
    MODULE_REGISTRY = static_cast<uint32_t>(GjsGlobalSlot::LAST),
    SCRIPT_REGISTRY,
    IMPORT_HOOK,
    LAST
};

bool gjs_global_is_type(JSContext* cx, GjsGlobalType type);
GjsGlobalType gjs_global_get_type(JSObject* global);

GJS_JSAPI_RETURN_CONVENTION
JSObject* gjs_create_global_object(JSContext* cx, GjsGlobalType global_type,
                                   JSObject* existing_global = nullptr);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_define_global_properties(JSContext* cx, JS::HandleObject global,
                                  GjsGlobalType global_type,
                                  const char* realm_name,
                                  const char* bootstrap_script);

template <typename GlobalSlot>
void gjs_set_global_slot(JSObject* global, GlobalSlot slot, JS::Value value);

template <typename GlobalSlot>
JS::Value gjs_get_global_slot(JSObject* global, GlobalSlot slot);

#endif  // GJS_GLOBAL_H_
