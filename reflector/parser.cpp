#include "parser.h"
#include "filesystem_utils.h"
#include "reflection_data.h"
#include "reflector.h"

#include <assert.h>
#include <cstdio>
#include <cstring>
#include <stdarg.h>
#include <string.h>

inline Token parser_get_current_token(const Parser *parser) { return parser->tokens.tokens[parser->token_index]; }

inline void parser_advance(Parser *parser) { parser->token_index++; }

inline Token parser_advance_and_get_next_token(Parser *parser) {
  parser_advance(parser);
  return parser_get_current_token(parser);
}

inline bool parser_still_valid(Parser *parser) { return parser->token_index < parser->tokens.size; }

inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

inline bool is_octal_digit(char c) { return c >= '0' && c <= '7'; }

inline bool is_hex_digit(char c) {
  bool is_upper_hex = (c >= 'A' && c <= 'F');
  bool is_lower_hex = (c >= 'a' && c <= 'f');
  return is_digit(c) || is_lower_hex || is_upper_hex;
}

inline bool is_nondigit(char c) {
  bool is_upper = (c >= 'A' && c <= 'Z');
  bool is_lower = (c >= 'a' && c <= 'z');
  return c == '_' || is_lower || is_upper;
}

TokenType string_slice_to_keyword_or_identifier(const char *string, u32 length) {
  if (length == 2 && strncmp(string, "in", length) == 0) {
    return TOKEN_TYPE_IN;
  }
  if (length == 3 && strncmp(string, "out", length) == 0) {
    return TOKEN_TYPE_OUT;
  }
  if (length == 7 && strncmp(string, "version", length) == 0) {
    return TOKEN_TYPE_VERSION;
  }
  if (length == 4 && strncmp(string, "void", length) == 0) {
    return TOKEN_TYPE_VOID;
  }
  if (length == 7 && strncmp(string, "uniform", length) == 0) {
    return TOKEN_TYPE_UNIFORM;
  }
  if (length == 7 && strncmp(string, "sampler", length) == 0) {
    return TOKEN_TYPE_SAMPLER;
  }
  if (length == 9 && strncmp(string, "sampler2D", length) == 0) {
    return TOKEN_TYPE_SAMPLER2D;
  }
  if (length == 9 && strncmp(string, "texture2D", length) == 0) {
    return TOKEN_TYPE_TEXTURE2D;
  }
  if (length == 7 && strncmp(string, "image2D", length) == 0) {
    return TOKEN_TYPE_IMAGE2D;
  }
  if (length == 4 && strncmp(string, "uint", length) == 0) {
    return TOKEN_TYPE_UINT;
  }
  if (length == 5 && strncmp(string, "float", length) == 0) {
    return TOKEN_TYPE_FLOAT;
  }
  if (length == 4 && strncmp(string, "vec2", length) == 0) {
    return TOKEN_TYPE_VEC2;
  }
  if (length == 4 && strncmp(string, "vec3", length) == 0) {
    return TOKEN_TYPE_VEC3;
  }
  if (length == 4 && strncmp(string, "vec4", length) == 0) {
    return TOKEN_TYPE_VEC4;
  }
  if (length == 4 && strncmp(string, "mat2", length) == 0) {
    return TOKEN_TYPE_MAT2;
  }
  if (length == 4 && strncmp(string, "mat3", length) == 0) {
    return TOKEN_TYPE_MAT3;
  }
  if (length == 4 && strncmp(string, "mat4", length) == 0) {
    return TOKEN_TYPE_MAT4;
  }
  if (length == 7 && strncmp(string, "VERSION", length) == 0) {
    return TOKEN_TYPE_DIRECTIVE_VERSION;
  }
  if (length == 8 && strncmp(string, "LOCATION", length) == 0) {
    return TOKEN_TYPE_DIRECTIVE_LOCATION;
  }
  if (length == 11 && strncmp(string, "SET_BINDING", length) == 0) {
    return TOKEN_TYPE_DIRECTIVE_SET_BINDING;
  }
  if (length == 12 && strncmp(string, "PUSH_CONSTANT", length) == 0) {
    return TOKEN_TYPE_DIRECTIVE_PUSH_CONSTANT;
  }
  if (length == 11 && strncmp(string, "RATE_VERTEX", length) == 0) {
    return TOKEN_TYPE_RATE_VERTEX;
  }
  if (length == 13 && strncmp(string, "RATE_INSTANCE", length) == 0) {
    return TOKEN_TYPE_RATE_INSTANCE;
  }
  if (length == 7 && strncmp(string, "BINDING", length) == 0) {
    return TOKEN_TYPE_BINDING;
  }
  if (length == 6 && strncmp(string, "OFFSET", length) == 0) {
    return TOKEN_TYPE_OFFSET;
  }
  if (length == 14 && strncmp(string, "TIGHTLY_PACKED", length) == 0) {
    return TOKEN_TYPE_TIGHTLY_PACKED;
  }
  if (length == 12 && strncmp(string, "BUFFER_LABEL", length) == 0) {
    return TOKEN_TYPE_BUFFER_LABEL;
  }

  return TOKEN_TYPE_TEXT;
}

VertexAttributeRate token_type_to_vertex_attribute_rate(TokenType type) {
  switch (type) {
  case TOKEN_TYPE_RATE_VERTEX:
    return VERTEX_ATTRIBUTE_RATE_VERTEX;
  case TOKEN_TYPE_RATE_INSTANCE:
    return VERTEX_ATTRIBUTE_RATE_INSTANCE;
  default:
    fprintf(stderr, "Trying to convert token type %s to a vertex attribute rate\n", token_type_to_string[type]);
    return NUM_VERTEX_ATTRIBUTE_RATES;
  }
}

GLSLType token_type_to_glsl_type(TokenType type) {
  switch (type) {
  case TOKEN_TYPE_FLOAT:
    return GLSL_TYPE_FLOAT;
  case TOKEN_TYPE_UINT:
    return GLSL_TYPE_UINT;
  case TOKEN_TYPE_VEC2:
    return GLSL_TYPE_VEC2;
  case TOKEN_TYPE_VEC3:
    return GLSL_TYPE_VEC3;
  case TOKEN_TYPE_VEC4:
    return GLSL_TYPE_VEC4;
  case TOKEN_TYPE_MAT2:
    return GLSL_TYPE_MAT2;
  case TOKEN_TYPE_MAT3:
    return GLSL_TYPE_MAT3;
  case TOKEN_TYPE_MAT4:
    return GLSL_TYPE_MAT4;
  default:
    fprintf(stderr, "Trying to convert token type %s to a glsl type\n", token_type_to_string[type]);
    return GLSL_TYPE_NULL;
  }
}

