/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StringBuilder.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/HTML/HTMLButtonElement.h>
#include <LibWeb/HTML/HTMLDataListElement.h>
#include <LibWeb/HTML/HTMLFieldSetElement.h>
#include <LibWeb/HTML/HTMLFormElement.h>
#include <LibWeb/HTML/HTMLInputElement.h>
#include <LibWeb/HTML/HTMLObjectElement.h>
#include <LibWeb/HTML/HTMLOptionElement.h>
#include <LibWeb/HTML/HTMLOutputElement.h>
#include <LibWeb/HTML/HTMLSelectElement.h>
#include <LibWeb/HTML/HTMLTextAreaElement.h>
#include <LibWeb/HTML/SubmitEvent.h>
#include <LibWeb/Page/Page.h>
#include <LibWeb/URL/URL.h>

namespace Web::HTML {

HTMLFormElement::HTMLFormElement(DOM::Document& document, DOM::QualifiedName qualified_name)
    : HTMLElement(document, move(qualified_name))
{
    set_prototype(&Bindings::cached_web_prototype(realm(), "HTMLFormElement"));
}

HTMLFormElement::~HTMLFormElement() = default;

void HTMLFormElement::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_elements);
    for (auto& element : m_associated_elements)
        visitor.visit(element.ptr());
}

void HTMLFormElement::submit_form(JS::GCPtr<HTMLElement> submitter, bool from_submit_binding)
{
    if (cannot_navigate())
        return;

    if (action().is_null()) {
        dbgln("Unsupported form action ''");
        return;
    }

    auto effective_method = method().to_lowercase();

    if (effective_method == "dialog") {
        dbgln("Failed to submit form: Unsupported form method '{}'", method());
        return;
    }

    if (effective_method != "get" && effective_method != "post") {
        effective_method = "get";
    }

    if (!from_submit_binding) {
        if (m_firing_submission_events)
            return;

        m_firing_submission_events = true;

        // FIXME: If the submitter element's no-validate state is false...

        JS::GCPtr<HTMLElement> submitter_button;

        if (submitter != this)
            submitter_button = submitter;

        SubmitEventInit event_init {};
        event_init.submitter = submitter_button;
        auto submit_event = SubmitEvent::create(realm(), EventNames::submit, event_init);
        submit_event->set_bubbles(true);
        submit_event->set_cancelable(true);
        bool continue_ = dispatch_event(*submit_event);

        m_firing_submission_events = false;

        if (!continue_)
            return;

        // This is checked again because arbitrary JS may have run when handling submit,
        // which may have changed the result.
        if (cannot_navigate())
            return;
    }

    AK::URL url(document().parse_url(action()));

    if (!url.is_valid()) {
        dbgln("Failed to submit form: Invalid URL: {}", action());
        return;
    }

    if (url.scheme() == "file") {
        if (document().url().scheme() != "file") {
            dbgln("Failed to submit form: Security violation: {} may not submit to {}", document().url(), url);
            return;
        }
        if (effective_method != "get") {
            dbgln("Failed to submit form: Unsupported form method '{}' for URL: {}", method(), url);
            return;
        }
    } else if (url.scheme() != "http" && url.scheme() != "https") {
        dbgln("Failed to submit form: Unsupported protocol for URL: {}", url);
        return;
    }

    Vector<URL::QueryParam> parameters;

    for_each_in_inclusive_subtree_of_type<HTMLInputElement>([&](auto& input) {
        if (!input.name().is_null() && (input.type() != "submit" || &input == submitter))
            parameters.append({ input.name(), input.value() });
        return IterationDecision::Continue;
    });

    if (effective_method == "get") {
        url.set_query(url_encode(parameters, AK::URL::PercentEncodeSet::ApplicationXWWWFormUrlencoded));
    }

    LoadRequest request = LoadRequest::create_for_url_on_page(url, document().page());

    if (effective_method == "post") {
        auto body = url_encode(parameters, AK::URL::PercentEncodeSet::ApplicationXWWWFormUrlencoded).to_byte_buffer();
        request.set_method("POST");
        request.set_header("Content-Type", "application/x-www-form-urlencoded");
        request.set_body(move(body));
    }

    if (auto* page = document().page())
        page->load(request);
}

void HTMLFormElement::submit()
{
    submit_form(this, true);
}

void HTMLFormElement::add_associated_element(Badge<FormAssociatedElement>, HTMLElement& element)
{
    m_associated_elements.append(element);
}

void HTMLFormElement::remove_associated_element(Badge<FormAssociatedElement>, HTMLElement& element)
{
    m_associated_elements.remove_first_matching([&](auto& entry) { return entry.ptr() == &element; });
}

