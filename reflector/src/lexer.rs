#[derive(PartialEq)]
pub enum TokenType {
    Pound,
    DoubleLeftBrace,
    DoubleRightBrace,
    LeftBrace,
    RightBrace,
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    Semicolon,
    Comma,
    Period,

    Equals,
    EqualsEquals,
    Plus,
    PlusPlus,
    Minus,
    MinusMinus,
    Asterisk,
    Slash,

    In,
    Out,
    Version,
    Void,
    Uniform,
    Sampler,
    Sampler2d,
    Texture2d,
    Image2d,

    Float,
    Uint,
    Vec2,
    Vec3,
    Vec4,
    Mat2,
    Mat3,
    Mat4,

    DirectiveVersion,
    DirectiveLocation,
    DirectiveSetBinding,
    DirectivePushConstant,
    RateVertex,
    RateInstance,
    Binding,
    Offset,
    TightlyPacked,
    BufferLabel,

    Text,
}

pub struct Token<'a> {
    pub token_type: TokenType,
    pub string_view: Option<&'a str>,
}

impl<'a> Token<'a> {
    fn new(token_type: TokenType) -> Self {
        Self {
            token_type,
            string_view: None,
        }
    }

    fn new_text_token(token_type: TokenType, string_view: &'a str) -> Self {
        Self {
            token_type,
            string_view: Some(string_view),
        }
    }
}

fn string_slice_to_token_type(slice: &str) -> TokenType {
    match slice {
        "in" => TokenType::In,
        "out" => TokenType::Out,
        "version" => TokenType::Version,
        "void" => TokenType::Void,
        "uniform" => TokenType::Uniform,
        "sampler" => TokenType::Sampler,
        "sampler2D" => TokenType::Sampler2d,
        "texture2D" => TokenType::Texture2d,
        "image2D" => TokenType::Image2d,

        "float" => TokenType::Float,
        "uint" => TokenType::Uint,
        "vec2" => TokenType::Vec2,
        "vec3" => TokenType::Vec3,
        "vec4" => TokenType::Vec4,
        "mat2" => TokenType::Mat2,
        "mat3" => TokenType::Mat3,
        "mat4" => TokenType::Mat4,

        "VERSION" => TokenType::DirectiveVersion,
        "LOCATION" => TokenType::DirectiveLocation,
        "SET_BINDING" => TokenType::DirectiveSetBinding,
        "PUSH_CONSTANT" => TokenType::DirectivePushConstant,
        "RATE_VERTEX" => TokenType::RateVertex,
        "RATE_INSTANCE" => TokenType::RateInstance,
        "BINDING" => TokenType::Binding,
        "OFFSET" => TokenType::Offset,
        "TIGHTLY_PACKED" => TokenType::TightlyPacked,
        "BUFFER_LABEL" => TokenType::BufferLabel,
        _ => TokenType::Text,
    }
}

fn lex_string(string: &String) -> Vec<Token> {
    let mut tokens: Vec<Token> = Vec::new();
    let mut chars = string.as_str().chars().peekable();
    // got railed by trying to be rust iterator functional style >:(
    let mut current_byte_offset = 0;

    while let Some(&c) = chars.peek() {
        if c.is_whitespace() {
            continue;
        }

        match c {
            '/' => {
                chars.next();
                current_byte_offset += c.len_utf8();
                // skip comment
                if chars.peek() == Some(&'/') {
                    chars.next();
                    current_byte_offset += c.len_utf8();
                    while let Some(&comment_char) = chars.peek() {
                        if comment_char == '\n' {
                            break;
                        }
                        chars.next();
                        current_byte_offset += c.len_utf8();
                    }
                } else {
                    tokens.push(Token::new(TokenType::Slash));
                }
            }
            '#' => {
                tokens.push(Token::new(TokenType::Pound));
                chars.next();
                current_byte_offset += c.len_utf8();
            }
            '{' => {
                chars.next();
                current_byte_offset += c.len_utf8();
                if chars.peek() == Some(&'{') {
                    tokens.push(Token::new(TokenType::DoubleLeftBrace));
                    chars.next();
                    current_byte_offset += c.len_utf8();
                } else {
                    tokens.push(Token::new(TokenType::LeftBrace));
                }
            }
            '}' => {
                chars.next();
                current_byte_offset += c.len_utf8();
                if chars.peek() == Some(&'}') {
                    tokens.push(Token::new(TokenType::DoubleRightBrace));
                    chars.next();
                    current_byte_offset += c.len_utf8();
                } else {
                    tokens.push(Token::new(TokenType::RightBrace));
                }
            }
            '+' => {
                chars.next();
                current_byte_offset += c.len_utf8();
                if chars.peek() == Some(&'+') {
                    tokens.push(Token::new(TokenType::PlusPlus));
                    chars.next();
                    current_byte_offset += c.len_utf8();
                } else {
                    tokens.push(Token::new(TokenType::Plus));
                }
            }
            '-' => {
                chars.next();
                current_byte_offset += c.len_utf8();
                if chars.peek() == Some(&'-') {
                    tokens.push(Token::new(TokenType::MinusMinus));
                    chars.next();
                    current_byte_offset += c.len_utf8();
                } else {
                    tokens.push(Token::new(TokenType::Minus));
                }
            }
            '=' => {
                chars.next();
                current_byte_offset += c.len_utf8();
                if chars.peek() == Some(&'=') {
                    tokens.push(Token::new(TokenType::EqualsEquals));
                    chars.next();
                    current_byte_offset += c.len_utf8();
                } else {
                    tokens.push(Token::new(TokenType::Equals));
                }
            }
            '(' => {
                tokens.push(Token::new(TokenType::LeftParen));
                chars.next();
                current_byte_offset += c.len_utf8();
            }
            ')' => {
                tokens.push(Token::new(TokenType::RightParen));
                chars.next();
                current_byte_offset += c.len_utf8();
            }
            '[' => {
                tokens.push(Token::new(TokenType::LeftBracket));
                chars.next();
                current_byte_offset += c.len_utf8();
            }
            ']' => {
                tokens.push(Token::new(TokenType::RightBracket));
                chars.next();
                current_byte_offset += c.len_utf8();
            }
            ';' => {
                tokens.push(Token::new(TokenType::Semicolon));
                chars.next();
                current_byte_offset += c.len_utf8();
            }
            ',' => {
                tokens.push(Token::new(TokenType::Comma));
                chars.next();
                current_byte_offset += c.len_utf8();
            }
            '.' => {
                tokens.push(Token::new(TokenType::Period));
                chars.next();
                current_byte_offset += c.len_utf8();
            }
            _ => {
                let start_byte_offset = current_byte_offset;

                // lex a number
                if c.is_numeric() {}

                while let Some(&identifier_ch) = chars.peek() {
                    if identifier_ch.is_alphanumeric() || identifier_ch == '_' {
                        chars.next();
                        current_byte_offset += identifier_ch.len_utf8();
                    } else {
                        break;
                    }
                }

                let past_the_end_byte_offset = current_byte_offset;
                let string_view = &string[start_byte_offset..past_the_end_byte_offset];
                let token_type = string_slice_to_token_type(string_view);
                if token_type == TokenType::Text {
                    tokens.push(Token::new(token_type));
                } else {
                    tokens.push(Token::new_text_token(token_type, string_view));
                }
            }
        }
    }

    tokens
}