// return the number of chars consumed, i.e. length of lexed number looking
// thing
u32 lex_number(const char *string, u32 string_length) {
  bool seen_decimal_point = false;
  bool seen_exponent = false;
  bool seen_float_suffix = false;
  bool seen_unsigned_suffix = false;
  u32 i = 0;

  // check hex
  if (string_length >= 2 && (string[0] == '0' && (string[1] == 'x' || string[1] == 'X'))) {
    i += 2;
    while (i < string_length && (is_hex_digit(string[i]))) {
      i++;
    }
    if (i < string_length && (string[i] == 'u' || string[i] == 'U')) {
      i++;
    }
    return i;
  }

  // check octal
  if (string_length >= 2 && (string[0] == '0' && is_octal_digit(string[1]))) {
    i += 2;
    while (i < string_length && (is_octal_digit(string[i]))) {
      i++;
    }
    if (i < string_length && (string[i] == 'u' || string[i] == 'U')) {
      i++;
    }
    return i;
  }

  while (i < string_length) {
    char c = string[i];
    if (is_digit(c)) {
      i++;
      continue;
    }

    // check decimal points
    if (c == '.') {
      if (seen_decimal_point) {
        fprintf(stderr, "lex_number: found number with two decimal points\n");
      }

      if (seen_exponent) {
        fprintf(stderr, "lex_number: found number with decimal exponent\n");
      }

      seen_decimal_point = true;
      i++;
      continue;
    }

    // check exponents
    if (c == 'e' || c == 'E') {
      if (seen_exponent) {
        fprintf(stderr, "lex_number: found number with two exponents\n");
      }
      seen_exponent = true;
      i++;

      if (i < string_length && (string[i] == '+' || string[i] == '-')) {
        i++;
      }

      continue;
    }

    // check suffixes
    // not currently supporting doubles
    if (c == 'F' || c == 'f') {
      if (seen_float_suffix) {
        fprintf(stderr, "lex_number: found number with two float suffixes\n");
      }
      seen_float_suffix = true;
      i++;
      continue;
    }

    if (c == 'u' || c == 'U') {
      if (seen_unsigned_suffix) {
        fprintf(stderr, "lex_number: found number with two unsigned suffixes\n");
      }
      seen_unsigned_suffix = true;
      i++;
      continue;
    }

    // got something not part of a number, break
    break;
  }

  return i;
}

TokenVector lex_string(const char *string, u64 string_length) {
  TokenVector tokens = new_token_vector();

  u64 i = 0;
  while (i < string_length) {
    char c = string[i];

    if (c == ' ' || c == '\t' || c == '\n') {
      i++;
      continue;
    }

    switch (c) {
    case '/': {
      // skip line comments
      if (i + 1 < string_length && string[i + 1] == '/') {
        while (i < string_length && string[i] != '\n' && string[i] != '\0') {
          i++;
        }
      } else {
        push_token(&tokens, new_token(TOKEN_TYPE_SLASH, string + i));
        i++;
      }
      break;
    }

      // periods and check numbers
    case '.': {
      if (i + 1 < string_length && is_digit(string[i + 1])) {
        const char *start = string + i;
        u32 consumed_chars = lex_number(string + i, string_length - i);
        push_token(&tokens, new_text_token(TOKEN_TYPE_NUMBER, start, consumed_chars));
        i += consumed_chars;
      } else {
        push_token(&tokens, new_token(TOKEN_TYPE_PERIOD, string + i));
        i++;
      }
      break;
    }

    case '{': {
      if (i + 1 < string_length && string[i + 1] == '{') {
        push_token(&tokens, new_token(TOKEN_TYPE_DOUBLE_L_BRACE, string + i));
        i += 2;
      } else {
        push_token(&tokens, new_token(TOKEN_TYPE_L_BRACE, string + i));
        i++;
      }
      break;
    }

    case '}': {
      if (i + 1 < string_length && string[i + 1] == '}') {
        push_token(&tokens, new_token(TOKEN_TYPE_DOUBLE_R_BRACE, string + i));
        i += 2;
      } else {
        push_token(&tokens, new_token(TOKEN_TYPE_R_BRACE, string + i));
        i++;
      }
      break;
    }

    case '+': {
      if (i + 1 < string_length && string[i + 1] == '+') {
        push_token(&tokens, new_token(TOKEN_TYPE_PLUS_PLUS, string + i));
        i += 2;
      } else {
        push_token(&tokens, new_token(TOKEN_TYPE_PLUS, string + i));
        i++;
      }
      break;
    }

    case '-': {
      if (i + 1 < string_length && string[i + 1] == '-') {
        push_token(&tokens, new_token(TOKEN_TYPE_MINUS_MINUS, string + i));
        i += 2;
      } else {
        push_token(&tokens, new_token(TOKEN_TYPE_MINUS, string + i));
        i++;
      }
      break;
    }

    case '=': {
      if (i + 1 < string_length && string[i + 1] == '=') {
        push_token(&tokens, new_token(TOKEN_TYPE_EQUALS_EQUALS, string + i));
        i += 2;
      } else {
        push_token(&tokens, new_token(TOKEN_TYPE_EQUALS, string + i));
        i++;
      }
      break;
    }

    case '#': {
      push_token(&tokens, new_token(TOKEN_TYPE_POUND, string + i));
      i++;
      break;
    }

    case '(': {
      push_token(&tokens, new_token(TOKEN_TYPE_L_PAREN, string + i));
      i++;
      break;
    }

    case ')': {
      push_token(&tokens, new_token(TOKEN_TYPE_R_PAREN, string + i));
      i++;
      break;
    }

    case '[': {
      push_token(&tokens, new_token(TOKEN_TYPE_L_BRACKET, string + i));
      i++;
      break;
    }

    case ']': {
      push_token(&tokens, new_token(TOKEN_TYPE_R_BRACKET, string + i));
      i++;
      break;
    }

    case ';': {
      push_token(&tokens, new_token(TOKEN_TYPE_SEMICOLON, string + i));
      i++;
      break;
    }

    case ',': {
      push_token(&tokens, new_token(TOKEN_TYPE_COMMA, string + i));
      i++;
      break;
    }

    default: {
      if (is_digit(c)) {
        const char *start = string + i;
        u32 consumed_chars = lex_number(string + i, string_length - i);
        push_token(&tokens, new_text_token(TOKEN_TYPE_NUMBER, start, consumed_chars));
        i += consumed_chars;
      } else {
        const char *text_start = string + i;
        u32 start_index = i;
        i++;

        while (i < string_length && (is_nondigit(string[i]) || is_digit(string[i]))) {
          i++;
        }

        u32 consumed_chars = i - start_index;
        TokenType token_type = string_slice_to_keyword_or_identifier(text_start, consumed_chars);
        if (token_type == TOKEN_TYPE_TEXT) {
          push_token(&tokens, new_text_token(TOKEN_TYPE_TEXT, text_start, consumed_chars));
        } else {
          push_token(&tokens, new_token(token_type, text_start));
        }
      }
    }
    }
  }

  return tokens;
}