// https://html.spec.whatwg.org/#dom-fs-action
String HTMLFormElement::action() const
{
    auto value = attribute(HTML::AttributeNames::action);

    // Return the current URL if the action attribute is null or an empty string
    if (value.is_null() || value.is_empty()) {
        return document().url().to_string();
    }

    return value;
}

static bool is_form_control(DOM::Element const& element)
{
    if (is<HTMLButtonElement>(element)
        || is<HTMLFieldSetElement>(element)
        || is<HTMLObjectElement>(element)
        || is<HTMLOutputElement>(element)
        || is<HTMLSelectElement>(element)
        || is<HTMLTextAreaElement>(element)) {
        return true;
    }

    if (is<HTMLInputElement>(element)
        && !element.get_attribute(HTML::AttributeNames::type).equals_ignoring_case("image"sv)) {
        return true;
    }

    return false;
}

// https://html.spec.whatwg.org/multipage/forms.html#dom-form-elements
JS::NonnullGCPtr<DOM::HTMLCollection> HTMLFormElement::elements() const
{
    if (!m_elements) {
        m_elements = DOM::HTMLCollection::create(const_cast<HTMLFormElement&>(*this), [](Element const& element) {
            return is_form_control(element);
        });
    }
    return *m_elements;
}

// https://html.spec.whatwg.org/multipage/forms.html#dom-form-length
unsigned HTMLFormElement::length() const
{
    // The length IDL attribute must return the number of nodes represented by the elements collection.
    return elements()->length();
}

// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#constructing-the-form-data-set
Optional<HashMap<String, Vector<XHR::FormDataEntryValue>>> HTMLFormElement::construct_entry_list()
{
    auto& realm = this->realm();
    // 1. If form's constructing entry list is true, then return null.
    if (constructing_entry_list())
        return Optional<HashMap<String, Vector<XHR::FormDataEntryValue>>> {};

    // 2. Set form's constructing entry list to true.
    set_constructing_entry_list(true);
    ScopeGuard scope_guard([&] {
        set_constructing_entry_list(false);
    });

    // 3. Let controls be a list of all the submittable elements whose form owner is form, in tree order.
    auto controls = get_submittable_elements();

    // 4. Let entry list be a new empty entry list.
    HashMap<String, Vector<XHR::FormDataEntryValue>> entry_list = {};

    // 5. For each element field in controls, in tree order:
    for (auto control : controls) {
        // 1. If any of the following is true, then continue:
        // The field element has a datalist element ancestor.
        if (control->first_ancestor_of_type<HTML::HTMLDataListElement>())
            continue;
        // The field element is disabled.
        if (control->is_actually_disabled())
            continue;
        // The field element is a button but it is not submitter.
        if (is<HTML::HTMLButtonElement>(control.ptr())) {
            auto* button_element = dynamic_cast<HTML::HTMLButtonElement*>(control.ptr());
            if (button_element->type() != "submit")
                continue;
        }
        // The field element is an input element whose type attribute is in the Checkbox state and whose checkedness is false.
        // The field element is an input element whose type attribute is in the Radio Button state and whose checkedness is false.
        if (is<HTML::HTMLInputElement>(control.ptr())) {
            auto* input_element = dynamic_cast<HTML::HTMLInputElement*>(control.ptr());
            if ((input_element->type() == "checkbox" || input_element->type() == "radio") && !input_element->checked())
                continue;
        }

        // 2. If the field element is an input element whose type attribute is in the Image Button state, then:
        if (auto* input_element = dynamic_cast<HTML::HTMLInputElement*>(control.ptr()); input_element->type() == "image") {
            // FIXME: 1. If the field element has a name attribute specified and its value is not the empty string, let name be that value followed by a single U+002E FULL STOP character (.). Otherwise, let name be the empty string.
            // FIXME: 2. Let namex be the string consisting of the concatenation of name and a single U+0078 LATIN SMALL LETTER X character (x).
            // FIXME: 3. Let namey be the string consisting of the concatenation of name and a single U+0079 LATIN SMALL LETTER Y character (y).
            // FIXME: 4. The field element is submitter, and before this algorithm was invoked the user indicated a coordinate. Let x be the x-component of the coordinate selected by the user, and let y be the y-component of the coordinate selected by the user.
            // FIXME: 5. Create an entry with namex and x, and append it to entry list.
            // FIXME: 6. Create an entry with namey and y, and append it to entry list.
            // 7. Continue.
            continue;
        }

        // FIXME: 3. If the field is a form-associated custom element, then perform the entry construction algorithm given field and entry list, then continue.

        // 4. If either the field element does not have a name attribute specified, or its name attribute's value is the empty string, then continue.
        if (control->name().is_empty())
            continue;

        // 5. Let name be the value of the field element's name attribute.
        auto name = control->name();

        // 6. If the field element is a select element, then for each option element in the select element's list of options whose selectedness is true and that is not disabled, create an entry with name and the value of the option element, and append it to entry list.
        if (auto* select_element = dynamic_cast<HTML::HTMLSelectElement*>(control.ptr())) {
            Vector<XHR::FormDataEntryValue> form_data_entries {};
            for (auto const& option_element : select_element->list_of_options()) {
                if (option_element->selected() && !option_element->disabled())
                    form_data_entries.append(option_element->value());
            }
            if (!form_data_entries.is_empty())
                entry_list.set(name, form_data_entries);
        }
        // 7. Otherwise, if the field element is an input element whose type attribute is in the Checkbox state or the Radio Button state, then:
        else if (auto* input_element = dynamic_cast<HTML::HTMLInputElement*>(control.ptr()); input_element && (input_element->type() == "checkbox" || input_element->type() == "radio") && input_element->checked()) {
            // else if (input_element && (input_element->type() == "checkbox" || input_element->type() == "radio") && input_element->checked()) {
            //  1. If the field element has a value attribute specified, then let value be the value of that attribute; otherwise, let value be the string "on".
            auto value = input_element->value();
            if (value.is_empty())
                value = "on";

            // 2. Create an entry with name and value, and append it to entry list.
            entry_list.set(name, { value });
        }
        // 8. Otherwise, if the field element is an input element whose type attribute is in the File Upload state, then:
        else if (input_element && input_element->type() == "file") {
            // 1. If there are no selected files, then create an entry with name and a new File object with an empty name, application/octet-stream as type, and an empty body, and append it to entry list.
            if (input_element->files()->length() == 0) {
                FileAPI::FilePropertyBag options {};
                options.type = "application/octet-stream";
                auto file = FileAPI::File::create(realm, {}, "", options);
                entry_list.set(name, { file.release_value() });
            }
            // 2. Otherwise, for each file in selected files, create an entry with name and a File object representing the file, and append it to entry list.
            else {
                Vector<XHR::FormDataEntryValue> form_data_entries {};
                for (size_t i = 0; i < input_element->files()->length(); i++)
                    form_data_entries.append({ *input_element->files()->item(i) });
                entry_list.set(name, form_data_entries);
            }
        }
        // FIXME: 9. Otherwise, if the field element is an input element whose type attribute is in the Hidden state and name is an ASCII case-insensitive match for "_charset_":
        // FIXME:    1. Let charset be the name of encoding if encoding is given, and "UTF-8" otherwise.
        // FIXME:    2. Create an entry with name and charset, and append it to entry list.
        // 10. Otherwise, create an entry with name and the value of the field element, and append it to entry list.
        else if (input_element) {
            entry_list.set(name, { input_element->value() });
        }

        // FIXME: 11. If the element has a dirname attribute, and that attribute's value is not the empty string, then:
        // FIXME:     1. Let dirname be the value of the element's dirname attribute.
        // FIXME:     2. Let dir be the string "ltr" if the directionality of the element is 'ltr', and "rtl" otherwise (i.e., when the directionality of the element is 'rtl').
        // FIXME:     3. Create an entry with dirname and dir, and append it to entry list.
    }
    // FIXME: 6. Let form data be a new FormData object associated with entry list.

    // FIXME: 7. Fire an event named formdata at form using FormDataEvent, with the formData attribute initialized to form data and the bubbles attribute initialized to true.

    // 8. Set form's constructing entry list to false.
    // NOTE: The ScopeGuard set earlier takes care of setting this back to false.

    // FIXME: 9. Return a clone of entry list.
    return entry_list;
}

void HTMLFormElement::retrieve_submittable_elements(Vector<JS::NonnullGCPtr<DOM::Element>>& elements, JS::NonnullGCPtr<DOM::Element> element)
{
    if (auto* form_associated_element = dynamic_cast<HTML::FormAssociatedElement*>(element.ptr())) {
        if (form_associated_element->is_submittable())
            elements.append(element);
    }

    for (size_t i; i < element->children()->length(); i++) {
        auto* child = element->children()->item(i);
        retrieve_submittable_elements(elements, *child);
    }
}

// https://html.spec.whatwg.org/multipage/forms.html#category-submit
Vector<JS::NonnullGCPtr<DOM::Element>> HTMLFormElement::get_submittable_elements()
{
    Vector<JS::NonnullGCPtr<DOM::Element>> submittable_elements = {};
    for (size_t i = 0; i < this->elements()->length(); i++) {
        auto* element = this->elements()->item(i);
        retrieve_submittable_elements(submittable_elements, *element);
    }
    return submittable_elements;
}

}
