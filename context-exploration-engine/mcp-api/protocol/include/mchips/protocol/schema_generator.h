/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_PROTOCOL_SCHEMA_GENERATOR_H_
#define MCHIPS_PROTOCOL_SCHEMA_GENERATOR_H_

#include <mchips/protocol/mcp_types.h>

#include <string>
#include <vector>

namespace mchips::protocol {

/// JSON Schema type names
enum class SchemaType {
  String,
  Number,
  Integer,
  Boolean,
  Array,
  Object,
  Null,
};

/// Convert SchemaType to JSON Schema type string
const char* SchemaTypeToString(SchemaType type);

/// Builder for JSON Schema objects (draft 2020-12)
class SchemaBuilder {
 public:
  SchemaBuilder();

  SchemaBuilder& Type(SchemaType type);
  SchemaBuilder& Description(const std::string& desc);
  SchemaBuilder& AddProperty(const std::string& name, SchemaType type,
                             const std::string& description,
                             bool required = false);
  SchemaBuilder& AddProperty(const std::string& name, const json& schema,
                             bool required = false);

  json Build() const;

 private:
  json schema_;
  std::vector<std::string> required_;
};

/// Fluent builder for MCP ToolDefinition objects
class ToolBuilder {
 public:
  explicit ToolBuilder(const std::string& name);

  ToolBuilder& Description(const std::string& desc);
  ToolBuilder& AddParam(const std::string& name, SchemaType type,
                        const std::string& description,
                        bool required = false);
  ToolBuilder& AddParam(const std::string& name, const json& schema,
                        bool required = false);
  ToolBuilder& OutputSchema(const json& schema);
  ToolBuilder& Annotations(const ToolAnnotations& annotations);

  ToolDefinition Build() const;

 private:
  std::string name_;
  std::string description_;
  SchemaBuilder input_schema_;
  std::optional<json> output_schema_;
  std::optional<ToolAnnotations> annotations_;
};

}  // namespace mchips::protocol

#endif  // MCHIPS_PROTOCOL_SCHEMA_GENERATOR_H_