void report_parser_error(Parser *parser, const char *token_start, TokenType recovery_token_type, const char *fmt, ...) {
  const char *start = token_start, *end = token_start;
  while (start > parser->source && *start != '\n') {
    start--;
  }
  if (*start == '\n') {
    start++;
  }

  const char *source_end = parser->source + parser->source_length;
  while (end < source_end && *end != '\n') {
    end++;
  }
  char buffer[512]; // stack buffer for formatted message
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  printf("%sERROR%s: ", RED, RESET);
  printf("%s\n", buffer);                        // print message
  printf("\t%.*s\n", (int)(end - start), start); // print offending line

  while (parser_still_valid(parser)) {
    Token current_token = parser_get_current_token(parser);
    if (current_token.type == recovery_token_type) {
      break;
    }
    parser_advance(parser);
  }
}

// {{ VERSION }}
void parse_version_directive(Parser *parser, TemplateStringSlice *template_string_slice) {
  Token current_token = parser_get_current_token(parser);
  assert(current_token.type == TOKEN_TYPE_DIRECTIVE_VERSION);

  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Expected }} after VERSION in version directive, got %s",
                        token_type_to_string[current_token.type]);
    return;
  }

  current_token = parser_advance_and_get_next_token(parser);
  template_string_slice->end = current_token.start;
}

u32 parse_integer_token(Parser *parser, Token token) {
  char buf[64];
  if (token.text_length >= sizeof(buf)) {
    report_parser_error(parser, token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Parsing integer token, but data was too long to buffer");
    return 0;
  }

  memcpy(buf, token.start, token.text_length);
  buf[token.text_length] = '\0'; // NUL-terminate

  return (int)strtol(buf, NULL, 10);
}

// {{ LOCATION 0 BINDING 0 RATE_VERTEX OFFSET TIGHTLY_PACKED }} in type identifier;
// or
// {{ LOCATION 0 }} - for fragment and compute shaders, or
LocationDirectiveParse parse_location_directive(Parser *parser, ShaderStage shader_stage,
                                                TemplateStringSlice *template_string_slice) {
  Token current_token = parser_get_current_token(parser);
  assert(current_token.type == TOKEN_TYPE_DIRECTIVE_LOCATION);

  LocationDirectiveParse location_directive_parse;
  location_directive_parse.next_glsl_source_start = NULL;
  location_directive_parse.found_repeat_attribute = false;
  bool is_tightly_packed = false;
  location_directive_parse.vertex_attribute.location = 0;
  location_directive_parse.vertex_attribute.binding = 0;
  location_directive_parse.vertex_attribute.offset = 0;
  location_directive_parse.vertex_attribute.rate = VERTEX_ATTRIBUTE_RATE_VERTEX;
  location_directive_parse.vertex_attribute.glsl_type = GLSL_TYPE_NULL;
  location_directive_parse.vertex_attribute.is_tightly_packed = false;
  location_directive_parse.vertex_attribute.is_valid = false;

  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_NUMBER) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Expected numeric literal after LOCATION in location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }
  u32 location = parse_integer_token(parser, current_token);
  template_string_slice->location = location;

  // either BINDING or the end for outs/fragment/compute
  current_token = parser_advance_and_get_next_token(parser);
  if (shader_stage != SHADER_STAGE_VERTEX) {
    if (current_token.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                          "Expected DOUBLE_R_BRACE after location in location directive "
                          "for a non-vertex shader, got %s",
                          token_type_to_string[current_token.type]);
      return location_directive_parse;
    }

    // move past double r brace and mark one past the end of this slice
    current_token = parser_advance_and_get_next_token(parser);
    location_directive_parse.next_glsl_source_start = current_token.start;
    template_string_slice->end = current_token.start;
    return location_directive_parse;
  }

  // in a vertex shader, can have either out or BINDING + the rest of the binding
  // check for double r brace and out
  if (current_token.type == TOKEN_TYPE_DOUBLE_R_BRACE) {
    current_token = parser_advance_and_get_next_token(parser);

    if (current_token.type != TOKEN_TYPE_OUT) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_OUT,
                          "Numeric location in LOCATION directive followed by DOUBLE_R_BRACE, expected out but got %s",
                          token_type_to_string[current_token.type]);
    }

    // move past double r brace and mark one past the end of this slice
    template_string_slice->end = current_token.start;
    location_directive_parse.next_glsl_source_start = current_token.start;
    return location_directive_parse;
  }

  // BINDING
  if (current_token.type != TOKEN_TYPE_BINDING) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Expected BINDING after location in location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }

  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_NUMBER) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Expected numeric literal after BINDING in location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }
  u32 binding = parse_integer_token(parser, current_token);

  // rate
  current_token = parser_advance_and_get_next_token(parser);
  bool is_rate = (current_token.type == TOKEN_TYPE_RATE_VERTEX) || (current_token.type == TOKEN_TYPE_RATE_INSTANCE);
  if (!is_rate) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Expected rate (vertex/instance) after binding in "
                        "location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }
  VertexAttributeRate vertex_attribute_rate = token_type_to_vertex_attribute_rate(current_token.type);

  // offset
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_OFFSET) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Expected OFFSET after rate in location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }

  current_token = parser_advance_and_get_next_token(parser);
  bool is_offset = (current_token.type == TOKEN_TYPE_NUMBER) || (current_token.type == TOKEN_TYPE_TIGHTLY_PACKED);
  if (!is_offset) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Expected rate offset or TIGHTlY_PACKED after OFFSET in "
                        "location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }
  u32 offset = 0;
  if (current_token.type == TOKEN_TYPE_NUMBER) {
    offset = parse_integer_token(parser, current_token);
  } else {
    is_tightly_packed = true;
  }

  // double r brace
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Expected DOUBLE_R_BRACE after offset in location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }

  template_string_slice->location = location;
  template_string_slice->binding = binding;
  template_string_slice->end = current_token.start;

  // in
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_IN) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                        "Expected in (glsl keyword) after DOUBLE_R_BRACE ending location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }
  location_directive_parse.next_glsl_source_start = current_token.start;

  // type
  current_token = parser_advance_and_get_next_token(parser);
  GLSLType glsl_type = token_type_to_glsl_type(current_token.type);
  if (glsl_type == GLSL_TYPE_NULL) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                        "Expected glsl type after in (glsl keyword) in location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }

  // identifier
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_TEXT) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                        "Expected identifier after glsl type in location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }

  // ;
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_SEMICOLON) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                        "Expected ; after identifier in location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }

  if (glsl_type != GLSL_TYPE_NULL) {
    assert(shader_stage == SHADER_STAGE_VERTEX);
    location_directive_parse.vertex_attribute.location = location;
    location_directive_parse.vertex_attribute.binding = binding;
    location_directive_parse.vertex_attribute.rate = vertex_attribute_rate;
    location_directive_parse.vertex_attribute.glsl_type = glsl_type;
    location_directive_parse.vertex_attribute.offset = offset;
    location_directive_parse.vertex_attribute.is_valid = true;
    location_directive_parse.vertex_attribute.is_tightly_packed = is_tightly_packed;
  }

  current_token = parser_advance_and_get_next_token(parser);
  return location_directive_parse;
}

