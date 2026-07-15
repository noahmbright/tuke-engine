#include "parser.h"
#include "filesystem_utils.h"
#include "reflector.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool labels_match(const char *a, const char *b, u32 a_len, u32 b_len) {
  return a_len == b_len && (strncmp(a, b, a_len) == 0);
}

static Token get_current_token(const Parser *parser) { return parser->tokens.tokens[parser->token_index]; }

static void advance(Parser *parser) { parser->token_index++; }

static Token get_next_token(Parser *parser) {
  advance(parser);
  return get_current_token(parser);
}

static bool still_valid(Parser *parser) { return parser->token_index < parser->tokens.size; }

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool is_octal_digit(char c) { return c >= '0' && c <= '7'; }

static bool is_hex_digit(char c) {
  bool is_upper_hex = (c >= 'A' && c <= 'F');
  bool is_lower_hex = (c >= 'a' && c <= 'f');
  return is_digit(c) || is_lower_hex || is_upper_hex;
}

static bool is_nondigit(char c) {
  bool is_upper = (c >= 'A' && c <= 'Z');
  bool is_lower = (c >= 'a' && c <= 'z');
  return c == '_' || is_lower || is_upper;
}

static TokenType string_slice_to_keyword_or_identifier(const char *string, u32 length) {
  // clang-format off
#define KW(kw, token) if (length == (sizeof(kw) - 1) && (strncmp(string, kw, length) == 0)) { return token; }
  KW("in",                  TOKEN_TYPE_IN)
  KW("out",                 TOKEN_TYPE_OUT)
  KW("version",             TOKEN_TYPE_VERSION)
  KW("void",                TOKEN_TYPE_VOID)
  KW("uniform",             TOKEN_TYPE_UNIFORM)
  KW("sampler",             TOKEN_TYPE_SAMPLER)
  KW("sampler2D",           TOKEN_TYPE_SAMPLER2D)
  KW("sampler2DArray",      TOKEN_TYPE_SAMPLER2D_ARRAY)
  KW("texture2D",           TOKEN_TYPE_TEXTURE2D)
  KW("image2D",             TOKEN_TYPE_IMAGE2D)
  KW("uint",                TOKEN_TYPE_UINT)
  KW("float",               TOKEN_TYPE_FLOAT)
  KW("vec2",                TOKEN_TYPE_VEC2)
  KW("vec3",                TOKEN_TYPE_VEC3)
  KW("vec4",                TOKEN_TYPE_VEC4)
  KW("mat2",                TOKEN_TYPE_MAT2)
  KW("mat3",                TOKEN_TYPE_MAT3)
  KW("mat4",                TOKEN_TYPE_MAT4)
  KW("VERSION",             TOKEN_TYPE_DIRECTIVE_VERSION)
  KW("LOCATION",            TOKEN_TYPE_DIRECTIVE_LOCATION)
  KW("SET_BINDING",         TOKEN_TYPE_DIRECTIVE_SET_BINDING)
  KW("PUSH_CONSTANT",       TOKEN_TYPE_DIRECTIVE_PUSH_CONSTANT)
  KW("VERTEX_SHADER",       TOKEN_TYPE_DIRECTIVE_VERTEX_SHADER)
  KW("BINDLESS",            TOKEN_TYPE_BINDLESS)
  KW("RATE_VERTEX",         TOKEN_TYPE_RATE_VERTEX)
  KW("RATE_INSTANCE",       TOKEN_TYPE_RATE_INSTANCE)
  KW("BINDING",             TOKEN_TYPE_BINDING)
  KW("OFFSET",              TOKEN_TYPE_OFFSET)
  KW("TIGHTLY_PACKED",      TOKEN_TYPE_TIGHTLY_PACKED)
  KW("SET_LABEL",           TOKEN_TYPE_SET_LABEL)
  KW("VERTEX_INDEX",        TOKEN_TYPE_DIRECTIVE_VERTEX_INDEX)
  KW("INSTANCE_INDEX",      TOKEN_TYPE_DIRECTIVE_INSTANCE_INDEX)
  // clang-format on
#undef KW
  return TOKEN_TYPE_TEXT;
}

static VertexAttributeRate token_type_to_vertex_attribute_rate(TokenType type) {
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

static GLSLType token_type_to_glsl_type(TokenType type) {
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
static u32 lex_number(const char *string, u32 string_length) {
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
        fprintf(stderr, "%s: found number with two decimal points\n", __func__);
      }

      if (seen_exponent) {
        fprintf(stderr, "%s: found number with decimal exponent\n", __func__);
      }

      seen_decimal_point = true;
      i++;
      continue;
    }

    // check exponents
    if (c == 'e' || c == 'E') {
      if (seen_exponent) {
        fprintf(stderr, "%s: found number with two exponents\n", __func__);
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
        fprintf(stderr, "%s: found number with two float suffixes\n", __func__);
      }
      seen_float_suffix = true;
      i++;
      continue;
    }

    if (c == 'u' || c == 'U') {
      if (seen_unsigned_suffix) {
        fprintf(stderr, "%s: found number with two unsigned suffixes\n", __func__);
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

static TokenVector lex_string(const char *string, u64 string_length) {
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

static void
report_parser_error(Parser *parser, const char *token_start, TokenType recovery_token_type, const char *fmt, ...) {
  const char *start = token_start;
  while (start > parser->input->source && *start != '\n') {
    start--;
  }
  if (*start == '\n') {
    start++;
  }

  const char *source_end = parser->input->source + parser->input->source_length;
  const char *end = token_start;
  while (end < source_end && *end != '\n') {
    end++;
  }
  char buffer[2048];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  printf("%sERROR%s: %s\n", RED, RESET, parser->input->source_path);
  printf("       %s\n", buffer);                      // print message
  printf("       %.*s\n", (int)(end - start), start); // print offending line

  while (still_valid(parser)) {
    Token cur_tok = get_current_token(parser);
    if (cur_tok.type == recovery_token_type) {
      break;
    }
    advance(parser);
  }

  parser->success = false;
}

static void parse_version_directive(Parser *parser, TemplateStringSlice *template_slice) {
  Token cur_tok = get_current_token(parser);
  assert(cur_tok.type == TOKEN_TYPE_DIRECTIVE_VERSION);

  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "Expected DOUBLE_R_BRACE after VERSION in version directive, got %s", token_type_to_string[cur_tok.type]
    );
    return;
  }

  cur_tok = get_next_token(parser);
  template_slice->end = cur_tok.start;
}

static void parse_vertex_index_directive(Parser *parser, TemplateStringSlice *template_slice) {
  Token cur_tok = get_current_token(parser);
  assert(cur_tok.type == TOKEN_TYPE_DIRECTIVE_VERTEX_INDEX);

  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "Expected DOUBLE_R_BRACE after VERTEX_INDEX in vertex index directive, got %s",
        token_type_to_string[cur_tok.type]
    );
    return;
  }

  cur_tok = get_next_token(parser);
  template_slice->end = cur_tok.start;
}

