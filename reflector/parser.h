#pragma once

#include "filesystem_utils.h"
#include "reflector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN_VECTOR_INITIAL_CAPACITY 64
#define MAX_NUM_STRING_SLICES 64
#define MAX_NUM_VERTEX_LAYOUTS 32
#define MAX_NUM_DESCRIPTOR_SET_LISTS 32
#define MAX_NUM_DESCRIPTOR_SET_LAYOUTS 16 //
#define MAX_NUM_GLSL_STRUCTS 64
#define MAX_NUM_BINDING_SLOTS 128

static const char *RED = "\033[31m";
static const char *RESET = "\033[0m";

typedef enum {
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
  TOKEN_TYPE_SET_LABEL,
  TOKEN_TYPE_DIRECTIVE_VERTEX_INDEX,
  TOKEN_TYPE_DIRECTIVE_INSTANCE_INDEX,

  TOKEN_TYPE_TEXT,
  TOKEN_TYPE_NUMBER,

  NUM_TOKEN_TYPES
} TokenType;

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
    [TOKEN_TYPE_SET_LABEL] = "SET_LABEL",

    [TOKEN_TYPE_TEXT] = "a text literal",
    [TOKEN_TYPE_NUMBER] = "a number literal",
};

typedef enum {
  DIRECTIVE_TYPE_VERSION,
  DIRECTIVE_TYPE_SET_BINDING,
  DIRECTIVE_TYPE_LOCATION,
  DIRECTIVE_TYPE_PUSH_CONSTANT,
  DIRECTIVE_TYPE_VERTEX_INDEX,
  DIRECTIVE_TYPE_INSTANCE_INDEX,
  DIRECTIVE_TYPE_GLSL_SOURCE // not really a directive, but the stuff that comes between the directives
} DirectiveType;

// template string slices start on the first { of {{ and end on the first char of the next token
// or they start on the first token after the {{ and end on the first } of the }}
typedef struct {
  const char *start, *end;
  DirectiveType type;
  // fat struct with all the possible fields that could be needed for any of my
  // supported template replacements
  u32 location;
  u32 set;
  u32 binding;
  DescriptorType descriptor_type;
} TemplateStringSlice;

inline void log_string_slices(const TemplateStringSlice *template_string_slices, u32 num_string_slices) {
  for (u32 i = 0; i < num_string_slices; i++) {
    TemplateStringSlice slice = template_string_slices[i];

    const char *start = slice.start;
    const char *end = slice.end;
    i32 length = (i32)(end - start);
    if (length < 0) {
      printf("Found a string slice with an end pointer preceding the beginning pointer\n");
    }

    if (slice.type == DIRECTIVE_TYPE_GLSL_SOURCE) {
      printf("GLSL source slice of length %d:\n%.*s\n", length, length, start);
    } else {
      printf("String slice of length %d:\n%.*s\n", length, length, start);
    }
  }
}

typedef struct {
  const char *next_glsl_source_start;
  GLSLStruct glsl_struct;

  u32 set;
  u32 binding;
  u32 descriptor_count;
  ShaderStage stage;
  DescriptorType descriptor_type;

  const char *set_label_name;
  u32 set_label_name_length;

  bool was_successful;
} SetBindingDirectiveParse;

typedef struct {
  const char *next_glsl_source_start;
  VertexAttribute vertex_attribute;
  bool found_repeat_attribute;
  u32 repeated_attribute_location;
} LocationDirectiveParse;

// Token definitions
typedef struct {
  TokenType type;
  const char *start;
  u32 text_length;
} Token;

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
typedef struct {
  u32 size;
  u32 capacity;
  Token *tokens;
} TokenVector;

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

// parser state and APIs
typedef struct {
  const char *source;
  u64 source_length;
  TokenVector tokens;
  u32 token_index;
} Parser;

typedef struct {
  ShaderStage stage;
  const char *name; // name is a malloc'd string owned by ShaderToCompile

  TemplateStringSlice template_slices[MAX_NUM_STRING_SLICES];
  u32 num_template_slices;

  const VertexLayout *vertex_layout;
  VertexAttributeRate binding_rates[MAX_NUM_VERTEX_BINDINGS];
  u16 binding_strides[MAX_NUM_VERTEX_BINDINGS];
  u8 binding_count;

  DescriptorSetLayout *descriptor_set_layouts[MAX_NUM_DESCRIPTOR_SET_LAYOUTS];
  u32 num_descriptor_set_layouts;
} ParsedShader;

// the IR contains the shaders after parsing, which all have their slices and pointers to their descriptor sets and
// vertex layouts
// it also holds the global list of descriptor sets and vertex layouts that the slices shaders point into
//
// ShaderToCompileList is generated in the beginning of the main function, which owns all source strings. It is freed
// at the end of the main function. ParsedShadersIR contains slices into it, so as long as ShaderToCompileList is
// freed after codegen, ParsedShadersIR will remain valid
typedef struct {
  bool parsing_successful;

  ParsedShader parsed_shaders[MAX_NUM_SHADERS];
  u32 num_parsed_shaders;

  DescriptorSetLayout descriptor_set_layouts[MAX_NUM_DESCRIPTOR_SET_LISTS];
  u32 num_descriptor_set_layouts;

  u32 descriptor_binding_types[NUM_DESCRIPTOR_TYPES];

  VertexLayout vertex_layouts[MAX_NUM_VERTEX_LAYOUTS];
  u32 num_vertex_layouts;

  GLSLStruct structs[MAX_NUM_GLSL_STRUCTS];
  u32 num_structs;
} ParsedShadersIR;

typedef struct {
  const GLSLStruct *matching_struct;
  bool found_mismatch;
} StructSearchResult;

ParsedShadersIR parse_shaders(const ShaderToCompileList *shader_to_compile_list);