bool token_type_is_glsl_type(TokenType type) {
  return type == TOKEN_TYPE_FLOAT || type == TOKEN_TYPE_UINT || type == TOKEN_TYPE_VEC2 || type == TOKEN_TYPE_VEC3 ||
         type == TOKEN_TYPE_VEC4 || type == TOKEN_TYPE_MAT2 || type == TOKEN_TYPE_MAT3 || type == TOKEN_TYPE_MAT4;
}

u32 parse_array_size(Parser *parser) {
  Token current_token = parser_advance_and_get_next_token(parser);
  u32 array_length = 0; // you can't have arrays of size 0!

  // N
  if (current_token.type != TOKEN_TYPE_NUMBER) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_R_BRACKET,
                        "Expected number after opening bracket in array size, got %s",
                        token_type_to_string[current_token.type]);
    return array_length;
  }
  array_length = parse_integer_token(parser, current_token);

  // ]
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_R_BRACKET) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_R_BRACKET,
                        "Expected closing bracket after array size in array size, got %s",
                        token_type_to_string[current_token.type]);
    return array_length;
  }

  parser_advance(parser);
  return array_length;
}

// { typename identifier([N]); }
GLSLStructMemberList parse_glsl_struct_member_list(Parser *parser) {
  GLSLStructMemberList glsl_struct_member_list;
  memset(&glsl_struct_member_list, 0, sizeof(glsl_struct_member_list));

  assert(parser_get_current_token(parser).type == TOKEN_TYPE_L_BRACE);

  Token current_token = parser_advance_and_get_next_token(parser);
  while (parser_still_valid(parser)) {
    GLSLStructMember glsl_struct_member;
    glsl_struct_member.array_length = 0;

    if (!token_type_is_glsl_type(current_token.type)) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_R_BRACE, "Expected glsl type in glsl struct, got %s",
                          token_type_to_string[current_token.type]);
      return glsl_struct_member_list;
    }
    glsl_struct_member.type = token_type_to_glsl_type(current_token.type);

    current_token = parser_advance_and_get_next_token(parser);
    if (current_token.type != TOKEN_TYPE_TEXT) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_R_BRACE,
                          "Expected identifier after glsl type in struct, got %s",
                          token_type_to_string[current_token.type]);
      return glsl_struct_member_list;
    }
    glsl_struct_member.identifier = current_token.start;
    glsl_struct_member.identifier_length = current_token.text_length;

    // current token could either be opening bracket for array or semicolon
    // [
    current_token = parser_advance_and_get_next_token(parser);
    if (current_token.type == TOKEN_TYPE_L_BRACKET) {
      glsl_struct_member.array_length = parse_array_size(parser);
      if (glsl_struct_member.array_length == 0) {
        report_parser_error(parser, current_token.start, TOKEN_TYPE_R_BRACE,
                            "Failed to parse array size in GLSL Struct member.");
      }
    }

    current_token = parser_get_current_token(parser);
    if (current_token.type != TOKEN_TYPE_SEMICOLON) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_R_BRACE,
                          "Expected semicolon after identifier in glsl struct, got %s",
                          token_type_to_string[current_token.type]);
      return glsl_struct_member_list;
    }
    current_token = parser_advance_and_get_next_token(parser);

    assert(glsl_struct_member_list.num_members < MAX_NUM_STRUCT_MEMBERS);
    glsl_struct_member_list.members[glsl_struct_member_list.num_members++] = glsl_struct_member;

    if (current_token.type == TOKEN_TYPE_R_BRACE) {
      parser_advance(parser);
      return glsl_struct_member_list;
    }
  }

  // should be unreachable under normal circumstances
  report_parser_error(parser, current_token.start, TOKEN_TYPE_R_BRACE, "Reached EOF while parsing struct members");
  return glsl_struct_member_list;
}

