/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mchips/protocol/mcp_types.h>

namespace mchips::protocol {

//=============================================================================
// ToolAnnotations
//=============================================================================

/// Serialize tool annotations to JSON per MCP spec.
json ToolAnnotations::ToJson() const {
  json j;
  if (title.has_value()) {
    j["title"] = *title;
  }
  j["readOnlyHint"] = readOnlyHint;
  j["destructiveHint"] = destructiveHint;
  j["idempotentHint"] = idempotentHint;
  j["openWorldHint"] = openWorldHint;
  j["priority"] = priority;
  return j;
}

/// Deserialize tool annotations from JSON.
ToolAnnotations ToolAnnotations::FromJson(const json& j) {
  ToolAnnotations a;
  if (j.contains("title") && !j["title"].is_null()) {
    a.title = j["title"].get<std::string>();
  }
  a.readOnlyHint = j.value("readOnlyHint", false);
  a.destructiveHint = j.value("destructiveHint", true);
  a.idempotentHint = j.value("idempotentHint", false);
  a.openWorldHint = j.value("openWorldHint", true);
  a.priority = j.value("priority", 0.5);
  return a;
}

//=============================================================================
// ToolDefinition
//=============================================================================

/// Serialize tool definition to JSON per MCP spec.
json ToolDefinition::ToJson() const {
  json j;
  j["name"] = name;
  j["description"] = description;
  j["inputSchema"] = inputSchema;
  if (outputSchema.has_value()) {
    j["outputSchema"] = *outputSchema;
  }
  if (annotations.has_value()) {
    j["annotations"] = annotations->ToJson();
  }
  return j;
}

/// Deserialize tool definition from JSON.
ToolDefinition ToolDefinition::FromJson(const json& j) {
  ToolDefinition td;
  td.name = j.at("name").get<std::string>();
  td.description = j.at("description").get<std::string>();
  td.inputSchema = j.at("inputSchema");
  if (j.contains("outputSchema") && !j["outputSchema"].is_null()) {
    td.outputSchema = j["outputSchema"];
  }
  if (j.contains("annotations") && !j["annotations"].is_null()) {
    td.annotations = ToolAnnotations::FromJson(j["annotations"]);
  }
  return td;
}

//=============================================================================
// ContentItem
//=============================================================================

/// Serialize content item to JSON per MCP spec.
json ContentItem::ToJson() const {
  json j;
  j["type"] = type;
  if (text.has_value()) {
    j["text"] = *text;
  }
  if (mimeType.has_value()) {
    j["mimeType"] = *mimeType;
  }
  if (data.has_value()) {
    j["data"] = *data;
  }
  return j;
}

/// Deserialize content item from JSON.
ContentItem ContentItem::FromJson(const json& j) {
  ContentItem ci;
  ci.type = j.at("type").get<std::string>();
  if (j.contains("text") && !j["text"].is_null()) {
    ci.text = j["text"].get<std::string>();
  }
  if (j.contains("mimeType") && !j["mimeType"].is_null()) {
    ci.mimeType = j["mimeType"].get<std::string>();
  }
  if (j.contains("data") && !j["data"].is_null()) {
    ci.data = j["data"].get<std::string>();
  }
  return ci;
}

//=============================================================================
// CallToolResult
//=============================================================================

/// Serialize tool call result to JSON per MCP spec.
json CallToolResult::ToJson() const {
  json j;
  json content_arr = json::array();
  for (const auto& item : content) {
    content_arr.push_back(item.ToJson());
  }
  j["content"] = content_arr;
  j["isError"] = isError;
  return j;
}

/// Deserialize tool call result from JSON.
CallToolResult CallToolResult::FromJson(const json& j) {
  CallToolResult result;
  if (j.contains("content") && j["content"].is_array()) {
    for (const auto& item : j["content"]) {
      result.content.push_back(ContentItem::FromJson(item));
    }
  }
  result.isError = j.value("isError", false);
  return result;
}

//=============================================================================
// ServerCapabilities
//=============================================================================

/// Serialize server capabilities to JSON.
json ServerCapabilities::ToJson() const {
  json j;
  if (tools.has_value()) {
    json tools_j;
    tools_j["listChanged"] = tools->listChanged;
    j["tools"] = tools_j;
  }
  return j;
}

/// Deserialize server capabilities from JSON.
ServerCapabilities ServerCapabilities::FromJson(const json& j) {
  ServerCapabilities caps;
  if (j.contains("tools") && !j["tools"].is_null()) {
    ServerCapabilities::ToolsCapability tc;
    tc.listChanged = j["tools"].value("listChanged", true);
    caps.tools = tc;
  }
  return caps;
}

//=============================================================================
// ClientCapabilities
//=============================================================================

/// Serialize client capabilities to JSON (currently empty).
json ClientCapabilities::ToJson() const {
  return json::object();
}

/// Deserialize client capabilities from JSON.
ClientCapabilities ClientCapabilities::FromJson(const json& /*j*/) {
  return ClientCapabilities{};
}

//=============================================================================
// ServerInfo
//=============================================================================

/// Serialize server info to JSON.
json ServerInfo::ToJson() const {
  json j;
  j["name"] = name;
  j["version"] = version;
  return j;
}

/// Deserialize server info from JSON.
ServerInfo ServerInfo::FromJson(const json& j) {
  ServerInfo info;
  info.name = j.value("name", "mchips");
  info.version = j.value("version", "0.1.0");
  return info;
}

//=============================================================================
// InitializeResult
//=============================================================================

/// Serialize initialize result to JSON per MCP spec.
json InitializeResult::ToJson() const {
  json j;
  j["protocolVersion"] = protocolVersion;
  j["capabilities"] = capabilities.ToJson();
  j["serverInfo"] = serverInfo.ToJson();
  if (instructions.has_value()) {
    j["instructions"] = *instructions;
  }
  return j;
}

/// Deserialize initialize result from JSON.
InitializeResult InitializeResult::FromJson(const json& j) {
  InitializeResult result;
  result.protocolVersion = j.value("protocolVersion", kMcpProtocolVersion);
  if (j.contains("capabilities")) {
    result.capabilities = ServerCapabilities::FromJson(j["capabilities"]);
  }
  if (j.contains("serverInfo")) {
    result.serverInfo = ServerInfo::FromJson(j["serverInfo"]);
  }
  if (j.contains("instructions") && !j["instructions"].is_null()) {
    result.instructions = j["instructions"].get<std::string>();
  }
  return result;
}

}  // namespace mchips::protocol
