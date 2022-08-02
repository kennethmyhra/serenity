/*
 * Copyright (c) 2022, Kenneth Myhra <kennethmyhra@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/ArrayBuffer.h>
#include <LibWeb/FileAPI/FileReaderSync.h>

namespace Web::FileAPI {

WebIDL::ExceptionOr<JS::NonnullGCPtr<FileReaderSync>> FileReaderSync::construct_impl(JS::Realm& realm)
{
    return JS::NonnullGCPtr(*realm.heap().allocate<FileReaderSync>(realm, realm));
}

FileReaderSync::FileReaderSync(JS::Realm& realm)
    : PlatformObject(realm)
{
    set_prototype(&Bindings::cached_web_prototype(realm, "FileReaderSync"));
}

FileReaderSync::~FileReaderSync() = default;

}