//{{ SET_BINDING set binding }} uniform sampler2D identifier;
//{{ SET_BINDING set binding BUFFER_LABEL label}} uniform identifier { (type identifier;)! } idenitifer;
//  first identifier is a typename, the second is the instance identifier
SetBindingDirectiveParse parse_set_binding_directive(Parser *parser, TemplateStringSlice *template_string_slice) {
  SetBindingDirectiveParse directive_parse;
  directive_parse.was_successful = false;
  directive_parse.next_glsl_source_start = NULL;

  Token current_token = parser_get_current_token(parser);
  assert(current_token.type == TOKEN_TYPE_DIRECTIVE_SET_BINDING);

  // numeric set
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_NUMBER) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Expected numeric set after SET_BINDING in directive, got %s",
                        token_type_to_string[current_token.type]);
    return directive_parse;
  }
  u32 set = parse_integer_token(parser, current_token);

  // numeric binding
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_NUMBER) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Expected numeric binding after numeric set in SET_BINDING directive, got %s",
                        token_type_to_string[current_token.type]);
    return directive_parse;
  }

  u32 binding = parse_integer_token(parser, current_token);
  if (binding >= MAX_NUM_VERTEX_BINDINGS) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Found invalid binding in location directive. Binding is %u but maximum num bindings is %u\n.",
                        binding, MAX_NUM_VERTEX_BINDINGS);
    return directive_parse;
  }

  // if we get a sampler, the double R braces come here
  DescriptorType descriptor_type = DESCRIPTOR_TYPE_INVALID;
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type == TOKEN_TYPE_DOUBLE_R_BRACE) {

    current_token = parser_advance_and_get_next_token(parser);
    if (current_token.type != TOKEN_TYPE_UNIFORM) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                          "In SET_BINDING without buffer label, expected uniform after DOUBLE_R_BRACES, got %s",
                          token_type_to_string[current_token.type]);
      return directive_parse;
    }
    template_string_slice->end = current_token.start;
    directive_parse.next_glsl_source_start = current_token.start;

    current_token = parser_advance_and_get_next_token(parser);
    if (current_token.type != TOKEN_TYPE_SAMPLER2D) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                          "In SET_BINDING for sampler, expected sampler2D after uniform , got %s",
                          token_type_to_string[current_token.type]);
      return directive_parse;
    }
    // TODO extend for other sampler2D types
    descriptor_type = DESCRIPTOR_TYPE_SAMPLER2D;

    current_token = parser_advance_and_get_next_token(parser);
    if (current_token.type != TOKEN_TYPE_TEXT) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                          "In SET_BINDING for sampler, expected identifier after sampler2D, got %s",
                          token_type_to_string[current_token.type]);
      return directive_parse;
    }

    current_token = parser_advance_and_get_next_token(parser);
    u32 descriptor_count = 0;
    if (current_token.type == TOKEN_TYPE_L_BRACKET) {
      descriptor_count = parse_array_size(parser);
      if (descriptor_count == 0) {
        report_parser_error(parser, current_token.start, TOKEN_TYPE_R_BRACE,
                            "Failed to parse descriptor count in sampler2D.");
      }
    }

    current_token = parser_get_current_token(parser);
    if (current_token.type != TOKEN_TYPE_SEMICOLON) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                          "In SET_BINDING for sampler, expected semicolon after identifier, got %s",
                          token_type_to_string[current_token.type]);
      return directive_parse;
    }

    parser_advance(parser);
    directive_parse.was_successful = true;
    directive_parse.descriptor_binding.type = descriptor_type;
    directive_parse.descriptor_binding.name = NULL;
    directive_parse.descriptor_binding.name_length = 0;
    directive_parse.descriptor_binding.descriptor_count = descriptor_count;
    directive_parse.descriptor_binding.glsl_struct = NULL;
    directive_parse.descriptor_binding.is_valid = true;
    directive_parse.was_successful = true;
    directive_parse.set = set;
    directive_parse.binding = binding;
    template_string_slice->set = set;
    template_string_slice->binding = binding;
    return directive_parse;
  }
  // done parsing sampler

  // on BUFFER_LABEL
  // {{ SET_BINDING set binding BUFFER_LABEL label}} uniform identifier { (type identifier;)! } idenitifer;
  if (current_token.type != TOKEN_TYPE_BUFFER_LABEL) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "In SET_BINDING for uniform buffer, expected BUFFER_LABEL after binding, got %s",
                        token_type_to_string[current_token.type]);
    return directive_parse;
  }

  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_TEXT) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "In SET_BINDING for uniform buffer, expected identifier after BUFFER_LABEL, got %s",
                        token_type_to_string[current_token.type]);
    return directive_parse;
  }
  Token buffer_label_token = current_token;
  (void)buffer_label_token; // currently unused

  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                        "In SET_BINDING for uniform buffer, expected DOUBLE_R_BRACE after buffer_label, got %s",
                        token_type_to_string[current_token.type]);
    return directive_parse;
  }

  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_UNIFORM) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                        "In SET_BINDING for uniform buffer, expected uniform after DOUBLE_R_BRACES, got %s",
                        token_type_to_string[current_token.type]);
    return directive_parse;
  }
  template_string_slice->end = current_token.start;
  directive_parse.next_glsl_source_start = current_token.start;
  descriptor_type = DESCRIPTOR_TYPE_UNIFORM;

  // struct's typename
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_TEXT) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                        "In SET_BINDING for uniform buffer, expected identifier after uniform, got %s",
                        token_type_to_string[current_token.type]);
    return directive_parse;
  }

  GLSLStruct glsl_struct;
  glsl_struct.type_name = current_token.start;
  glsl_struct.type_name_length = current_token.text_length;

  // opening brace
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_L_BRACE) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                        "In SET_BINDING for uniform buffer, expected left brace after struct typename, got %s",
                        token_type_to_string[current_token.type]);
    return directive_parse;
  }
  // parsing members processes the closing brace
  glsl_struct.member_list = parse_glsl_struct_member_list(parser);
  template_string_slice->set = set;
  template_string_slice->binding = binding;

  current_token = parser_get_current_token(parser);
  if (current_token.type != TOKEN_TYPE_TEXT) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                        "In SET_BINDING for uniform buffer, expected identifier after struct members, got %s",
                        token_type_to_string[current_token.type]);
    return directive_parse;
  }
  const char *struct_instance_name = current_token.start;
  u32 struct_instance_name_length = current_token.text_length;

  current_token = parser_advance_and_get_next_token(parser);
  u32 descriptor_count = 0;
  if (current_token.type == TOKEN_TYPE_L_BRACKET) {
    descriptor_count = parse_array_size(parser);
    if (descriptor_count == 0) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_R_BRACE,
                          "Failed to parse descriptor count in uniform.");
    }
  }

  current_token = parser_get_current_token(parser);
  if (current_token.type != TOKEN_TYPE_SEMICOLON) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                        "In SET_BINDING for uniform buffer, expected semicolon after struct identifier, got %s",
                        token_type_to_string[current_token.type]);
    return directive_parse;
  }

  directive_parse.descriptor_binding.type = descriptor_type;
  directive_parse.descriptor_binding.name = struct_instance_name;
  directive_parse.descriptor_binding.name_length = struct_instance_name_length;
  // still need to check if we already found the same struct in a different shader
  directive_parse.descriptor_binding.descriptor_count = descriptor_count;
  directive_parse.descriptor_binding.glsl_struct = NULL;
  directive_parse.descriptor_binding.is_valid = true;

  directive_parse.was_successful = true;
  directive_parse.glsl_struct = glsl_struct;
  directive_parse.set = set;
  directive_parse.binding = binding;
  return directive_parse;
}

// {{ PUSH_CONSTANT }}
void parse_push_constant_directive(Parser *parser, TemplateStringSlice *template_string_slice) {
  (void)parser;
  (void)template_string_slice;
  return;
}

static inline const char *glsl_type_to_vertex_layout_enum_slice(GLSLType type) {
  switch (type) {
  case GLSL_TYPE_NULL:
    fprintf(stderr, "Vertex Attribute has GLSL_TYPE_NULL, but attempting to convert to VertexLayout name slice.\n");
    return NULL;
  case GLSL_TYPE_FLOAT:
    return "FLOAT";
  case GLSL_TYPE_UINT:
    return "UINT";
  case GLSL_TYPE_VEC2:
    return "VEC2";
  case GLSL_TYPE_VEC3:
    return "VEC3";
  case GLSL_TYPE_VEC4:
    return "VEC4";
  case GLSL_TYPE_MAT2:
    return "MAT2";
  case GLSL_TYPE_MAT3:
    return "MAT3";
  case GLSL_TYPE_MAT4:
    return "MAT4";
  default:
    fprintf(stderr, "Vertex Attribute has GLSL_TYPE_NULL, but attempting to convert to VertexLayout name slice.\n");
    return NULL;
  }
}

inline u32 num_digits(u32 n) {
  u32 res = 0;
  if (n == 0)
    return 1;

  while (n) {
    res++;
    n /= 10;
  }
  return res;
}

static inline const char *vertex_attribute_rate_to_string(VertexAttributeRate rate) {
  switch (rate) {
  case VERTEX_ATTRIBUTE_RATE_VERTEX:
    return "VERTEX";
  case VERTEX_ATTRIBUTE_RATE_INSTANCE:
    return "INSTANCE";

  case VERTEX_ATTRIBUTE_RATE_NULL:
  default:
    fprintf(stderr, "vertex_attribute_rate_to_string got an invalid rate enum.\n");
    return NULL;
  }
}

