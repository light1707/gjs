// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

#ifndef GJS_INTERNAL_H_
#define GJS_INTERNAL_H_

#include <config.h>

#include <js/TypeDecls.h>
#include <jsapi.h>

#include "gjs/macros.h"

bool gjs_load_internal_module(JSContext* cx, const char* identifier);

bool gjs_internal_compile_module(JSContext* cx, unsigned argc, JS::Value* vp);

bool gjs_internal_compile_internal_module(JSContext* cx, unsigned argc,
                                          JS::Value* vp);

bool gjs_internal_global_get_registry(JSContext* cx, unsigned argc,
                                      JS::Value* vp);

bool gjs_internal_global_import_sync(JSContext* cx, unsigned argc,
                                     JS::Value* vp);

bool gjs_internal_global_set_module_hook(JSContext* cx, unsigned argc,
                                         JS::Value* vp);

bool gjs_internal_global_set_module_meta_hook(JSContext* cx, unsigned argc,
                                              JS::Value* vp);

bool gjs_internal_set_module_private(JSContext* cx, unsigned argc,
                                     JS::Value* vp);

bool gjs_internal_global_set_module_resolve_hook(JSContext* cx, unsigned argc,
                                                 JS::Value* vp);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_internal_parse_uri(JSContext* cx, unsigned argc, JS::Value* vp);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_internal_resolve_relative_resource_or_file(JSContext* cx,
                                                    unsigned argc,
                                                    JS::Value* vp);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_internal_load_resource_or_file(JSContext* cx, unsigned argc,
                                        JS::Value* vp);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_internal_load_resource_or_file_async(JSContext* cx, unsigned argc,
                                              JS::Value* vp);

GJS_JSAPI_RETURN_CONVENTION
bool gjs_internal_uri_exists(JSContext* cx, unsigned argc, JS::Value* vp);

#endif  // GJS_INTERNAL_H_