void parse_instance_index_directive(Parser *parser, TemplateStringSlice *template_string_slice) {
  Token cur_tok = get_current_token(parser);
  assert(cur_tok.type == TOKEN_TYPE_DIRECTIVE_INSTANCE_INDEX);

  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "Expected DOUBLE_R_BRACE after INSTANCE_INDEX in instance index directive, got %s",
        token_type_to_string[cur_tok.type]
    );
    return;
  }

  cur_tok = get_next_token(parser);
  template_string_slice->end = cur_tok.start;
}

static u32 parse_integer_token(Parser *parser, Token token) {
  char buf[64];
  if (token.text_length >= sizeof(buf)) {
    report_parser_error(
        parser, token.start, TOKEN_TYPE_DOUBLE_R_BRACE, "Parsing integer token, but data was too long to buffer"
    );
    return 0;
  }

  memcpy(buf, token.start, token.text_length);
  buf[token.text_length] = '\0'; // NUL-terminate

  return (int)strtol(buf, NULL, 10);
}

// {{ LOCATION 0 BINDING 0 RATE_VERTEX OFFSET TIGHTLY_PACKED }} in type identifier;
// or
// {{ LOCATION 0 }} - for fragment and compute shaders, or
static LocationDirectiveParse
parse_location_directive(Parser *parser, ShaderStage shader_stage, TemplateStringSlice *template_string_slice) {
  Token cur_tok = get_current_token(parser);
  assert(cur_tok.type == TOKEN_TYPE_DIRECTIVE_LOCATION);

  LocationDirectiveParse location_parse = {
      .next_glsl_source_start = NULL,
      .found_repeat_attribute = false,
      .vertex_attribute.location = 0,
      .vertex_attribute.binding = 0,
      .vertex_attribute.offset = 0,
      .vertex_attribute.rate = VERTEX_ATTRIBUTE_RATE_VERTEX,
      .vertex_attribute.glsl_type = GLSL_TYPE_NULL,
      .vertex_attribute.is_tightly_packed = false,
      .vertex_attribute.is_valid = false,
  };
  bool is_tightly_packed = false;

  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_NUMBER) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "Expected numeric literal after LOCATION in location directive, got %s", token_type_to_string[cur_tok.type]
    );
    return location_parse;
  }
  u32 location = parse_integer_token(parser, cur_tok);
  template_string_slice->location = location;

  // either BINDING or the end for outs/fragment/compute
  cur_tok = get_next_token(parser);
  if (shader_stage != SHADER_STAGE_VERTEX) {
    if (cur_tok.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
      report_parser_error(
          parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
          "Expected DOUBLE_R_BRACE after location in location directive "
          "for a non-vertex shader, got %s",
          token_type_to_string[cur_tok.type]
      );
      return location_parse;
    }

    // move past double r brace and mark one past the end of this slice
    cur_tok = get_next_token(parser);
    location_parse.next_glsl_source_start = cur_tok.start;
    template_string_slice->end = cur_tok.start;
    return location_parse;
  }

  // in a vertex shader, can have either out or BINDING + the rest of the binding
  // check for double r brace and out
  if (cur_tok.type == TOKEN_TYPE_DOUBLE_R_BRACE) {
    cur_tok = get_next_token(parser);

    if (cur_tok.type != TOKEN_TYPE_OUT) {
      report_parser_error(
          parser, cur_tok.start, TOKEN_TYPE_OUT,
          "Numeric location in LOCATION directive followed by DOUBLE_R_BRACE, expected out but got %s",
          token_type_to_string[cur_tok.type]
      );
    }

    // move past double r brace and mark one past the end of this slice
    template_string_slice->end = cur_tok.start;
    location_parse.next_glsl_source_start = cur_tok.start;
    return location_parse;
  }

  // BINDING
  if (cur_tok.type != TOKEN_TYPE_BINDING) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "Expected BINDING after location in location directive, got %s", token_type_to_string[cur_tok.type]
    );
    return location_parse;
  }

  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_NUMBER) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "Expected numeric literal after BINDING in location directive, got %s", token_type_to_string[cur_tok.type]
    );
    return location_parse;
  }
  u32 binding = parse_integer_token(parser, cur_tok);

  // rate
  cur_tok = get_next_token(parser);
  bool is_rate = (cur_tok.type == TOKEN_TYPE_RATE_VERTEX) || (cur_tok.type == TOKEN_TYPE_RATE_INSTANCE);
  if (!is_rate) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "Expected rate (vertex/instance) after binding in "
        "location directive, got %s",
        token_type_to_string[cur_tok.type]
    );
    return location_parse;
  }
  VertexAttributeRate vertex_attribute_rate = token_type_to_vertex_attribute_rate(cur_tok.type);

  // offset
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_OFFSET) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE, "Expected OFFSET after rate in location directive, got %s",
        token_type_to_string[cur_tok.type]
    );
    return location_parse;
  }

  cur_tok = get_next_token(parser);
  bool is_offset = (cur_tok.type == TOKEN_TYPE_NUMBER) || (cur_tok.type == TOKEN_TYPE_TIGHTLY_PACKED);
  if (!is_offset) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "Expected rate offset or TIGHTlY_PACKED after OFFSET in "
        "location directive, got %s",
        token_type_to_string[cur_tok.type]
    );
    return location_parse;
  }
  u32 offset = 0;
  if (cur_tok.type == TOKEN_TYPE_NUMBER) {
    offset = parse_integer_token(parser, cur_tok);
  } else {
    is_tightly_packed = true;
  }

  // double r brace
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "Expected DOUBLE_R_BRACE after offset in location directive, got %s", token_type_to_string[cur_tok.type]
    );
    return location_parse;
  }

  template_string_slice->location = location;
  template_string_slice->binding = binding;
  template_string_slice->end = cur_tok.start;

  // in
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_IN) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON,
        "Expected in (glsl keyword) after DOUBLE_R_BRACE ending location directive, got %s",
        token_type_to_string[cur_tok.type]
    );
    return location_parse;
  }
  location_parse.next_glsl_source_start = cur_tok.start;

  // type
  cur_tok = get_next_token(parser);
  GLSLType glsl_type = token_type_to_glsl_type(cur_tok.type);
  if (glsl_type == GLSL_TYPE_NULL) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON,
        "Expected glsl type after in (glsl keyword) in location directive, got %s", token_type_to_string[cur_tok.type]
    );
    return location_parse;
  }

  // identifier
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_TEXT) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON,
        "Expected identifier after glsl type in location directive, got %s", token_type_to_string[cur_tok.type]
    );
    return location_parse;
  }

  // ;
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_SEMICOLON) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON, "Expected ; after identifier in location directive, got %s",
        token_type_to_string[cur_tok.type]
    );
    return location_parse;
  }

  if (glsl_type != GLSL_TYPE_NULL) {
    assert(shader_stage == SHADER_STAGE_VERTEX);
    location_parse.vertex_attribute = {
        .location = (u8)location,
        .binding = (u8)binding,
        .rate = vertex_attribute_rate,
        .glsl_type = glsl_type,
        .offset = offset,
        .is_valid = true,
        .is_tightly_packed = is_tightly_packed,
    };
  }

  cur_tok = get_next_token(parser);
  return location_parse;
}