// assumption is that the vertex layout is valid, one of the stuipulations for which is
// that all attributes are at locations in consecutive order, starting from 0
// can use that assumption to impose a stopiing condition (or second validation condition)
static inline void populate_vertex_layout_name(VertexLayout *vertex_layout) {
  if (vertex_layout == NULL) {
    fprintf(stderr, "Tried to populate name for null vertex layout, returning.\n");
    return;
  }

  bool found_invalid = false;
  const char *prefix = "VERTEX_LAYOUT";
  memcpy(vertex_layout->name, prefix, strlen(prefix));
  u32 current_length = strlen(prefix);
  u32 current_binding = MAX_NUM_VERTEX_BINDINGS + 1; // init to impossible value so first comparison in loop is false

  for (u32 i = 0; i < MAX_NUM_VERTEX_ATTRIBUTES; i++) {
    if (!vertex_layout->attributes[i].is_valid) {
      found_invalid = true;
      continue;
    }

    // double checking no nonconsecutive valids
    if (found_invalid) {
      fprintf(stderr,
              "Attempting to populate Vertex Layout name, but found nonconsecutive valid attributes. Stopping.\n");
      return;
    }

    // if we find a new binding, add that to the name
    if (vertex_layout->attributes[i].binding != current_binding) {
      current_binding = vertex_layout->attributes[i].binding;
      const char *binding_string = "_BINDING";
      const char *rate_string = vertex_attribute_rate_to_string(vertex_layout->attributes[i].rate);
      u32 remaining_length = MAX_VERTEX_LAYOUT_NAME_LENGTH - current_length - 1;
      u32 next_slice_length = snprintf(NULL, 0, "%s%u%s", binding_string, current_binding, rate_string);
      if (next_slice_length >= remaining_length) {
        fprintf(stderr, "Not enough space to add next binding to vertex layout name, which currently is %s.\n",
                vertex_layout->name);
        return;
      }

      current_length += snprintf(vertex_layout->name + current_length, remaining_length, "%s%u%s", binding_string,
                                 current_binding, rate_string);
    }

    // need to append underscore and next type's slice
    const char *next_slice = glsl_type_to_vertex_layout_enum_slice(vertex_layout->attributes[i].glsl_type);
    u32 slice_length = strlen(next_slice);
    u32 next_length = slice_length + 1;
    // need to keep a null terminator, so stop at  max length - 1
    if (current_length + next_length >= MAX_VERTEX_LAYOUT_NAME_LENGTH - 1) {
      fprintf(stderr, "Not enough space to add next glsl type to vertex layout name, which currently is %s.\n",
              vertex_layout->name);
      return;
    }

    vertex_layout->name[current_length] = '_';
    memcpy(vertex_layout->name + current_length + 1, next_slice, slice_length);
    current_length += next_length;
  }

  // the assumption here is that the vertex layout we're populating was 0'd out at init time,
  // so this null terminator shouldn't be necessary. add out of abundance of caution
  vertex_layout->name[current_length] = '\0';
  vertex_layout->name_length = current_length;
}

static void populate_descriptor_set_layout_name(DescriptorSetLayout *descriptor_set_layout) {
  memset(descriptor_set_layout->name, 0, sizeof(descriptor_set_layout->name));

  const char *prefix = "DESCRIPTOR_SET_LAYOUT";
  memcpy(descriptor_set_layout->name, prefix, strlen(prefix));
  u32 current_length = strlen(prefix);

  const char *descriptor_type_to_string[NUM_DESCRIPTOR_TYPES] = {
      [DESCRIPTOR_TYPE_UNIFORM] = "UNIFORM",
      [DESCRIPTOR_TYPE_SAMPLER2D] = "SAMPLER2D",
  };

  // main loop
  for (u32 binding_id = 0; binding_id < MAX_NUM_DESCRIPTOR_BINDINGS; binding_id++) {
    const DescriptorBinding *binding = &descriptor_set_layout->bindings[binding_id];
    if (!binding->is_valid) {
      continue;
    }

    // meaningful stuff: binding #, binding type, binding count
    u32 remaining_length = MAX_DESCRIPTOR_SET_LAYOUT_NAME_LENGTH - current_length;
    u32 next_length = snprintf(NULL, 0, "_BIND%u_%s_ARR%u", binding_id, descriptor_type_to_string[binding->type],
                               binding->descriptor_count);
    if (next_length > remaining_length) {
      printf("Ran out of room for descriptor set layout name at %s, stopping.\n", descriptor_set_layout->name);
      return;
    }

    snprintf(descriptor_set_layout->name + current_length, remaining_length, "_BIND%u_%s_ARR%u", binding_id,
             descriptor_type_to_string[binding->type], binding->descriptor_count);
    current_length += next_length;
  }
}

