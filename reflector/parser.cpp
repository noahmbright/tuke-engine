#include "parser.h"
#include "filesystem_utils.h"
#include "reflection_data.h"
#include "reflector.h"

#include <assert.h>
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

  location_directive_parse.vertex_attribute.location = 0;
  location_directive_parse.vertex_attribute.binding = 0;
  location_directive_parse.vertex_attribute.offset = 0;
  location_directive_parse.vertex_attribute.rate = VERTEX_ATTRIBUTE_RATE_VERTEX;
  location_directive_parse.vertex_attribute.glsl_type = GLSL_TYPE_NULL;
  location_directive_parse.vertex_attribute.is_tightly_packed = false;

  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_NUMBER) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Expected numeric literal after LOCATION in location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }
  u32 location = parse_integer_token(parser, current_token);

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

    // move past }} and mark one past the end of this slice
    current_token = parser_advance_and_get_next_token(parser);
    template_string_slice->end = current_token.start;
    return location_directive_parse;
  }

  // in a vertex shader, can have either out or BINDING + the rest of the binding
  // check for }} and out
  if (current_token.type == TOKEN_TYPE_DOUBLE_R_BRACE) {
    current_token = parser_advance_and_get_next_token(parser);

    if (current_token.type != TOKEN_TYPE_OUT) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_OUT,
                          "Numeric location in LOCATION directive followed by DOUBLE_R_BRACE, expected out but got %s",
                          token_type_to_string[current_token.type]);
    }

    // move past }} and mark one past the end of this slice
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
  if (current_token.type == TOKEN_TYPE_NUMBER) {
    location_directive_parse.vertex_attribute.offset = parse_integer_token(parser, current_token);
  } else {
    location_directive_parse.vertex_attribute.is_tightly_packed = true;
  }

  // }}
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_DOUBLE_R_BRACE,
                        "Expected }} after offset in location directive, got %s",
                        token_type_to_string[current_token.type]);
    return location_directive_parse;
  }

  // printf("got location %d and binding %d\n", location, binding);
  template_string_slice->location = location;
  template_string_slice->binding = binding;
  template_string_slice->end = current_token.start;

  // in
  current_token = parser_advance_and_get_next_token(parser);
  if (current_token.type != TOKEN_TYPE_IN) {
    report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                        "Expected in (glsl keyword) after double R braces ending location directive, got %s",
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

  location_directive_parse.vertex_attribute.location = location;
  location_directive_parse.vertex_attribute.binding = binding;
  location_directive_parse.vertex_attribute.rate = vertex_attribute_rate;
  location_directive_parse.vertex_attribute.glsl_type = glsl_type;

  current_token = parser_advance_and_get_next_token(parser);
  return location_directive_parse;
}

bool token_type_is_glsl_type(TokenType type) {
  return type == TOKEN_TYPE_FLOAT || type == TOKEN_TYPE_UINT || type == TOKEN_TYPE_VEC2 || type == TOKEN_TYPE_VEC3 ||
         type == TOKEN_TYPE_VEC4 || type == TOKEN_TYPE_MAT2 || type == TOKEN_TYPE_MAT3 || type == TOKEN_TYPE_MAT4;
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
      current_token = parser_advance_and_get_next_token(parser);

      // N
      if (current_token.type != TOKEN_TYPE_NUMBER) {
        report_parser_error(parser, current_token.start, TOKEN_TYPE_R_BRACE,
                            "Expected number after opening bracket in array in glsl struct, got %s",
                            token_type_to_string[current_token.type]);
        return glsl_struct_member_list;
      }
      glsl_struct_member.array_length = parse_integer_token(parser, current_token);

      // ]
      current_token = parser_advance_and_get_next_token(parser);
      if (current_token.type != TOKEN_TYPE_R_BRACKET) {
        report_parser_error(parser, current_token.start, TOKEN_TYPE_R_BRACE,
                            "Expected closing bracket after array size in glsl struct, got %s",
                            token_type_to_string[current_token.type]);
        return glsl_struct_member_list;
      }

      current_token = parser_advance_and_get_next_token(parser);
    }

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

  // if we get a sampler, the double R braces come here
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

    current_token = parser_advance_and_get_next_token(parser);
    if (current_token.type != TOKEN_TYPE_TEXT) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                          "In SET_BINDING for sampler, expected identifier after sampler2D, got %s",
                          token_type_to_string[current_token.type]);
      return directive_parse;
    }

    current_token = parser_advance_and_get_next_token(parser);
    if (current_token.type != TOKEN_TYPE_SEMICOLON) {
      report_parser_error(parser, current_token.start, TOKEN_TYPE_SEMICOLON,
                          "In SET_BINDING for sampler, expected semicolon after identifier, got %s",
                          token_type_to_string[current_token.type]);
      return directive_parse;
    }

    parser_advance(parser);
    directive_parse.was_successful = true;
    return directive_parse;
  } // done parsing sampler

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
  (void)buffer_label_token;

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
  // parsing members expects the closing brace
  glsl_struct.member_list = parse_glsl_struct_member_list(parser);

  template_string_slice->set = set;
  template_string_slice->binding = binding;

  current_token = parser_advance_and_get_next_token(parser);

  directive_parse.was_successful = true;
  directive_parse.glsl_struct = glsl_struct;
  return directive_parse;
}