bool token_type_is_glsl_type(TokenType type) {
  return type == TOKEN_TYPE_FLOAT || type == TOKEN_TYPE_UINT || type == TOKEN_TYPE_VEC2 || type == TOKEN_TYPE_VEC3 ||
         type == TOKEN_TYPE_VEC4 || type == TOKEN_TYPE_MAT2 || type == TOKEN_TYPE_MAT3 || type == TOKEN_TYPE_MAT4;
}

// Called when current token is left bracket
static bool parse_array_size(Parser *parser, u32 *out_size) {
  Token cur_tok = get_next_token(parser);
  *out_size = 0; // you can't have arrays of size 0!

  // N
  if (cur_tok.type != TOKEN_TYPE_NUMBER) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_R_BRACKET, "Expected number after opening bracket in array size, got %s",
        token_type_to_string[cur_tok.type]
    );
    return false;
  }
  *out_size = parse_integer_token(parser, cur_tok);

  // ]
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_R_BRACKET) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_R_BRACKET, "Expected closing bracket after array size in array size, got %s",
        token_type_to_string[cur_tok.type]
    );
    return false;
  }

  advance(parser);
  return true;
}

// { typename identifier([N]); }
static void parse_glsl_struct_member_list(Parser *parser, GLSLStruct *glsl_struct) {
  assert(get_current_token(parser).type == TOKEN_TYPE_L_BRACE);
  glsl_struct->num_members = 0;

  Token cur_tok = get_next_token(parser);
  while (still_valid(parser)) {
    GLSLStructMember member;
    member.array_length = 0;

    if (!token_type_is_glsl_type(cur_tok.type)) {
      report_parser_error(
          parser, cur_tok.start, TOKEN_TYPE_R_BRACE, "Expected glsl type in glsl struct, got %s",
          token_type_to_string[cur_tok.type]
      );
      return;
    }
    member.type = token_type_to_glsl_type(cur_tok.type);

    cur_tok = get_next_token(parser);
    if (cur_tok.type != TOKEN_TYPE_TEXT) {
      report_parser_error(
          parser, cur_tok.start, TOKEN_TYPE_R_BRACE, "Expected identifier after glsl type in struct, got %s",
          token_type_to_string[cur_tok.type]
      );
      return;
    }
    member.identifier = cur_tok.start;
    member.identifier_length = cur_tok.text_length;

    // current token could either be opening bracket for array or semicolon
    // [
    cur_tok = get_next_token(parser);
    if (cur_tok.type == TOKEN_TYPE_L_BRACKET) {
      bool parsed_array_len = parse_array_size(parser, &member.array_length);
      if (parsed_array_len == false) {
        report_parser_error(
            parser, cur_tok.start, TOKEN_TYPE_R_BRACE, "Failed to parse array size in GLSL Struct member."
        );
      }
    }

    cur_tok = get_current_token(parser);
    if (cur_tok.type != TOKEN_TYPE_SEMICOLON) {
      report_parser_error(
          parser, cur_tok.start, TOKEN_TYPE_R_BRACE, "Expected semicolon after identifier in glsl struct, got %s",
          token_type_to_string[cur_tok.type]
      );
      return;
    }
    cur_tok = get_next_token(parser);

    assert(member_list.num_members < MAX_NUM_STRUCT_MEMBERS);
    glsl_struct->members[glsl_struct->num_members++] = member;

    if (cur_tok.type == TOKEN_TYPE_R_BRACE) {
      advance(parser);
      return;
    }
  }

  // should be unreachable under normal circumstances
  report_parser_error(parser, cur_tok.start, TOKEN_TYPE_R_BRACE, "Reached EOF while parsing struct members");
}

// struct_declaration:
// Typename { (typename identifier([array_size]); )! } instance_identifier
//
// Considering splitting GLSLStruct into GLSLType + name and array size
static bool parse_struct(Parser *parser, GLSLStruct *glsl_struct, const char **name, u32 *name_len, u32 *out_count) {
  Token cur_tok = get_current_token(parser);
  assert(cur_tok.type == TOKEN_TYPE_TEXT);

  // On struct typename
  *glsl_struct = {
      .type_name = cur_tok.start,
      .type_name_len = cur_tok.text_length,
  };

  // Opening brace
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_L_BRACE) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON,
        "Parsing struct: expected left brace after struct typename, got %s", token_type_to_string[cur_tok.type]
    );
    return false;
  }

  // parsing members processes the closing brace
  parse_glsl_struct_member_list(parser, glsl_struct);

  // Struct instance identifier
  cur_tok = get_current_token(parser);
  if (cur_tok.type != TOKEN_TYPE_TEXT) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON, "Parsing struct: expected identifier after struct members, got %s",
        token_type_to_string[cur_tok.type]
    );
    return false;
  }
  Token instance_tok = cur_tok;

  cur_tok = get_next_token(parser);
  *out_count = 0;
  if (cur_tok.type == TOKEN_TYPE_L_BRACKET) {
    bool parsed_array_size = parse_array_size(parser, out_count);
    if (!parsed_array_size) {
      report_parser_error(parser, cur_tok.start, TOKEN_TYPE_R_BRACE, "Failed to parse descriptor count in uniform.");
    }
  } else {
    *out_count = 1;
  }

  *name = instance_tok.start;
  *name_len = instance_tok.text_length;
  return true;
}