static bool vertex_layout_validate_and_compute_offsets(VertexLayout *vertex_layout) {

  bool found_first_invalid_after_valids = false;
  u64 found_locations = 0;

  for (u32 i = 0; i < MAX_NUM_VERTEX_BINDINGS; i++) {
    vertex_layout->binding_rates[i] = VERTEX_ATTRIBUTE_RATE_NULL;
  }

  // we only accept vertex attribute locations in consecutive, ascending order, starting from 0
  // the edge case here is that the vertex layout has no in attributes, this loop checks that
  // if the first attribute is not present, then no attributes are present
  if (!vertex_layout->attributes[0].is_valid) {
    for (u32 i = 1; i < MAX_NUM_VERTEX_ATTRIBUTES; i++) {
      if (vertex_layout->attributes[i].is_valid) {
        fprintf(stderr, "Found vertex attribute with valid entries, but not starting at 0, stopping.\n");
        return false;
      }
    }

    // no valid entries to do anything for, valid null layout, just name it
    vertex_layout->name_length = strlen(vertex_layout_null_string);
    memcpy(vertex_layout->name, vertex_layout_null_string, vertex_layout->name_length);
    vertex_layout->name[vertex_layout->name_length] = '\0';
    return true;
  }

  // we have at least one valid entry at 0. Still need to validate that all other valid entries
  // are in consecutive order
  bool is_tightly_packed = vertex_layout->attributes[0].is_tightly_packed;
  u32 tentative_attribute_count = 1;
  for (u32 i = 1; i < MAX_NUM_VERTEX_ATTRIBUTES; i++) {
    if (!vertex_layout->attributes[i].is_valid) {
      found_first_invalid_after_valids = true;
      continue;
    } else {
      // in the valid case, just confirm that we haven't already found invalid attributes
      if (found_first_invalid_after_valids) {
        fprintf(stderr, "Found vertex attribute with non-consecutive attribute locations, stopping.\n");
        return false;
      }
      tentative_attribute_count++;
    }

    // validate rates are consistent per binding
    VertexAttributeRate rate = vertex_layout->attributes[i].rate;
    if (rate != VERTEX_ATTRIBUTE_RATE_NULL) {
      if (vertex_layout->binding_rates[vertex_layout->attributes[i].binding] == VERTEX_ATTRIBUTE_RATE_NULL) {
        vertex_layout->binding_rates[vertex_layout->attributes[i].binding] = rate;
      } else {
        if (vertex_layout->binding_rates[vertex_layout->attributes[i].binding] != rate) {
          fprintf(stderr, "In vertex layout, found binding %u with inconsistent rate.\n",
                  vertex_layout->attributes[i].binding);
          return false;
        }
      }
    }

    // validate no double locations
    // this will probably never fire, because we always write entries into their [location] entry
    // into the vertexlayout array. need to validate at insertion time
    u64 location_flag = (1ull << vertex_layout->attributes[i].location);
    if ((found_locations & location_flag) != 0) {
      fprintf(stderr, "In vertex layout, found location %u is double counted\n", vertex_layout->attributes[i].location);
      return false;
    }
    found_locations |= location_flag;

    // 2nd+ entry, look for inconsistencies in tight packing
    if (vertex_layout->attributes[i].is_tightly_packed != is_tightly_packed) {
      fprintf(stderr, "Found inconsistency in packing for vertex layout\n");
      return false;
    }
  }

  // at this point, we're valid
  vertex_layout->attribute_count = tentative_attribute_count;
  vertex_layout->binding_rates[vertex_layout->attributes[0].binding] = vertex_layout->attributes[0].rate;
  populate_vertex_layout_name(vertex_layout);
  // offsets were already given manually in the shader, done
  // TODO need semantics for stride when offsets are given manually
  if (!is_tightly_packed) {
    return true;
  }

  // using what will become the final stride per binding array to accumulate per attribute offsets
  // naming is overloaded
  memset(vertex_layout->binding_strides, 0, sizeof(vertex_layout->binding_strides));
  for (u32 i = 0; i < MAX_NUM_VERTEX_ATTRIBUTES; i++) {
    if (!vertex_layout->attributes[i].is_valid) {
      continue;
    }
    u32 current_binding = vertex_layout->attributes[i].binding;
    vertex_layout->attributes[i].offset = vertex_layout->binding_strides[current_binding];
    vertex_layout->binding_strides[current_binding] += glsl_type_to_size[vertex_layout->attributes[i].glsl_type];
  }

  vertex_layout->binding_count = 0;
  for (u32 i = 0; i < MAX_NUM_VERTEX_BINDINGS; i++) {
    if (vertex_layout->binding_strides[i] > 0) {
      vertex_layout->binding_count++;
    }
  }

  return true;
}

StructSearchResult search_for_matching_struct(const GLSLStruct *new_glsl_struct, const GLSLStruct *glsl_structs,
                                              u32 num_glsl_structs) {

  StructSearchResult search_result;
  search_result.matching_struct = NULL;
  search_result.found_mismatch = false;

  // bool found_mismatch = false;
  u32 name_length = new_glsl_struct->type_name_length;
  const char *name = new_glsl_struct->type_name;

  for (u32 i = 0; i < num_glsl_structs; i++) {
    const GLSLStruct *current_glsl_struct = &glsl_structs[i];
    bool name_is_same = (current_glsl_struct->type_name_length == name_length &&
                         (strncmp(current_glsl_struct->type_name, name, name_length) == 0));

    if (name_is_same) {
      search_result.matching_struct = current_glsl_struct;
      if (!glsl_struct_member_list_equals(&new_glsl_struct->member_list, &current_glsl_struct->member_list)) {
        // mismatch - same typenames but different layouts
        search_result.found_mismatch = true;
      }
      break;
    }
  }

  return search_result;
}

