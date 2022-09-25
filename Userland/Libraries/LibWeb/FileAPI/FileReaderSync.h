/*
 * Copyright (c) 2022, Kenneth Myhra <kennethmyhra@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Bindings/FileReaderSyncPrototype.h>
#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/FileAPI/Blob.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/WebIDL/ExceptionOr.h>

namespace Web::FileAPI {

class FileReaderSync : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(FileReaderSync, Bindings::PlatformObject)

public:
    virtual ~FileReaderSync() override;

    static WebIDL::ExceptionOr<JS::NonnullGCPtr<FileReaderSync>> construct_impl(JS::Realm&);

    WebIDL::ExceptionOr<JS::NonnullGCPtr<JS::ArrayBuffer>> read_as_array_buffer(Blob& blob);

private:
    explicit FileReaderSync(JS::Realm&);
};

}
