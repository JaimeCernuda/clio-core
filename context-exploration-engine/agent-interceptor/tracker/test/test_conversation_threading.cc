#include <catch2/catch_test_macros.hpp>

#include "dt_provenance/protocol/interaction.h"
#include "dt_provenance/tracker/conversation_threading.h"

using namespace dt_provenance::protocol;
using namespace dt_provenance::tracker;
using json = nlohmann::ordered_json;

TEST_CASE("First interaction in a session gets initial turn", "[threading]") {
  ConversationThreader threader;
  InteractionRecord record;
  record.sequence_id = 1;
  record.session_id = "agent-0";
  record.request.body = json{{"messages", json::array({json{{"role", "user"}, {"content", "Hello"}}})}};
  record.response.text = "Hi there!";

  threader.ResolveThreading(record);

  REQUIRE(record.conversation.turn_type == "initial");
  REQUIRE(record.conversation.turn_number == 1);
  REQUIRE(record.conversation.parent_sequence_id == 0);
  REQUIRE_FALSE(record.conversation.conversation_id.empty());
}

TEST_CASE("Continuation detected when previous response in messages",
          "[threading]") {
  ConversationThreader threader;

  // First interaction
  InteractionRecord first;
  first.sequence_id = 1;
  first.session_id = "agent-0";
  first.request.body = json{{"messages", json::array({json{{"role", "user"}, {"content", "Hello"}}})}};
  first.response.text = "Hi there! How can I help?";
  threader.ResolveThreading(first);

  REQUIRE(first.conversation.turn_type == "initial");

  // Second interaction — includes previous response as assistant message
  InteractionRecord second;
  second.sequence_id = 2;
  second.session_id = "agent-0";
  second.request.body = json{
      {"messages",
       json::array(
           {json{{"role", "user"}, {"content", "Hello"}},
            json{{"role", "assistant"}, {"content", "Hi there! How can I help?"}},
            json{{"role", "user"}, {"content", "Tell me about CTE"}}})}};
  second.response.text = "CTE stands for Context Transfer Engine.";
  threader.ResolveThreading(second);

  REQUIRE(second.conversation.turn_type == "continuation");
  REQUIRE(second.conversation.turn_number == 2);
  REQUIRE(second.conversation.parent_sequence_id == 1);
  REQUIRE(second.conversation.conversation_id == first.conversation.conversation_id);
}

TEST_CASE("Tool result turn detected correctly", "[threading]") {
  ConversationThreader threader;

  // First: assistant makes a tool call
  InteractionRecord first;
  first.sequence_id = 1;
  first.session_id = "agent-0";
  first.request.body = json{{"messages", json::array({json{{"role", "user"}, {"content", "Read test.cc"}}})}};
  first.response.text = "";
  first.response.tool_calls.push_back(ToolCall{"t1", "Read", json{{"file", "test.cc"}}});
  threader.ResolveThreading(first);

  // Second: has tool_result message
  InteractionRecord second;
  second.sequence_id = 2;
  second.session_id = "agent-0";
  second.request.body = json{
      {"messages",
       json::array(
           {json{{"role", "user"}, {"content", "Read test.cc"}},
            json{{"role", "assistant"}, {"content", json::array({json{{"type", "tool_use"}, {"id", "t1"}, {"name", "Read"}}})}},
            json{{"role", "tool"}, {"content", "file contents here"}}})}};
  second.response.text = "Here is the file content.";
  threader.ResolveThreading(second);

  REQUIRE(second.conversation.turn_type == "tool_result");
  REQUIRE(second.conversation.turn_number == 2);
  REQUIRE(second.conversation.parent_sequence_id == 1);
}

TEST_CASE("New conversation started when no continuation signal",
          "[threading]") {
  ConversationThreader threader;

  // First interaction
  InteractionRecord first;
  first.sequence_id = 1;
  first.session_id = "agent-0";
  first.request.body = json{{"messages", json::array({json{{"role", "user"}, {"content", "Hello"}}})}};
  first.response.text = "Hi!";
  threader.ResolveThreading(first);
  std::string first_conv_id = first.conversation.conversation_id;

  // Second interaction — completely unrelated (no continuation signal)
  InteractionRecord second;
  second.sequence_id = 2;
  second.session_id = "agent-0";
  second.request.body = json{{"messages", json::array({json{{"role", "user"}, {"content", "What is 2+2?"}}})}};
  second.response.text = "4";
  threader.ResolveThreading(second);

  REQUIRE(second.conversation.turn_type == "initial");
  REQUIRE(second.conversation.turn_number == 1);
  REQUIRE(second.conversation.conversation_id != first_conv_id);
}

TEST_CASE("Multiple sessions tracked independently", "[threading]") {
  ConversationThreader threader;

  InteractionRecord a1;
  a1.sequence_id = 1;
  a1.session_id = "agent-0";
  a1.request.body = json{{"messages", json::array({json{{"role", "user"}, {"content", "Hi"}}})}};
  a1.response.text = "Hello from A";
  threader.ResolveThreading(a1);

  InteractionRecord b1;
  b1.sequence_id = 2;
  b1.session_id = "agent-1";
  b1.request.body = json{{"messages", json::array({json{{"role", "user"}, {"content", "Hi"}}})}};
  b1.response.text = "Hello from B";
  threader.ResolveThreading(b1);

  REQUIRE(a1.conversation.turn_type == "initial");
  REQUIRE(b1.conversation.turn_type == "initial");
  REQUIRE(a1.conversation.conversation_id != b1.conversation.conversation_id);
}

TEST_CASE("Three-turn conversation has correct numbering", "[threading]") {
  ConversationThreader threader;

  InteractionRecord r1;
  r1.sequence_id = 1;
  r1.session_id = "s1";
  r1.request.body = json{{"messages", json::array({json{{"role", "user"}, {"content", "Start"}}})}};
  r1.response.text = "Response 1";
  threader.ResolveThreading(r1);

  InteractionRecord r2;
  r2.sequence_id = 2;
  r2.session_id = "s1";
  r2.request.body = json{
      {"messages",
       json::array({json{{"role", "user"}, {"content", "Start"}},
                     json{{"role", "assistant"}, {"content", "Response 1"}},
                     json{{"role", "user"}, {"content", "Continue"}}})}};
  r2.response.text = "Response 2";
  threader.ResolveThreading(r2);

  InteractionRecord r3;
  r3.sequence_id = 3;
  r3.session_id = "s1";
  r3.request.body = json{
      {"messages",
       json::array({json{{"role", "user"}, {"content", "Start"}},
                     json{{"role", "assistant"}, {"content", "Response 1"}},
                     json{{"role", "user"}, {"content", "Continue"}},
                     json{{"role", "assistant"}, {"content", "Response 2"}},
                     json{{"role", "user"}, {"content", "Finish"}}})}};
  r3.response.text = "Done!";
  threader.ResolveThreading(r3);

  REQUIRE(r1.conversation.turn_number == 1);
  REQUIRE(r2.conversation.turn_number == 2);
  REQUIRE(r3.conversation.turn_number == 3);

  REQUIRE(r1.conversation.turn_type == "initial");
  REQUIRE(r2.conversation.turn_type == "continuation");
  REQUIRE(r3.conversation.turn_type == "continuation");

  REQUIRE(r2.conversation.parent_sequence_id == 1);
  REQUIRE(r3.conversation.parent_sequence_id == 2);
}
