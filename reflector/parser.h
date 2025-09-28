#pragma once

#include "filesystem_utils.h"
#include "reflector.h"

#include <malloc/_malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN_VECTOR_INITIAL_CAPACITY 64
#define MAX_NUM_STRING_SLICES 16

static const char *RED = "\033[31m";
static const char *RESET = "\033[0m";

enum TokenType {
  TOKEN_TYPE_POUND,
  TOKEN_TYPE_DOUBLE_L_BRACE,
  TOKEN_TYPE_DOUBLE_R_BRACE,
  TOKEN_TYPE_L_BRACE,
  TOKEN_TYPE_R_BRACE,
  TOKEN_TYPE_L_PAREN,
  TOKEN_TYPE_R_PAREN,
  TOKEN_TYPE_L_BRACKET,
  TOKEN_TYPE_R_BRACKET,
  TOKEN_TYPE_SEMICOLON,
  TOKEN_TYPE_COMMA,
  TOKEN_TYPE_PERIOD,

  TOKEN_TYPE_EQUALS,
  TOKEN_TYPE_EQUALS_EQUALS,
  TOKEN_TYPE_PLUS,
  TOKEN_TYPE_PLUS_PLUS,
  TOKEN_TYPE_MINUS,
  TOKEN_TYPE_MINUS_MINUS,
  TOKEN_TYPE_ASTERISK,
  TOKEN_TYPE_SLASH,

  TOKEN_TYPE_IN,
  TOKEN_TYPE_OUT,
  TOKEN_TYPE_VERSION,
  TOKEN_TYPE_VOID,
  TOKEN_TYPE_UNIFORM,
  TOKEN_TYPE_SAMPLER,
  TOKEN_TYPE_SAMPLER2D,
  TOKEN_TYPE_TEXTURE2D,
  TOKEN_TYPE_IMAGE2D,

  TOKEN_TYPE_FLOAT,
  TOKEN_TYPE_UINT,
  TOKEN_TYPE_VEC2,
  TOKEN_TYPE_VEC3,
  TOKEN_TYPE_VEC4,
  TOKEN_TYPE_MAT2,
  TOKEN_TYPE_MAT3,
  TOKEN_TYPE_MAT4,

  TOKEN_TYPE_DIRECTIVE_VERSION,
  TOKEN_TYPE_DIRECTIVE_LOCATION,
  TOKEN_TYPE_DIRECTIVE_SET_BINDING,
  TOKEN_TYPE_DIRECTIVE_PUSH_CONSTANT,
  TOKEN_TYPE_RATE_VERTEX,
  TOKEN_TYPE_RATE_INSTANCE,
  TOKEN_TYPE_BINDING,
  TOKEN_TYPE_OFFSET,
  TOKEN_TYPE_TIGHTLY_PACKED,
  TOKEN_TYPE_BUFFER_LABEL,

  TOKEN_TYPE_TEXT,
  TOKEN_TYPE_NUMBER,

  NUM_TOKEN_TYPES
};

static const char *token_type_to_string[NUM_TOKEN_TYPES] = {
    [TOKEN_TYPE_POUND] = "#",
    [TOKEN_TYPE_DOUBLE_L_BRACE] = "{{",
    [TOKEN_TYPE_DOUBLE_R_BRACE] = "}}",
    [TOKEN_TYPE_L_BRACE] = "{",
    [TOKEN_TYPE_R_BRACE] = "}",
    [TOKEN_TYPE_L_PAREN] = "(",
    [TOKEN_TYPE_R_PAREN] = ")",
    [TOKEN_TYPE_L_BRACKET] = "[",
    [TOKEN_TYPE_R_BRACKET] = "]",
    [TOKEN_TYPE_SEMICOLON] = ";",
    [TOKEN_TYPE_COMMA] = ",",
    [TOKEN_TYPE_PERIOD] = ".",

    [TOKEN_TYPE_EQUALS] = "=",
    [TOKEN_TYPE_EQUALS_EQUALS] = "==",
    [TOKEN_TYPE_PLUS] = "+",
    [TOKEN_TYPE_PLUS_PLUS] = "++",
    [TOKEN_TYPE_MINUS] = "-",
    [TOKEN_TYPE_MINUS_MINUS] = "--",
    [TOKEN_TYPE_ASTERISK] = "*",
    [TOKEN_TYPE_SLASH] = "/",

    [TOKEN_TYPE_IN] = "in",
    [TOKEN_TYPE_OUT] = "out",
    [TOKEN_TYPE_VERSION] = "version",
    [TOKEN_TYPE_VOID] = "void",
    [TOKEN_TYPE_UNIFORM] = "uniform",
    [TOKEN_TYPE_SAMPLER] = "sampler",
    [TOKEN_TYPE_SAMPLER2D] = "sampler2D",
    [TOKEN_TYPE_TEXTURE2D] = "texture2D",
    [TOKEN_TYPE_IMAGE2D] = "image2D",

    [TOKEN_TYPE_FLOAT] = "float",
    [TOKEN_TYPE_UINT] = "uint",
    [TOKEN_TYPE_VEC2] = "vec2",
    [TOKEN_TYPE_VEC3] = "vec3",
    [TOKEN_TYPE_VEC4] = "vec4",
    [TOKEN_TYPE_MAT2] = "mat2",
    [TOKEN_TYPE_MAT3] = "mat3",
    [TOKEN_TYPE_MAT4] = "mat4",
    [TOKEN_TYPE_DIRECTIVE_VERSION] = "VERSION",
    [TOKEN_TYPE_DIRECTIVE_LOCATION] = "LOCATION",
    [TOKEN_TYPE_DIRECTIVE_SET_BINDING] = "SET_BINDING",
    [TOKEN_TYPE_DIRECTIVE_PUSH_CONSTANT] = "PUSH_CONSTANT",
    [TOKEN_TYPE_RATE_VERTEX] = "RATE_VERTEX",
    [TOKEN_TYPE_RATE_INSTANCE] = "RATE_INSTANCE",
    [TOKEN_TYPE_BINDING] = "BINDING",
    [TOKEN_TYPE_OFFSET] = "OFFSET",
    [TOKEN_TYPE_TIGHTLY_PACKED] = "TIGHTLY_PACKED",
    [TOKEN_TYPE_BUFFER_LABEL] = "BUFFER_LABEL",

    [TOKEN_TYPE_TEXT] = "a text literal",
    [TOKEN_TYPE_NUMBER] = "a number literal",
};