//{{ SET_BINDING (BINDLESS)set binding SET_LABEL label}} uniform (sampler2D|sampler2DArray) identifier;
//{{ SET_BINDING (BINDLESS) set binding SET_LABEL label}} uniform struct_declaration;
//  first identifier is a typename, the second is the instance identifier
static SetBindingDirectiveParse
parse_set_binding_directive(Parser *parser, TemplateStringSlice *template_string_slice) {
  SetBindingDirectiveParse directive_parse = {
      .descriptor_type = DESCRIPTOR_TYPE_INVALID,
      .was_successful = false,
      .next_glsl_source_start = NULL,
      .set_name = NULL,
      .set_name_len = 0,
  };

  Token cur_tok = get_current_token(parser);
  assert(cur_tok.type == TOKEN_TYPE_DIRECTIVE_SET_BINDING);

  // Check BINDLESS, then check numeric binding
  cur_tok = get_next_token(parser);
  if (cur_tok.type == TOKEN_TYPE_BINDLESS) {
    directive_parse.bindless = true;
    cur_tok = get_next_token(parser);
  }

  // numeric binding TODO remove bc unused
  if (cur_tok.type != TOKEN_TYPE_NUMBER) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "Expected numeric binding after numeric set in SET_BINDING directive, got %s",
        token_type_to_string[cur_tok.type]
    );
    return directive_parse;
  }

  u32 binding = parse_integer_token(parser, cur_tok);
  template_string_slice->binding = binding;
  if (binding >= MAX_NUM_VERTEX_BINDINGS) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "Found invalid binding in location directive. Binding is %u but maximum num bindings is %u\n.", binding,
        MAX_NUM_VERTEX_BINDINGS
    );
    return directive_parse;
  }

  // SET_LABEL
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_SET_LABEL) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "In SET_BINDING for uniform buffer, expected SET_LABEL after binding, got %s",
        token_type_to_string[cur_tok.type]
    );
    return directive_parse;
  }

  // SET_LABEL's textual identifier
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_TEXT) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_DOUBLE_R_BRACE,
        "In SET_BINDING for uniform buffer, expected identifier after SET_LABEL, got %s",
        token_type_to_string[cur_tok.type]
    );
    return directive_parse;
  }
  Token set_label_tok = cur_tok;
  directive_parse.set_name = set_label_tok.start;
  directive_parse.set_name_len = set_label_tok.text_length;

  // TOKEN_TYPE_DOUBLE_R_BRACE
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON,
        "In SET_BINDING for uniform buffer, expected DOUBLE_R_BRACE after buffer_label, got %s",
        token_type_to_string[cur_tok.type]
    );
    return directive_parse;
  }

  // TOKEN_TYPE_UNIFORM
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_UNIFORM) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON,
        "In SET_BINDING for uniform buffer, expected uniform after DOUBLE_R_BRACES, got %s",
        token_type_to_string[cur_tok.type]
    );
    return directive_parse;
  }
  template_string_slice->end = cur_tok.start;
  directive_parse.next_glsl_source_start = cur_tok.start;

  // Diverge here - either sampler2D(Array) or uniform struct's typename
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_SAMPLER2D && cur_tok.type != TOKEN_TYPE_SAMPLER2D_ARRAY &&
      cur_tok.type != TOKEN_TYPE_TEXT) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON,
        "In SET_BINDING, expected sampler2D(Array) or structure type after DOUBLE_R_BRACKETS, got %s",
        token_type_to_string[cur_tok.type]
    );
    return directive_parse;
  }

  // Sampler/Sampler Array
  if (cur_tok.type == TOKEN_TYPE_SAMPLER2D || cur_tok.type == TOKEN_TYPE_SAMPLER2D_ARRAY) {
    DescriptorType descriptor_type =
        (cur_tok.type == TOKEN_TYPE_SAMPLER2D) ? DESCRIPTOR_TYPE_SAMPLER2D : DESCRIPTOR_TYPE_SAMPLER2D_ARRAY;
    const char *expected = token_type_to_string[cur_tok.type];
    // Identifier
    cur_tok = get_next_token(parser);
    if (cur_tok.type != TOKEN_TYPE_TEXT) {
      report_parser_error(
          parser, cur_tok.start, TOKEN_TYPE_SEMICOLON,
          "In SET_BINDING for sampler, expected identifier after %s, got %s", expected,
          token_type_to_string[cur_tok.type]
      );
      return directive_parse;
    }
    Token instance_tok = cur_tok;

    // Array size
    cur_tok = get_next_token(parser);
    u32 descriptor_count = 0;
    if (cur_tok.type == TOKEN_TYPE_L_BRACKET) {
      if (directive_parse.bindless) {
        cur_tok = get_next_token(parser);
        if (cur_tok.type != TOKEN_TYPE_R_BRACKET) { // TODO Can I support sampler2DArrays?
          report_parser_error(
              parser, cur_tok.start, TOKEN_TYPE_R_BRACE, "Expected empty array size in BINDLESS Sampler"
          );
        }
        cur_tok = get_next_token(parser);
      } else {
        bool parsed_array_size = parse_array_size(parser, &descriptor_count);
        if (!parsed_array_size) {
          report_parser_error(
              parser, cur_tok.start, TOKEN_TYPE_R_BRACE, "Failed to parse descriptor count in sampler2D."
          );
        }
      }
    } else {
      descriptor_count = 1;
    }

    cur_tok = get_current_token(parser);
    if (cur_tok.type != TOKEN_TYPE_SEMICOLON) {
      report_parser_error(
          parser, cur_tok.start, TOKEN_TYPE_SEMICOLON,
          "In SET_BINDING for sampler, expected semicolon after identifier, got %s", token_type_to_string[cur_tok.type]
      );
      return directive_parse;
    }

    // Not happy about duplicate assignment here and in the uniform path
    advance(parser);
    directive_parse.was_successful = true;
    directive_parse.binding = binding;
    directive_parse.descriptor_type = descriptor_type;
    directive_parse.descriptor_count = descriptor_count;
    directive_parse.instance_name = instance_tok.start;
    directive_parse.instance_name_len = instance_tok.text_length;

    // TODO Don't like the repetition between the directive parse and slice here.
    template_string_slice->binding = binding;
    template_string_slice->descriptor_type = descriptor_type;
    return directive_parse;
  }

  // On struct typename
  GLSLStruct glsl_struct;
  u32 struct_identifier_len = 0;
  const char *struct_identifier = NULL;
  u32 descriptor_count = 0;
  bool parsed_struct_type =
      parse_struct(parser, &glsl_struct, &struct_identifier, &struct_identifier_len, &descriptor_count);
  if (!parsed_struct_type) {
    return directive_parse;
  }

  cur_tok = get_current_token(parser);
  if (cur_tok.type != TOKEN_TYPE_SEMICOLON) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON,
        "In SET_BINDING for uniform buffer, expected semicolon after struct identifier, got %s",
        token_type_to_string[cur_tok.type]
    );
    return directive_parse;
  }

  template_string_slice->descriptor_type = DESCRIPTOR_TYPE_UNIFORM;

  directive_parse.descriptor_type = DESCRIPTOR_TYPE_UNIFORM;
  directive_parse.descriptor_count = descriptor_count;
  directive_parse.was_successful = true;
  directive_parse.glsl_struct = glsl_struct;
  directive_parse.binding = binding;
  directive_parse.instance_name = struct_identifier;
  directive_parse.instance_name_len = struct_identifier_len;
  return directive_parse;
}

// No support for arrays of push constant structs.
// {{ PUSH_CONSTANT }} uniform struct_type identifier;
static PushConstantDirectiveParse
parse_push_constant_directive(Parser *parser, TemplateStringSlice *template_string_slice) {
  Token cur_tok = get_current_token(parser);
  assert(cur_tok.type == TOKEN_TYPE_DIRECTIVE_PUSH_CONSTANT);

  PushConstantDirectiveParse parse = {
      .was_successful = false,
      .next_glsl_source_start = NULL,
  };

  // TOKEN_TYPE_DOUBLE_R_BRACE
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON,
        "In PUSH_CONSTANT, expected DOUBLE_R_BRACE after buffer_label, got %s", token_type_to_string[cur_tok.type]
    );
    return parse;
  }

  // TOKEN_TYPE_UNIFORM
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_UNIFORM) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON, "In PUSH_CONSTANT, expected uniform after DOUBLE_R_BRACES, got %s",
        token_type_to_string[cur_tok.type]
    );
    return parse;
  }
  template_string_slice->end = cur_tok.start;
  parse.next_glsl_source_start = cur_tok.start;

  // On struct typename
  cur_tok = get_next_token(parser);

  GLSLStruct glsl_struct;
  u32 struct_identifier_len = 0;
  const char *struct_identifier = NULL;
  u32 descriptor_count = 0;
  bool parsed_struct_type =
      parse_struct(parser, &glsl_struct, &struct_identifier, &struct_identifier_len, &descriptor_count);
  if (!parsed_struct_type) {
    return parse;
  }

  if (descriptor_count != 1) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON, "In PUSH_CONSTANT, got array of instances. Not supported."
    );
    return parse;
  }

  parse.was_successful = true;
  parse.glsl_struct = glsl_struct;
  parse.instance_name = struct_identifier;
  parse.instance_name_len = struct_identifier_len;
  return parse;
}