void parse_shader(ShaderToCompile shader_to_compile, ParsedShadersIR *parsed_shaders_ir) {

  Parser parser;
  parser.tokens = lex_string(shader_to_compile.source, shader_to_compile.source_length);
  parser.source = shader_to_compile.source;
  parser.source_length = shader_to_compile.source_length;
  parser.token_index = 0;

  u32 string_slice_index = 0;

  ParsedShader parsed_shader;
  memset(&parsed_shader, 0, sizeof(parsed_shader));
  parsed_shader.name = shader_to_compile.name;
  parsed_shader.stage = shader_to_compile.stage;

  VertexLayout vertex_layout;
  memset(&vertex_layout, 0, sizeof(vertex_layout));

  // within a shader, the set id uniquely identifies a set
  // across two different shaders, set 0 can be two different layouts, but in THIS shader
  // we parse now, it has to be one consistent thing
  DescriptorSetLayout descriptor_set_layouts[MAX_NUM_DESCRIPTOR_SET_LAYOUTS_PER_SHADER];
  memset(&descriptor_set_layouts, 0, sizeof(descriptor_set_layouts));

  TemplateStringSlice glsl_string_slice = new_template_string_slice(shader_to_compile.source);
  glsl_string_slice.type = DIRECTIVE_TYPE_GLSL_SOURCE;

  while (parser_still_valid(&parser)) {
    Token current_token = parser_get_current_token(&parser);
    if (current_token.type != TOKEN_TYPE_DOUBLE_L_BRACE) {
      parser_advance(&parser);
      continue;
    }

    // on a double L brace, end the glsl slice
    glsl_string_slice.end = current_token.start;
    if (glsl_string_slice.start != glsl_string_slice.end) {
      parsed_shader.template_slices[string_slice_index++] = glsl_string_slice;
    }

    TemplateStringSlice template_string_slice = new_template_string_slice(current_token.start);

    current_token = parser_advance_and_get_next_token(&parser);
    switch (current_token.type) {
      // version
    case TOKEN_TYPE_DIRECTIVE_VERSION: {
      template_string_slice.type = DIRECTIVE_TYPE_VERSION;
      parse_version_directive(&parser, &template_string_slice);
      glsl_string_slice.start = parser_get_current_token(&parser).start;
      break;
    }

      // location
    case TOKEN_TYPE_DIRECTIVE_LOCATION: {
      template_string_slice.type = DIRECTIVE_TYPE_LOCATION;

      LocationDirectiveParse location_directive_parse =
          parse_location_directive(&parser, shader_to_compile.stage, &template_string_slice);

      if (location_directive_parse.vertex_attribute.glsl_type != GLSL_TYPE_NULL) {
        assert(shader_to_compile.stage == SHADER_STAGE_VERTEX);
        if (vertex_layout.attributes[location_directive_parse.vertex_attribute.location].is_valid) {
          fprintf(stderr, "Found repeat location %u for vertex layout in %s.\n",
                  location_directive_parse.vertex_attribute.location, shader_to_compile.name);
        } else {
          vertex_layout.attributes[location_directive_parse.vertex_attribute.location] =
              location_directive_parse.vertex_attribute;
        }
      }

      glsl_string_slice.start = location_directive_parse.next_glsl_source_start;
      break;
    }

      // set binding
      // each set binding directive can handle one binding out of one set
      // just accumulate here, validate and assign pointers after parsing
    case TOKEN_TYPE_DIRECTIVE_SET_BINDING: {
      template_string_slice.type = DIRECTIVE_TYPE_SET_BINDING;
      SetBindingDirectiveParse set_binding_directive_parse =
          parse_set_binding_directive(&parser, &template_string_slice);
      glsl_string_slice.start = set_binding_directive_parse.next_glsl_source_start;
      set_binding_directive_parse.descriptor_binding.shader_stage = shader_to_compile.stage;

      // validate descriptor binding
      // just got a single binding for a particular set in a particular shader
      // it may describe a struct we haven't seen before, or may be a redefintion of one with the same name
      if (set_binding_directive_parse.descriptor_binding.type == DESCRIPTOR_TYPE_UNIFORM) {
        const GLSLStruct *new_glsl_struct = &set_binding_directive_parse.glsl_struct;
        StructSearchResult struct_search_result =
            search_for_matching_struct(&set_binding_directive_parse.glsl_struct, parsed_shaders_ir->glsl_structs,
                                       parsed_shaders_ir->num_glsl_structs);

        if (struct_search_result.found_mismatch == true) {
          fprintf(stderr, "Found mismatching definitions for struct %.*s. Originally found in %.*s, repeat in %.*s.\n",
                  new_glsl_struct->type_name_length, new_glsl_struct->type_name,
                  struct_search_result.matching_struct->discovered_shader_name_length,
                  struct_search_result.matching_struct->discovered_shader_name, shader_to_compile.name_length,
                  shader_to_compile.name);
          fprintf(stderr, "New struct is:\n");
          log_glsl_struct(stderr, new_glsl_struct);
          fprintf(stderr, "Old struct is:\n");
          log_glsl_struct(stderr, struct_search_result.matching_struct);
        }

        // discovered new struct
        if (struct_search_result.matching_struct != NULL) {
          set_binding_directive_parse.descriptor_binding.glsl_struct = struct_search_result.matching_struct;
        } else {
          set_binding_directive_parse.glsl_struct.discovered_shader_name = shader_to_compile.name;
          set_binding_directive_parse.glsl_struct.discovered_shader_name_length = shader_to_compile.name_length;
          parsed_shaders_ir->glsl_structs[parsed_shaders_ir->num_glsl_structs++] =
              set_binding_directive_parse.glsl_struct;
        }
      }

      // check that we haven't double defined this binding for this set
      u32 set = set_binding_directive_parse.set;
      u32 binding = set_binding_directive_parse.binding;
      if (descriptor_set_layouts[set].bindings[binding].is_valid) {
        fprintf(stderr, "Found repeat binding %u for set %u in shader %.*s.\n", binding, set,
                shader_to_compile.name_length, shader_to_compile.name);
      } else {
        descriptor_set_layouts[set].bindings[binding] = set_binding_directive_parse.descriptor_binding;
        descriptor_set_layouts[set].num_bindings++;
      }

      if (set_binding_directive_parse.descriptor_binding.type != DESCRIPTOR_TYPE_INVALID) {
        if (set_binding_directive_parse.descriptor_binding.type == NUM_DESCRIPTOR_TYPES) {
          fprintf(stderr, "Somehow descriptor set binding type is NUM_DESCRIPTOR TYPES, in shader %.*s.\n",
                  shader_to_compile.name_length, shader_to_compile.name);
        } else {
          parsed_shaders_ir->descriptor_binding_types[set_binding_directive_parse.descriptor_binding.type]++;
        }
      }
      break;
    }

      // push constant
    case TOKEN_TYPE_DIRECTIVE_PUSH_CONSTANT: {
      template_string_slice.type = DIRECTIVE_TYPE_PUSH_CONSTANT;
      parse_push_constant_directive(&parser, &template_string_slice);
      glsl_string_slice.start = parser_get_current_token(&parser).start;
      break;
    }
    default:
      assert(false);
    };

    parsed_shader.template_slices[string_slice_index++] = template_string_slice;
  }
  // finished parsing

  glsl_string_slice.end = shader_to_compile.source + shader_to_compile.source_length;
  if (glsl_string_slice.start != glsl_string_slice.end) {
    parsed_shader.template_slices[string_slice_index++] = glsl_string_slice;
  }

  // TODO rwlocks
  // validate vertex layouts
  if (shader_to_compile.stage == SHADER_STAGE_VERTEX) {
    if (vertex_layout_validate_and_compute_offsets(&vertex_layout)) {
      // if this vertex layout is valid, check that it doesn't match an existing one
      const VertexLayout *matching_vertex_layout_index = NULL;
      for (u32 i = 0; i < parsed_shaders_ir->num_vertex_layouts; i++) {
        if (vertex_layout_equals(&vertex_layout, &parsed_shaders_ir->vertex_layouts[i])) {
          matching_vertex_layout_index = &parsed_shaders_ir->vertex_layouts[i];
          break;
        }
      }

      if (matching_vertex_layout_index != NULL) {
        parsed_shader.vertex_layout = matching_vertex_layout_index;
      } else {
        parsed_shaders_ir->vertex_layouts[parsed_shaders_ir->num_vertex_layouts++] = vertex_layout;
        parsed_shader.vertex_layout = &parsed_shaders_ir->vertex_layouts[parsed_shaders_ir->num_vertex_layouts - 1];
      }
    } else {
      // release locks?
      fprintf(stderr, "Failed to validate vertex layout for %s.\n", shader_to_compile.name);
      return;
    }
  }

  // validate descriptor sets
  for (u32 i = 0; i < MAX_NUM_DESCRIPTOR_SET_LAYOUTS_PER_SHADER; i++) {
    DescriptorSetLayout *descriptor_set_layout = &descriptor_set_layouts[i];
    if (descriptor_set_layout->num_bindings > 0) {
      populate_descriptor_set_layout_name(descriptor_set_layout);
    }
  }

  parsed_shader.num_template_slices = string_slice_index;
  parsed_shaders_ir->parsed_shaders[parsed_shaders_ir->num_parsed_shaders++] = parsed_shader;
  token_vector_free(&parser.tokens);
}

ParsedShadersIR parse_all_shaders_and_populate_global_tables(const ShaderToCompileList *shader_to_compile_list) {
  ParsedShadersIR parsed_shaders_ir;
  memset(&parsed_shaders_ir, 0, sizeof(parsed_shaders_ir));

  for (u32 i = 0; i < shader_to_compile_list->num_shaders; i++) {
    parse_shader(shader_to_compile_list->shaders[i], &parsed_shaders_ir);
  }

  return parsed_shaders_ir;
}
