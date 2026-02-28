/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mchips/protocol/schema_generator.h>

namespace mchips::protocol {

//=============================================================================
// Free function
//=============================================================================

/// Convert SchemaType enum to JSON Schema type string (draft 2020-12).
const char* SchemaTypeToString(SchemaType type) {
  switch (type) {
    case SchemaType::String:  return "string";
    case SchemaType::Number:  return "number";
    case SchemaType::Integer: return "integer";
    case SchemaType::Boolean: return "boolean";
    case SchemaType::Array:   return "array";
    case SchemaType::Object:  return "object";
    case SchemaType::Null:    return "null";
    default:                  return "string";
  }
}

//=============================================================================
// SchemaBuilder
//=============================================================================

SchemaBuilder::SchemaBuilder() {
  schema_["type"] = "object";
  schema_["properties"] = json::object();
}

/// Set the top-level schema type.
SchemaBuilder& SchemaBuilder::Type(SchemaType type) {
  schema_["type"] = SchemaTypeToString(type);
  return *this;
}

/// Add a description to the schema.
SchemaBuilder& SchemaBuilder::Description(const std::string& desc) {
  schema_["description"] = desc;
  return *this;
}

/// Add a property with a SchemaType (auto-generates a simple type schema).
SchemaBuilder& SchemaBuilder::AddProperty(const std::string& name,
                                           SchemaType type,
                                           const std::string& description,
                                           bool required) {
  json prop;
  prop["type"] = SchemaTypeToString(type);
  if (!description.empty()) {
    prop["description"] = description;
  }
  return AddProperty(name, prop, required);
}

/// Add a property with an explicit JSON Schema object.
SchemaBuilder& SchemaBuilder::AddProperty(const std::string& name,
                                           const json& schema,
                                           bool required) {
  schema_["properties"][name] = schema;
  if (required) {
    required_.push_back(name);
  }
  return *this;
}

/// Build and return the final JSON Schema object.
json SchemaBuilder::Build() const {
  json result = schema_;
  if (!required_.empty()) {
    json req_arr = json::array();
    for (const auto& name : required_) {
      req_arr.push_back(name);
    }
    result["required"] = req_arr;
  }
  return result;
}

//=============================================================================
// ToolBuilder
//=============================================================================

ToolBuilder::ToolBuilder(const std::string& name) : name_(name) {
  input_schema_.Type(SchemaType::Object);
}

/// Set the tool description.
ToolBuilder& ToolBuilder::Description(const std::string& desc) {
  description_ = desc;
  return *this;
}

/// Add a parameter with a SchemaType.
ToolBuilder& ToolBuilder::AddParam(const std::string& name, SchemaType type,
                                    const std::string& description,
                                    bool required) {
  input_schema_.AddProperty(name, type, description, required);
  return *this;
}

/// Add a parameter with an explicit JSON Schema.
ToolBuilder& ToolBuilder::AddParam(const std::string& name, const json& schema,
                                    bool required) {
  input_schema_.AddProperty(name, schema, required);
  return *this;
}

/// Set the output schema for the tool.
ToolBuilder& ToolBuilder::OutputSchema(const json& schema) {
  output_schema_ = schema;
  return *this;
}

/// Set tool annotations (hints for clients about tool behavior).
ToolBuilder& ToolBuilder::Annotations(const ToolAnnotations& annotations) {
  annotations_ = annotations;
  return *this;
}

/// Build and return the ToolDefinition.
ToolDefinition ToolBuilder::Build() const {
  ToolDefinition td;
  td.name = name_;
  td.description = description_;
  td.inputSchema = input_schema_.Build();
  td.outputSchema = output_schema_;
  td.annotations = annotations_;
  return td;
}

}  // namespace mchips::protocol
