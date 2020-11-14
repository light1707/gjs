/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2017 Philip Chimento <philip.chimento@gmail.com>
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

#ifndef GJS_MODULE_H_
#define GJS_MODULE_H_

#include <config.h>

#include <gio/gio.h>

#include <js/TypeDecls.h>

#include "gjs/macros.h"

GJS_JSAPI_RETURN_CONVENTION
JSObject *
gjs_module_import(JSContext       *cx,
                  JS::HandleObject importer,
                  JS::HandleId     id,
                  const char      *name,
                  GFile           *file);

GJS_JSAPI_RETURN_CONVENTION
JSObject* gjs_get_native_registry(JSContext* cx, JSObject* global);

GJS_JSAPI_RETURN_CONVENTION
JSObject* gjs_get_module_registry(JSContext* cx, JSObject* global);

GJS_JSAPI_RETURN_CONVENTION
JSObject* gjs_module_load(JSContext* cx, const char* identifier,
                          const char* uri);

GJS_JSAPI_RETURN_CONVENTION
JSObject* gjs_module_resolve(JSContext* cx, JS::HandleValue mod_val,
                             JS::HandleString specifier);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_dynamic_module_resolve(JSContext* cx,
                                JS::Handle<JS::Value> aReferencingPrivate,
                                JS::Handle<JSString*> aSpecifier,
                                JS::Handle<JSObject*> aPromise);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_populate_module_meta(JSContext* m_cx,
                              JS::Handle<JS::Value> private_ref,
                              JS::Handle<JSObject*> meta_object);

#endif  // GJS_MODULE_H_
