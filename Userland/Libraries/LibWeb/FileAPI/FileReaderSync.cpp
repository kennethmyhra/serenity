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

// https://w3c.github.io/FileAPI/#dfn-readAsArrayBufferSync
WebIDL::ExceptionOr<JS::NonnullGCPtr<JS::ArrayBuffer>> FileReaderSync::read_as_array_buffer(Blob& blob)
{
    // FIXME: 1. Let stream be the result of calling get stream on blob.

    // FIXME: 2. Let reader be the result of getting a reader from stream.

    // FIXME: 3. Let promise be the result of reading all bytes from stream with reader.

    // FIXME: 4. Wait for promise to be fulfilled or rejected.

    // FIXME: 5. If promise fulfilled with a byte sequence bytes:

    // FIXME: 1. Return the result of package data given bytes, BinaryString, and blob’s type.

    // FIXME: 6. Throw promise’s rejection reason.

    auto array_buffer = JS::ArrayBuffer::create(realm(), blob.size()).release_value();
    array_buffer->buffer().overwrite(0, blob.bytes().data(), blob.bytes().size());
    return JS::NonnullGCPtr(*array_buffer);
}

}