// {{ PUSH_CONSTANT }}
void parse_push_constant_directive(Parser *parser, TemplateStringSlice *template_string_slice) {
  (void)parser;
  (void)template_string_slice;
  return;
}

ParsedShader parse_shader(ShaderToCompile shader_to_compile) {

  Parser parser;
  parser.tokens = lex_string(shader_to_compile.source, shader_to_compile.source_length);
  parser.source = shader_to_compile.source;
  parser.source_length = shader_to_compile.source_length;
  parser.token_index = 0;

  u32 string_slice_index = 0;

  ParsedShader parsed_shader;
  memset(&parsed_shader, 0, sizeof(parsed_shader));

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
    case TOKEN_TYPE_DIRECTIVE_VERSION: {
      template_string_slice.type = DIRECTIVE_TYPE_VERSION;
      parse_version_directive(&parser, &template_string_slice);
      glsl_string_slice.start = parser_get_current_token(&parser).start;
      break;
    }

    case TOKEN_TYPE_DIRECTIVE_LOCATION: {
      template_string_slice.type = DIRECTIVE_TYPE_LOCATION;
      LocationDirectiveParse location_directive_parse =
          parse_location_directive(&parser, shader_to_compile.stage, &template_string_slice);

      if (location_directive_parse.vertex_attribute.glsl_type != GLSL_TYPE_NULL) {
        assert(shader_to_compile.stage == SHADER_STAGE_VERTEX);
        parsed_shader.vertex_layout.attributes[parsed_shader.vertex_layout.num_attributes++] =
            location_directive_parse.vertex_attribute;
      }

      log_vertex_attribute(location_directive_parse.vertex_attribute);
      glsl_string_slice.start = location_directive_parse.next_glsl_source_start;
      break;
    }

    case TOKEN_TYPE_DIRECTIVE_SET_BINDING: {
      template_string_slice.type = DIRECTIVE_TYPE_SET_BINDING;
      SetBindingDirectiveParse set_binding_directive_parse =
          parse_set_binding_directive(&parser, &template_string_slice);
      log_glsl_struct(&set_binding_directive_parse.glsl_struct);
      glsl_string_slice.start = set_binding_directive_parse.next_glsl_source_start;
      break;
    }

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

  glsl_string_slice.end = shader_to_compile.source + shader_to_compile.source_length;
  if (glsl_string_slice.start != glsl_string_slice.end) {
    parsed_shader.template_slices[string_slice_index++] = glsl_string_slice;
  }

  parsed_shader.num_template_slices = string_slice_index;
  token_vector_free(&parser.tokens);
  return parsed_shader;
}

ParsedShadersIR parse_all_shaders_and_populate_global_tables(const ShaderToCompileList *shader_to_compile_list) {
  ParsedShadersIR parsed_shaders_ir;
  memset(&parsed_shaders_ir, 0, sizeof(parsed_shaders_ir));
  parsed_shaders_ir.num_sliced_shaders = shader_to_compile_list->num_shaders;

  for (u32 i = 0; i < parsed_shaders_ir.num_sliced_shaders; i++) {
    const ShaderToCompile *shader_to_compile = &shader_to_compile_list->shaders[i];
    parsed_shaders_ir.sliced_shaders[i] = parse_shader(*shader_to_compile);
    parsed_shaders_ir.sliced_shaders[i].name = shader_to_compile->name;
    parsed_shaders_ir.sliced_shaders[i].stage = shader_to_compile->stage;
  }

  return parsed_shaders_ir;
}
