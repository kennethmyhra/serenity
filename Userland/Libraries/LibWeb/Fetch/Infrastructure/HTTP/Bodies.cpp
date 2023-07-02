/*
 * Copyright (c) 2022-2023, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/PromiseCapability.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/Fetch/BodyInit.h>
#include <LibWeb/Fetch/Infrastructure/HTTP/Bodies.h>
#include <LibWeb/Fetch/Infrastructure/Task.h>
#include <LibWeb/Streams/AbstractOperations.h>
#include <LibWeb/Streams/ReadableStreamDefaultReader.h>
#include <LibWeb/WebIDL/Promise.h>

namespace Web::Fetch::Infrastructure {

JS_DEFINE_ALLOCATOR(Body);

JS::NonnullGCPtr<Body> Body::create(JS::VM& vm, JS::NonnullGCPtr<Streams::ReadableStream> stream)
{
    return vm.heap().allocate_without_realm<Body>(stream);
}

JS::NonnullGCPtr<Body> Body::create(JS::VM& vm, JS::NonnullGCPtr<Streams::ReadableStream> stream, SourceType source, Optional<u64> length)
{
    return vm.heap().allocate_without_realm<Body>(stream, source, length);
}

Body::Body(JS::NonnullGCPtr<Streams::ReadableStream> stream)
    : m_stream(move(stream))
{
}

Body::Body(JS::NonnullGCPtr<Streams::ReadableStream> stream, SourceType source, Optional<u64> length)
    : m_stream(move(stream))
    , m_source(move(source))
    , m_length(move(length))
{
}

void Body::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_stream);
}

// https://fetch.spec.whatwg.org/#concept-body-clone
JS::NonnullGCPtr<Body> Body::clone(JS::Realm& realm) const
{
    // To clone a body body, run these steps:
    // FIXME: 1. Let « out1, out2 » be the result of teeing body’s stream.
    // FIXME: 2. Set body’s stream to out1.
    auto out2 = realm.heap().allocate<Streams::ReadableStream>(realm, realm);

    // 3. Return a body whose stream is out2 and other members are copied from body.
    return Body::create(realm.vm(), out2, m_source, m_length);
}

// https://fetch.spec.whatwg.org/#body-fully-read
WebIDL::ExceptionOr<void> Body::fully_read(JS::Realm& realm, Web::Fetch::Infrastructure::Body::ProcessBodyCallback process_body, Web::Fetch::Infrastructure::Body::ProcessBodyErrorCallback process_body_error, TaskDestination task_destination) const
{
    auto& vm = realm.vm();

    // FIXME: 1. If taskDestination is null, then set taskDestination to the result of starting a new parallel queue.
    // FIXME: Handle 'parallel queue' task destination
    VERIFY(!task_destination.has<Empty>());
    auto task_destination_object = task_destination.get<JS::NonnullGCPtr<JS::Object>>();

    // 2. Let successSteps given a byte sequence bytes be to queue a fetch task to run processBody given bytes, with taskDestination.
    auto success_steps = [process_body = move(process_body), task_destination_object = JS::make_handle(task_destination_object)](ByteBuffer const& bytes) mutable -> ErrorOr<void> {
        // Make a copy of the bytes, as the source of the bytes may disappear between the time the task is queued and executed.
        auto bytes_copy = TRY(ByteBuffer::copy(bytes));
        queue_fetch_task(*task_destination_object, [process_body = move(process_body), bytes_copy = move(bytes_copy)]() {
            process_body(move(bytes_copy));
        });
        return {};
    };

    // 3. Let errorSteps optionally given an exception exception be to queue a fetch task to run processBodyError given exception, with taskDestination.
    auto error_steps = [process_body_error = move(process_body_error), task_destination_object = JS::make_handle(task_destination_object)](JS::GCPtr<WebIDL::DOMException> exception) mutable {
        queue_fetch_task(*task_destination_object, [process_body_error = move(process_body_error), exception = JS::make_handle(exception)]() {
            process_body_error(*exception);
        });
    };

    // 4. Let reader be the result of getting a reader for body’s stream. If that threw an exception, then run errorSteps with that exception and return.
    // 5. Read all bytes from reader, given successSteps and errorSteps.
    // FIXME: Implement the streams spec - this is completely made up for now :^)
    if (auto const* byte_buffer = m_source.get_pointer<ByteBuffer>()) {
        TRY_OR_THROW_OOM(vm, success_steps(*byte_buffer));
    } else if (auto const* blob_handle = m_source.get_pointer<JS::Handle<FileAPI::Blob>>()) {
        auto byte_buffer = TRY_OR_THROW_OOM(vm, ByteBuffer::copy((*blob_handle)->bytes()));
        TRY_OR_THROW_OOM(vm, success_steps(move(byte_buffer)));
    } else {
        // Empty, Blob, FormData
        error_steps(WebIDL::DOMException::create(realm, "DOMException"_fly_string, "Reading from Blob, FormData or null source is not yet implemented"_fly_string));
    }
    return {};
}

// https://fetch.spec.whatwg.org/#body-incrementally-read
WebIDL::ExceptionOr<void> Body::incrementally_read(ProcessBodyChunkCallback process_body_chunk, ProcessEndOfBodyCallback process_end_of_body, ProcessBodyErrorCallback process_body_error, TaskDestination task_destination) const
{
    // FIXME: 1. If taskDestination is null, then set taskDestination to the result of starting a new parallel queue.
    // FIXME: Handle 'parallel queue' task destination

    // 2. Let reader be the result of getting a reader for body’s stream.
    // NOTE: This operation will not throw an exception. We still handle any allocation errors though.
    auto reader = TRY(stream()->get_reader());
    VERIFY(reader.has<JS::NonnullGCPtr<Streams::ReadableStreamDefaultReader>>());

    // 3. Perform the incrementally-read loop given reader, taskDestination, processBodyChunk, processEndOfBody, and processBodyError.
    TRY(incrementally_read_loop(reader.get<JS::NonnullGCPtr<Streams::ReadableStreamDefaultReader>>(), move(task_destination), move(process_body_chunk), move(process_end_of_body), move(process_body_error)));

    return {};
}

// https://fetch.spec.whatwg.org/#incrementally-read-loop
WebIDL::ExceptionOr<void> Body::incrementally_read_loop(Streams::ReadableStreamDefaultReader& reader, TaskDestination task_destination, ProcessBodyChunkCallback process_body_chunk, ProcessEndOfBodyCallback process_end_of_body, ProcessBodyErrorCallback process_body_error) const
{
    // 1. Let readRequest be the following read request:
    auto read_request = adopt_ref(*new IncrementalReadLoopReadRequest(*this, reader, move(task_destination), move(process_body_chunk), move(process_end_of_body), move(process_body_error)));

    // 2. Read a chunk from reader given readRequest.
    TRY(Streams::readable_stream_default_reader_read(reader, read_request));

    return {};
}

// https://fetch.spec.whatwg.org/#byte-sequence-as-a-body
WebIDL::ExceptionOr<JS::NonnullGCPtr<Body>> byte_sequence_as_body(JS::Realm& realm, ReadonlyBytes bytes)
{
    // To get a byte sequence bytes as a body, return the body of the result of safely extracting bytes.
    auto [body, _] = TRY(safely_extract_body(realm, bytes));
    return body;
}

IncrementalReadLoopReadRequest::IncrementalReadLoopReadRequest(Body const& body, Streams::ReadableStreamDefaultReader& reader, TaskDestination task_destination, Body::ProcessBodyChunkCallback process_body_chunk, Body::ProcessEndOfBodyCallback process_end_of_body, Body::ProcessBodyErrorCallback process_body_error)
    : m_body(body)
    , m_reader(reader)
    , m_task_destination(move(task_destination))
    , m_process_body_chunk(move(process_body_chunk))
    , m_process_end_of_body(move(process_end_of_body))
    , m_process_body_error(move(process_body_error))
{
}

void IncrementalReadLoopReadRequest::on_chunk(JS::Value chunk)
{
    auto& realm = m_reader.realm();
    // 1. Let continueAlgorithm be null.
    JS::SafeFunction<void()> continue_algorithm = nullptr;

    // 2. If chunk is not a Uint8Array object, then set continueAlgorithm to this step: run processBodyError given a TypeError.
    if (!chunk.is_object() || !is<JS::Uint8Array>(chunk.as_object())) {
        continue_algorithm = [&] {
            m_process_body_error(WebIDL::DOMException::create(realm, "TypeError"_fly_string, "Chunk is not a Uint8Array"_fly_string));
        };
    }
    // 3. Otherwise:
    else {
        // 1. Let bytes be a copy of chunk.
        // NOTE: Implementations are strongly encouraged to use an implementation strategy that avoids this copy where possible.
        auto& uint8_array = static_cast<JS::Uint8Array&>(chunk.as_object());
        dbgln("uint8_array.byte_length: {}", uint8_array.byte_length());
        auto bytes = ByteBuffer::copy(uint8_array.data().data(), uint8_array.byte_length()).release_value_but_fixme_should_propagate_errors();
        dbgln("ByteBuffer::copy() returned {} bytes", bytes.size());
        // 2. Set continueAlgorithm to these steps:
        continue_algorithm = [&] {
            // 1. Run processBodyChunk given bytes.
            m_process_body_chunk(move(bytes));
            // 2. Perform the incrementally-read loop given reader, taskDestination, processBodyChunk, processEndOfBody, and processBodyError.
            m_body.incrementally_read_loop(m_reader, move(m_task_destination), move(m_process_body_chunk), move(m_process_end_of_body), move(m_process_body_error)).release_value_but_fixme_should_propagate_errors();
        };
    }

    // 4. Queue a fetch task given continueAlgorithm and taskDestination.
    Fetch::Infrastructure::queue_fetch_task(m_task_destination.get<JS::NonnullGCPtr<JS::Object>>(), move(continue_algorithm));
}

void IncrementalReadLoopReadRequest::on_close()
{
    // 1. Queue a fetch task given processEndOfBody and taskDestination.
    Fetch::Infrastructure::queue_fetch_task(m_task_destination.get<JS::NonnullGCPtr<JS::Object>>(), move(m_process_end_of_body));
}

void IncrementalReadLoopReadRequest::on_error(JS::Value error)
{
    // 1. Queue a fetch task to run processBodyError given e, with taskDestination.
    Fetch::Infrastructure::queue_fetch_task(m_task_destination.get<JS::NonnullGCPtr<JS::Object>>(), [this, &error]() mutable {
        VERIFY(is<WebIDL::DOMException>(error.as_object()));
        m_process_body_error(verify_cast<WebIDL::DOMException>(error.as_object()));
    });
}

}