// {{ VERTEX_SHADER program_name }}
static VertexShaderDirectiveParse parse_vertex_shader_directive(Parser *parser, TemplateStringSlice *template_slice) {
  Token cur_tok = get_current_token(parser);
  assert(cur_tok.type == TOKEN_TYPE_DIRECTIVE_VERTEX_SHADER);

  VertexShaderDirectiveParse parse = {
      .was_successful = false,
      .next_glsl_source_start = NULL,
  };

  // program name
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_TEXT) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON, "Expected VERTEX_SHADER name, got %s",
        token_type_to_string[cur_tok.type]
    );
    return parse;
  }
  parse.program_name = cur_tok.start;
  parse.program_name_len = cur_tok.text_length;

  // TOKEN_TYPE_DOUBLE_R_BRACE
  cur_tok = get_next_token(parser);
  if (cur_tok.type != TOKEN_TYPE_DOUBLE_R_BRACE) {
    report_parser_error(
        parser, cur_tok.start, TOKEN_TYPE_SEMICOLON,
        "In VERTEX_PARSE, expected DOUBLE_R_BRACE after program name, got %s", token_type_to_string[cur_tok.type]
    );
    return parse;
  }

  cur_tok = get_next_token(parser);
  template_slice->end = cur_tok.start;
  parse.next_glsl_source_start = cur_tok.start;
  parse.was_successful = true;
  return parse;
}

