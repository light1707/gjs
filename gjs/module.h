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

#ifndef GJS_MODULE_H_
#define GJS_MODULE_H_

#include <config.h>

#include <gio/gio.h>

#include <js/GCHashTable.h>
#include <js/GCVector.h>
#include <js/TypeDecls.h>
#include <jsapi.h>        // for JS_GetContextPrivate
#include <jsfriendapi.h>  // for ScriptEnvironmentPreparer

#include <string>

#include "gjs/macros.h"

class CppStringHashPolicy {
 public:
    typedef std::string Lookup;

    static js::HashNumber hash(const Lookup& l) {
        return std::hash<std::string>{}(std::string(l));
    }

    static bool match(const std::string& k, const Lookup& l) {
        return k.compare(l) == 0;
    }

    static void rekey(std::string* k, const std::string& newKey) {
        *k = newKey;
    }
};

namespace JS {
template <>
struct GCPolicy<std::string> : public IgnoreGCPolicy<std::string> {};
}  // namespace JS

using GjsModuleRegistry =
    JS::GCHashMap<std::string, JS::Heap<JSObject*>, CppStringHashPolicy,
                  js::SystemAllocPolicy>;

GJS_JSAPI_RETURN_CONVENTION
JSObject *
gjs_module_import(JSContext       *cx,
                  JS::HandleObject importer,
                  JS::HandleId     id,
                  const char      *name,
                  GFile           *file);

class GjsESModule {
    std::string m_identifier;
    std::string m_uri;
    bool m_is_internal;

 public:
    GjsESModule(std::string module_identifier, std::string module_uri,
                bool is_internal) {
        m_is_internal = is_internal;
        m_uri = module_uri;
        m_identifier = module_identifier;
    }

    GjsESModule(std::string module_identifier, std::string module_uri)
        : GjsESModule(module_identifier, module_uri, false) {}

    void setUri(std::string uri) { m_uri = uri; }

    std::string uri() { return m_uri; }

    std::string identifier() { return m_identifier; }

    bool isInternal() { return m_is_internal; }

    GJS_JSAPI_RETURN_CONVENTION
    JSObject* compile(JSContext* cx, const char* mod_text, size_t mod_len);
};

bool gjs_require_module(JSContext* js_context, unsigned argc, JS::Value* vp);

GjsModuleRegistry* gjs_get_native_module_registry(JSContext* js_context);
GjsModuleRegistry* gjs_get_esm_registry(JSContext* js_context);
GjsModuleRegistry* gjs_get_internal_module_registry(JSContext* js_context);
GjsModuleRegistry* gjs_get_internal_script_registry(JSContext* js_context);

GJS_JSAPI_RETURN_CONVENTION
JSObject* gjs_module_resolve(JSContext* cx, JS::HandleValue mod_val,
                             JS::HandleString specifier);

bool gjs_populate_module_meta(JSContext* m_cx,
                              JS::Handle<JS::Value> private_ref,
                              JS::Handle<JSObject*> meta_object);

#endif  // GJS_MODULE_H_