enum DirectiveType {
  DIRECTIVE_TYPE_VERSION,
  DIRECTIVE_TYPE_SET_BINDING,
  DIRECTIVE_TYPE_LOCATION,
  DIRECTIVE_TYPE_PUSH_CONSTANT,
  DIRECTIVE_TYPE_GLSL_SOURCE // not really a directive, but the stuff that comes between the directives
};

struct TemplateStringReplacement {
  const char *string;
  u32 length;
};

enum GraphicsBackend { GRAPHICS_BACKEND_VULKAN, GRAPHICS_BACKEND_OPENGL, NUM_GRAPHICS_BACKENDS };

// template string slices start on the first { of {{ and end on the first char of the next token
// or they start on the first token after the {{ and end on the first } of the }}
struct TemplateStringSlice {
  const char *start, *end;
  DirectiveType type;
  // fat struct with all the possible fields that could be needed for any of my
  // supported template replacements
  // slightly wasteful in space, but easy to use
  u32 location;
  u32 set;
  u32 binding;

  TemplateStringReplacement replacements[NUM_GRAPHICS_BACKENDS];
};

inline TemplateStringSlice new_template_string_slice(const char *start) {
  TemplateStringSlice template_string_slice;
  template_string_slice.start = start;
  template_string_slice.end = NULL;
  template_string_slice.location = 0;
  template_string_slice.set = 0;
  template_string_slice.binding = 0;

  memset(template_string_slice.replacements, 0, sizeof(template_string_slice.replacements));
  return template_string_slice;
}

struct SetBindingDirectiveParse {
  const char *next_glsl_source_start;
  bool was_successful;
};

// Token definitions
struct Token {
  TokenType type;
  const char *start;
  u32 text_length;
};

inline Token new_token(TokenType type, const char *start) {
  Token token;
  token.type = type;
  token.start = start;
  token.text_length = 0;
  return token;
};

inline Token new_text_token(TokenType type, const char *text, u32 text_length) {
  Token token;
  token.type = type;
  token.start = text;
  token.text_length = text_length;
  return token;
};

// TokenVector implementation/declarations
struct TokenVector {
  u32 size;
  u32 capacity;
  Token *tokens;
};

inline TokenVector new_token_vector() {
  TokenVector token_vector;
  token_vector.size = 0;
  token_vector.capacity = TOKEN_VECTOR_INITIAL_CAPACITY;
  token_vector.tokens = (Token *)malloc(TOKEN_VECTOR_INITIAL_CAPACITY * sizeof(Token));

  if (token_vector.tokens == NULL) {
    fprintf(stderr, "new_token_vector: failed to malloc\n");
    exit(1);
  }

  return token_vector;
}

inline void push_token(TokenVector *token_vector, Token token) {
  if (token_vector->size >= token_vector->capacity) {
    token_vector->capacity *= 2;
    Token *new_tokens = (Token *)realloc(token_vector->tokens, sizeof(Token) * token_vector->capacity);

    if (new_tokens == NULL) {
      fprintf(stderr, "push_token: failed to realloc\n");
      exit(1);
    }
    token_vector->tokens = new_tokens;
  }

  token_vector->tokens[token_vector->size++] = token;
}

inline void token_vector_free(TokenVector *token_vector) {
  token_vector->capacity = 0;
  token_vector->size = 0;
  free(token_vector->tokens);
  token_vector->tokens = NULL;
}

// parser state and inline helpers
struct Parser {
  const char *source;
  u32 source_length;
  TokenVector tokens;
  u32 token_index;
};

inline Token parser_get_current_token(const Parser *parser) { return parser->tokens.tokens[parser->token_index]; }

inline void parser_advance(Parser *parser) { parser->token_index++; }

inline Token parser_advance_and_get_next_token(Parser *parser) {
  parser_advance(parser);
  return parser_get_current_token(parser);
}

inline bool parser_still_valid(Parser *parser) { return parser->token_index < parser->tokens.size; }

TokenVector lex_string(const char *string, u32 string_length);
void parse_shader(ShaderToCompile shader_to_compile, TemplateStringSlice *out_string_slices,
                  u32 *out_num_string_slices);