static const char *glsl_type_to_vertex_layout_enum_slice(GLSLType type) {
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

static const char *vertex_attribute_rate_to_string(VertexAttributeRate rate) {
  switch (rate) {
  case VERTEX_ATTRIBUTE_RATE_VERTEX:
    return "VERTEX";
  case VERTEX_ATTRIBUTE_RATE_INSTANCE:
    return "INSTANCE";

  case VERTEX_ATTRIBUTE_RATE_NULL:
  default:
    fprintf(stderr, "in_parsing, vertex_attribute_rate_to_string got an invalid rate enum.\n");
    return NULL;
  }
}

// assumption is that the vertex layout is valid, one of the stuipulations for which is
// that all attributes are at locations in consecutive order, starting from 0
// can use that assumption to impose a stopiing condition (or second validation condition)
static void populate_vertex_layout_name(VertexLayout *vertex_layout) {
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
      fprintf(
          stderr, "Attempting to populate Vertex Layout name, but found nonconsecutive valid attributes. Stopping.\n"
      );
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
        fprintf(
            stderr, "Not enough space to add next binding to vertex layout name, which currently is %s.\n",
            vertex_layout->name
        );
        return;
      }

      current_length += snprintf(
          vertex_layout->name + current_length, remaining_length, "%s%u%s", binding_string, current_binding, rate_string
      );
    }

    // need to append underscore and next type's slice
    const char *next_slice = glsl_type_to_vertex_layout_enum_slice(vertex_layout->attributes[i].glsl_type);
    u32 slice_length = strlen(next_slice);
    u32 next_length = slice_length + 1;
    // need to keep a null terminator, so stop at  max length - 1
    if (current_length + next_length >= MAX_VERTEX_LAYOUT_NAME_LENGTH - 1) {
      fprintf(
          stderr, "Not enough space to add next glsl type to vertex layout name, which currently is %s.\n",
          vertex_layout->name
      );
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
          fprintf(
              stderr, "In vertex layout, found binding %u with inconsistent rate.\n",
              vertex_layout->attributes[i].binding
          );
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

static bool member_list_equals(const GLSLStruct *left, const GLSLStruct *right) {
  if (left->num_members != right->num_members) {
    return false;
  }

  for (u32 i = 0; i < left->num_members; i++) {
    const GLSLStructMember *left_member = &left->members[i];
    const GLSLStructMember *right_member = &right->members[i];

    bool name_lens_are_same = (left_member->identifier_length == right_member->identifier_length);
    if (!name_lens_are_same) {
      return false;
    }

    bool names_are_same =
        (strncmp(left_member->identifier, right_member->identifier, left_member->identifier_length) == 0);
    bool array_lens_are_same = left_member->array_length == right_member->array_length;
    bool types_are_same = left_member->type == right_member->type;
    if (!names_are_same || !array_lens_are_same || !types_are_same) {
      return false;
    }
  }

  return true;
}

static const GLSLStruct *search_structs(const GLSLStruct *new_struct, const ParsedShadersIR *ir) {
  u32 name_len = new_struct->type_name_len;
  const char *name = new_struct->type_name;

  for (u32 i = 0; i < ir->num_structs; i++) {
    const GLSLStruct *old_struct = &ir->structs[i];
    bool name_is_same = labels_match(old_struct->type_name, name, old_struct->type_name_len, name_len);
    if (name_is_same) {
      return old_struct;
    }
  }

  return NULL;
}

static DescriptorSetLayout *search_descriptor_set_layouts(ParsedShadersIR *ir, const char *label, u32 label_len) {
  for (u32 i = 0; i < ir->num_descriptor_set_layouts; i++) {
    DescriptorSetLayout *layout = &ir->descriptor_set_layouts[i];
    bool name_is_same = labels_match(label, layout->name, label_len, layout->name_len);
    if (name_is_same) {
      return layout;
    }
  }

  return NULL;
}

// Return is success. Out parameter is if the parsed binding is new.
// Push a binding at a numeric index from sb_parse to layout.
static bool
push_and_validate_descriptor(DescriptorSetLayout *layout, const SetBindingRecord *record, bool *binding_is_new) {
  DescriptorBinding *binding = &layout->bindings[record->binding];
  bool binding_in_use = (binding->type != DESCRIPTOR_TYPE_INVALID);
  u32 label_name_len = record->set_name_len;

  // If we find two definitions for this set/binding, assert consistency.
  if (binding_in_use) {
    *binding_is_new = false;
    const char *discovered_name = binding->discovered_shader_name;
    u32 discovered_len = binding->discovered_shader_name_len;
    u32 binding_num = record->binding;

    bool instance_names_same =
        labels_match(binding->name, record->binding_name, binding->name_length, record->binding_name_len);
    if (!instance_names_same) {
      fprintf(
          stderr, "Set %.*s, binding %u: instance name %.*s mismatches previous definition.\n", label_name_len,
          layout->name, binding_num, record->binding_name_len, record->binding_name
      );
      fprintf(
          stderr, "    Originally found %.*s in %.*s.\n", binding->name_length, binding->name, discovered_len,
          discovered_name
      );
    }

    bool types_match = (binding->type == record->descriptor_type);
    if (!types_match) {
      fprintf(
          stderr, "Set %.*s, binding %u: overrwritten descriptor type.\n", label_name_len, layout->name, binding_num
      );
      fprintf(stderr, "    Originally found in %.*s.\n", discovered_len, discovered_name);
    }

    bool counts_match = (binding->descriptor_count == record->descriptor_count);
    if (!counts_match) {
      fprintf(
          stderr, "Set %.*s, binding %u: overrwritten descriptor count.\n", label_name_len, layout->name, binding_num
      );
      fprintf(stderr, "    Originally found in %.*s.\n", discovered_len, discovered_name);
    }

    if (!instance_names_same || !types_match || !counts_match) {
      return false;
    }

    // Using the same binding from a new shader stage is allowed.
    binding->stage_flags |= record->stage;
    return true;
  } // Finish validation

  // New binding: check no existing slot already owns this instance name.
  for (u32 i = 0; i < MAX_NUM_DESCRIPTOR_BINDINGS; i++) {
    const DescriptorBinding *existing = &layout->bindings[i];
    if (existing->type == DESCRIPTOR_TYPE_INVALID || existing->name == NULL) {
      continue;
    }
    if (labels_match(existing->name, record->binding_name, existing->name_length, record->binding_name_len)) {
      fprintf(
          stderr, "Set %.*s: instance name %.*s at binding %u already used at binding %u.\n", label_name_len,
          layout->name, record->binding_name_len, record->binding_name, record->binding, i
      );
      return false;
    }
  }

  *binding_is_new = true;
  *binding = {
      .descriptor_count = record->descriptor_count,
      .type = record->descriptor_type,
      .stage_flags = record->stage,
      .name_length = record->binding_name_len,
      .name = record->binding_name,
  };

  layout->num_bindings++;
  if (record->bindless) {
    if (layout->num_bindings > 1) {
      fprintf(stderr, "Bindless binding is not the unique binding in a set. Not supported.\n");
      return false;
    }
    layout->is_bindless = true;
  }

  return true;
}

// When we have a new fragment shader, connect it to a vertex shader.
// We should not have to worry about several programs with the same name.
//  That should be checked at the beginning of parsing.
static ShaderProgram *allocate_graphics_program(ParsedShadersIR *ir, ParsedShader *frag_shader) {
  assert(frag_shader->stage == SHADER_STAGE_FRAGMENT);

  // If this fragment shader requests a particular vertex shader, search for it.
  // If there is no request, search for a matching name.
  const char *requested_vert_name = frag_shader->requested_vertex_shader_name;
  const char *search_key = (requested_vert_name != NULL) ? requested_vert_name : frag_shader->name;
  u32 key_len = (requested_vert_name != NULL) ? frag_shader->requested_vertex_shader_name_len : strlen(search_key);

  ParsedShader *vert_shader = NULL;
  for (u32 i = 0; i < ir->num_parsed_shaders; i++) {
    ParsedShader *old = &ir->parsed_shaders[i];
    if (old->stage == SHADER_STAGE_VERTEX && labels_match(search_key, old->name, key_len, strlen(old->name))) {
      vert_shader = old;
      break;
    }
  }

  // Error reporting
  if (vert_shader == NULL) {
    fprintf(stderr, "Could not find vertex shader for %s.\n", frag_shader->name);
    if (requested_vert_name != NULL) {
      fprintf(stderr, "  Requested %.*s, but not found.\n", key_len, search_key);
    } else {
      fprintf(stderr, "  No request. Tried to match name %s, but not found.\n", search_key);
    }
    return NULL;
  }

  // Validate push constant structs
  const GLSLStruct *vs_pc = vert_shader->push_constant_struct;
  const GLSLStruct *fs_pc = frag_shader->push_constant_struct;
  if (vs_pc && fs_pc && !member_list_equals(vs_pc, fs_pc)) {
    fprintf(
        stderr, "Vertex shader %s and fragment shader %s push constant structs differ.\n", vert_shader->name,
        frag_shader->name
    );
    return NULL;
  }
  u32 vs_pc_stage_flag = vs_pc ? SHADER_STAGE_VERTEX : 0;
  u32 fs_pc_stage_flag = fs_pc ? SHADER_STAGE_FRAGMENT : 0;

  ShaderProgram *prog = &ir->programs[ir->num_programs++];
  *prog = {
      .name = frag_shader->name,
      .parsed_vert = vert_shader,
      .parsed_frag = frag_shader,
      .push_constant_struct = vs_pc ? vs_pc : fs_pc,
      .push_constant_stage_flags = vs_pc_stage_flag | fs_pc_stage_flag,
  };

  return prog;
}

static ShaderProgram *allocate_compute_program(ParsedShadersIR *ir, ParsedShader *shader) {
  (void)ir;
  (void)shader;
  fprintf(stderr, "Not supporting compute programs yet.\n");
  return NULL;
}

static u32 find_label_set_number(ShaderProgram *prog, DescriptorSetLayout *layout) {
  for (u32 old_idx = 0; old_idx < prog->num_descriptor_set_layouts; old_idx++) {
    const DescriptorSetLayout *old_layout = prog->descriptor_set_layouts[old_idx];
    if (old_layout == layout) {
      return old_idx;
    }
  }

  prog->descriptor_set_layouts[prog->num_descriptor_set_layouts++] = layout;
  return prog->num_descriptor_set_layouts - 1;
}

static u32 align_up(u32 offset, u32 alignment) { return (offset + alignment - 1) & ~(alignment - 1); }

static void populate_glsl_struct_size(GLSLStruct *glsl_struct) {
  u32 max_alignment = 0;
  u32 size = 0;
  for (u32 i = 0; i < glsl_struct->num_members; i++) {
    const GLSLStructMember *member = &glsl_struct->members[i];
    u32 alignment = glsl_type_to_alignment[member->type];
    if (alignment > max_alignment)
      max_alignment = alignment;
    size = align_up(size, alignment);
    size += glsl_type_to_size[member->type];
  }

  // Align up to get full size, and then subtract data size from aligned size to emit padding.
  glsl_struct->size_in_bytes = align_up(size, max_alignment);
  glsl_struct->padding = glsl_struct->size_in_bytes - size;
}

static const GLSLStruct *push_struct(ParsedShadersIR *ir, const ShaderToCompile *input, GLSLStruct *new_struct) {
  const GLSLStruct *persistent_struct = NULL;
  const GLSLStruct *matching_struct = search_structs(new_struct, ir);

  if (matching_struct == NULL) { // Discovered new struct.
    new_struct->discovered_shader_name = input->name;
    new_struct->discovered_shader_name_len = input->name_len;
    populate_glsl_struct_size(new_struct);

    ir->structs[ir->num_structs] = *new_struct;
    persistent_struct = &ir->structs[ir->num_structs++];
  } else { // Found existing match.
    // Name is same. If mismatch, report error. If matches, update matching struct.
    bool mismatch = !member_list_equals(new_struct, matching_struct);
    if (mismatch) {
      const GLSLStruct *match = matching_struct;
      fprintf(stderr, "Conflicting definitions for %.*s.\n", new_struct->type_name_len, new_struct->type_name);
      fprintf(stderr, "New found in %.*s:\n", input->name_len, input->name);
      log_glsl_struct(stderr, new_struct);
      fprintf(stderr, "Old found in %.*s:\n", match->discovered_shader_name_len, match->discovered_shader_name);
      log_glsl_struct(stderr, matching_struct);
      return NULL;
    }

    persistent_struct = matching_struct;
  }

  return persistent_struct;
}

static bool parse_shader(const ShaderToCompile *input, ParsedShadersIR *ir) {
  Parser parser = {
      .success = true,
      .tokens = lex_string(input->source, input->source_length),
      .token_index = 0,
      .input = input,
  };

  // Validate no name conflicts
  bool new_vert = (input->stage == SHADER_STAGE_VERTEX);
  bool new_frag = (input->stage == SHADER_STAGE_FRAGMENT);
  bool new_comp = (input->stage == SHADER_STAGE_COMPUTE);
  for (u32 i = 0; i < ir->num_parsed_shaders; i++) {
    const ParsedShader *old = &ir->parsed_shaders[i];
    if (strcmp(input->name, old->name) != 0) {
      continue;
    }

    bool old_vert = (old->stage == SHADER_STAGE_VERTEX);
    bool old_frag = (old->stage == SHADER_STAGE_FRAGMENT);
    bool old_comp = (old->stage == SHADER_STAGE_COMPUTE);

    bool valid = true;
    if ((new_vert && old_vert) || (new_frag && old_frag) || (new_comp && old_comp)) {
      char *stage_str = (char *)shader_stage_to_string[old->stage];
      fprintf(stderr, "Shader %s has a duplicate %s stage.\n", input->name, stage_str);
      valid = false;
    }

    bool old_graphics_new_comp = new_comp && (old_vert || old_frag);
    bool new_graphics_old_comp = (new_vert || new_frag) && old_comp;
    if (old_graphics_new_comp || new_graphics_old_comp) {
      fprintf(stderr, "Shader %s mixes compute with vertex/fragment stages.\n", input->name);
      valid = false;
    }

    if (!valid) {
      return false;
    }
  }

  // Setup parsing
  u32 slice_idx = 0;
  ParsedShader *parsed_shader = &ir->parsed_shaders[ir->num_parsed_shaders++];
  *parsed_shader = {.name = input->name, .stage = input->stage};
  VertexLayout vertex_layout = {};
  TemplateStringSlice glsl_slice = {.start = input->source, .type = DIRECTIVE_TYPE_GLSL_SOURCE};

  while (still_valid(&parser)) {
    Token cur_tok = get_current_token(&parser);
    if (cur_tok.type != TOKEN_TYPE_DOUBLE_L_BRACE) {
      advance(&parser);
      continue;
    }

    // on a double L brace, end the glsl slice
    glsl_slice.end = cur_tok.start;
    if (glsl_slice.start != glsl_slice.end) {
      parsed_shader->slices[slice_idx++] = glsl_slice;
    }

    TemplateStringSlice slice = {.start = input->source};

    cur_tok = get_next_token(&parser);
    switch (cur_tok.type) {
    case TOKEN_TYPE_DIRECTIVE_VERSION: {
      slice.type = DIRECTIVE_TYPE_VERSION;
      parse_version_directive(&parser, &slice);
      glsl_slice.start = get_current_token(&parser).start;
      break;
    }

    case TOKEN_TYPE_DIRECTIVE_VERTEX_INDEX: {
      slice.type = DIRECTIVE_TYPE_VERTEX_INDEX;
      parse_vertex_index_directive(&parser, &slice);
      glsl_slice.start = get_current_token(&parser).start;
      break;
    }

    case TOKEN_TYPE_DIRECTIVE_INSTANCE_INDEX: {
      slice.type = DIRECTIVE_TYPE_INSTANCE_INDEX;
      parse_instance_index_directive(&parser, &slice);
      glsl_slice.start = get_current_token(&parser).start;
      break;
    }

    case TOKEN_TYPE_DIRECTIVE_LOCATION: {
      slice.type = DIRECTIVE_TYPE_LOCATION;
      LocationDirectiveParse location_parse = parse_location_directive(&parser, input->stage, &slice);
      u32 location = location_parse.vertex_attribute.location;

      if (location_parse.vertex_attribute.glsl_type != GLSL_TYPE_NULL) {
        assert(input.stage == SHADER_STAGE_VERTEX);
        if (vertex_layout.attributes[location].is_valid) {
          fprintf(stderr, "Found repeat location %u for vertex layout in %s.\n", location, input->name);
        } else {
          vertex_layout.attributes[location] = location_parse.vertex_attribute;
        }
      }

      glsl_slice.start = location_parse.next_glsl_source_start;
      break;
    } // End location

    case TOKEN_TYPE_DIRECTIVE_PUSH_CONSTANT: {
      slice.type = DIRECTIVE_TYPE_PUSH_CONSTANT;
      PushConstantDirectiveParse pc_parse = parse_push_constant_directive(&parser, &slice);
      glsl_slice.start = pc_parse.next_glsl_source_start;

      const GLSLStruct *persistent_struct = push_struct(ir, input, &pc_parse.glsl_struct);
      if (persistent_struct == NULL) {
        fprintf(stderr, "Shader %.*s has push constant, but cannot identify struct.\n", input->name_len, input->name);
        return false;
      } else {
        parsed_shader->push_constant_struct = persistent_struct;
      }

      if (persistent_struct->size_in_bytes > 128) {
        fprintf(
            stderr, "Shader %.*s has push constant of size %u, exceeding max of 128.\n", input->name_len, input->name,
            persistent_struct->size_in_bytes
        );
        return false;
      }

      break;
    } // End push constant

    case TOKEN_TYPE_DIRECTIVE_SET_BINDING: {
      slice.type = DIRECTIVE_TYPE_SET_BINDING;

      SetBindingDirectiveParse sb_parse = parse_set_binding_directive(&parser, &slice);
      sb_parse.stage = input->stage;
      glsl_slice.start = sb_parse.next_glsl_source_start;

      // May have a new struct, or may be a redefintion of one with the same type name.
      const GLSLStruct *persistent_struct = NULL;
      if (sb_parse.descriptor_type == DESCRIPTOR_TYPE_UNIFORM) {
        persistent_struct = push_struct(ir, input, &sb_parse.glsl_struct);
      }

      parsed_shader->set_binding_records[parsed_shader->num_set_binding_records++] = {
          .set_name = sb_parse.set_name,
          .set_name_len = sb_parse.set_name_len,
          .binding_name = sb_parse.instance_name,
          .binding_name_len = sb_parse.instance_name_len,
          .glsl_struct = persistent_struct,
          .binding = sb_parse.binding,
          .descriptor_type = sb_parse.descriptor_type,
          .descriptor_count = sb_parse.descriptor_count,
          .stage = sb_parse.stage,
          .slice_idx = slice_idx,
          .bindless = sb_parse.bindless,
      };

      break;
    } // End set binding.

    case TOKEN_TYPE_DIRECTIVE_VERTEX_SHADER: {
      slice.type = DIRECTIVE_TYPE_VERTEX_SHADER;
      if (input->stage != SHADER_STAGE_FRAGMENT) {
        fprintf(
            stderr, "Found VERTEX_SHADER directive in %.*s, which is not a fragment shader.\n", input->name_len,
            input->name
        );
        return false;
      }

      VertexShaderDirectiveParse vs_parse = parse_vertex_shader_directive(&parser, &slice);
      parsed_shader->requested_vertex_shader_name_len = vs_parse.program_name_len;
      parsed_shader->requested_vertex_shader_name = vs_parse.program_name;
      glsl_slice.start = vs_parse.next_glsl_source_start;
      break;
    }
    default:
      assert(false);
    };

    parsed_shader->slices[slice_idx++] = slice;
  } // finished parsing

  glsl_slice.end = input->source + input->source_length;
  if (glsl_slice.start != glsl_slice.end) {
    parsed_shader->slices[slice_idx++] = glsl_slice;
  }

  // Validate vertex layouts
  if (input->stage == SHADER_STAGE_VERTEX) {
    if (vertex_layout_validate_and_compute_offsets(&vertex_layout)) {
      // If this vertex layout is valid, check that it doesn't match an existing one
      const VertexLayout *matching_layout = NULL;
      for (u32 i = 0; i < ir->num_vertex_layouts; i++) {
        if (vertex_layout_equals(&vertex_layout, &ir->vertex_layouts[i])) {
          matching_layout = &ir->vertex_layouts[i];
          break;
        }
      }

      if (matching_layout != NULL) {
        parsed_shader->vertex_layout = matching_layout;
      } else {
        ir->vertex_layouts[ir->num_vertex_layouts++] = vertex_layout;
        parsed_shader->vertex_layout = &ir->vertex_layouts[ir->num_vertex_layouts - 1];
      }
    } else {
      fprintf(stderr, "Failed to validate vertex layout for %s.\n", input->name);
      return true;
    }
  }

  parsed_shader->num_slices = slice_idx;
  token_vector_free(&parser.tokens);
  return parser.success;
}

static bool resolve_set_bindings(ParsedShadersIR *ir, ShaderProgram *program, ParsedShader *shader) {
  for (u32 i = 0; i < shader->num_set_binding_records; i++) {
    const SetBindingRecord *record = &shader->set_binding_records[i];
    DescriptorSetLayout *matching_layout = search_descriptor_set_layouts(ir, record->set_name, record->set_name_len);

    DescriptorSetLayout *layout;

    if (matching_layout == NULL) { // New layout.
      assert(ir->num_descriptor_set_layouts < MAX_NUM_DESCRIPTOR_SET_LISTS);
      layout = &ir->descriptor_set_layouts[ir->num_descriptor_set_layouts++];

      u32 name_len = record->set_name_len;
      memcpy(layout->name, record->set_name, name_len);
      layout->name_len = name_len;
    } else {
      layout = matching_layout;
    }

    // Push the parsed binding to the current layout
    bool binding_is_new;
    bool added_binding = push_and_validate_descriptor(layout, record, &binding_is_new);
    if (!added_binding) {
      return false;
    }

    DescriptorBinding *binding = &layout->bindings[record->binding];
    if (binding_is_new) {
      binding->discovered_shader_name = shader->name;
      binding->discovered_shader_name_len = (u32)strlen(shader->name);
      binding->glsl_struct = record->glsl_struct;
    } else {
      if (binding->glsl_struct != record->glsl_struct) {
        fprintf(
            stderr, "Set %.*s, binding %u: conflicting struct types.\n", layout->name_len, layout->name, record->binding
        );
        fprintf(stderr, "New found in %s:\n", shader->name);
        log_glsl_struct(stderr, record->glsl_struct);
        fprintf(stderr, "Old found in %.*s:\n", binding->discovered_shader_name_len, binding->discovered_shader_name);
        log_glsl_struct(stderr, binding->glsl_struct);
        return false;
      }
    }

    // Determine if this layout label is new to this shader.
    bool layout_is_new = true;
    for (u32 i = 0; i < shader->num_descriptor_set_layouts; i++) {
      if (shader->descriptor_set_layouts[i] == layout) {
        layout_is_new = false;
        break;
      }
    }
    if (layout_is_new) {
      shader->descriptor_set_layouts[shader->num_descriptor_set_layouts++] = layout;
    }

    // Increment IR's descriptor_binding_types
    if (record->descriptor_type != DESCRIPTOR_TYPE_INVALID) {
      assert(record->descriptor_type < NUM_DESCRIPTOR_TYPES);
      ir->descriptor_binding_types[record->descriptor_type]++;
    }

    // Need to give string slices for set bindings their numeric sets
    shader->slices[record->slice_idx].set = find_label_set_number(program, layout);
  }
  return true;
}

static bool semantic_analysis(ParsedShadersIR *ir) {
  for (u32 i = 0; i < ir->num_parsed_shaders; i++) {
    ParsedShader *shader = &ir->parsed_shaders[i];

    if (shader->stage == SHADER_STAGE_FRAGMENT) {
      ShaderProgram *program = allocate_graphics_program(ir, shader);
      if (program == NULL) {
        return false;
      }
    } else if (shader->stage == SHADER_STAGE_COMPUTE) {
      ShaderProgram *program = allocate_compute_program(ir, shader);
      if (program == NULL) {
        return false;
      }
    }
  }

  // For every program, for its vertex and fragment shaders, accumulate sets into the global registry
  // Accumulate bindings into the program's descriptor set layouts
  for (u32 j = 0; j < ir->num_programs; j++) {
    ShaderProgram *program = &ir->programs[j];
    if (program->parsed_vert && !resolve_set_bindings(ir, program, program->parsed_vert)) {
      return false;
    }
    if (program->parsed_frag && !resolve_set_bindings(ir, program, program->parsed_frag)) {
      return false;
    }
    if (program->parsed_comp && !resolve_set_bindings(ir, program, program->parsed_comp)) {
      return false;
    }
  }

  return true;
}

ParsedShadersIR parse_shaders(const ShaderToCompileList *shaders) {
  ParsedShadersIR ir;
  memset(&ir, 0, sizeof(ir));

  ir.parsing_successful = true;
  for (u32 i = 0; i < shaders->num_shaders; i++) {
    bool successful = parse_shader(&shaders->shaders[i], &ir);
    if (!successful) {
      ir.parsing_successful = false;
    }
  }

  if (!semantic_analysis(&ir)) {
    ir.parsing_successful = false;
  }

  // Validate names are either compute OR both vertex and fragment.
  for (u32 i = 0; i < ir.num_programs; i++) {
    ShaderProgram *prog = &ir.programs[i];
    if (prog->parsed_comp) {
      continue;
    }
    if (!prog->parsed_vert || !prog->parsed_frag) {
      const char *missing_stage = !prog->parsed_vert ? "vertex" : "fragment";
      fprintf(stderr, "Shader %s is missing a %s stage.\n", prog->name, missing_stage);
      ir.parsing_successful = false;
      continue;
    }
  }

  return ir;
}
