//! Shell Tokenizer
//!
//! This module implements a comprehensive shell tokenizer supporting
//! proper quoting, escape sequences, and all shell syntax elements.

/// Token types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TokenType {
    // Simple tokens
    Word,           // Regular word
    Assignment,     // VAR=value
    Number,         // Numeric literal
    
    // Operators
    Pipe,           // |
    And,            // &&
    Or,             // ||
    Semicolon,      // ;
    Ampersand,      // &
    
    // Redirections
    RedirectIn,     // <
    RedirectOut,    // >
    Append,         // >>
    RedirectErr,    // 2>
    RedirectErrOut, // &>
    DupIn,          // <&
    DupOut,         // >&,
    HereDoc,        // <<
    Clobber,        // >|
    
    // Grouping
    LParen,         // (
    RParen,         // )
    LBrace,         // {
    RBrace,         // }
    
    // Keywords
    If,
    Then,
    Else,
    Elif,
    Fi,
    Case,
    Esac,
    For,
    While,
    Until,
    Do,
    Done,
    In,
    Function,
    Bang,           // !
    
    // Special
    Newline,
    Eof,
    Error,
}

/// Token with position information
#[derive(Debug, Clone)]
pub struct Token {
    pub token_type: TokenType,
    pub text: String,
    pub start: usize,
    pub end: usize,
    pub is_quoted: bool,
    pub is_function_name: bool,
}

impl Default for Token {
    fn default() -> Self {
        Self {
            token_type: TokenType::Error,
            text: String::new(),
            start: 0,
            end: 0,
            is_quoted: false,
            is_function_name: false,
        }
    }
}

/// Reserved words in shell
const RESERVED_WORDS: &[(&str, TokenType)] = &[
    ("if", TokenType::If),
    ("then", TokenType::Then),
    ("else", TokenType::Else),
    ("elif", TokenType::Elif),
    ("fi", TokenType::Fi),
    ("case", TokenType::Case),
    ("esac", TokenType::Esac),
    ("for", TokenType::For),
    ("while", TokenType::While),
    ("until", TokenType::Until),
    ("do", TokenType::Do),
    ("done", TokenType::Done),
    ("in", TokenType::In),
    ("!", TokenType::Bang),
    ("function", TokenType::Function),
];

/// Tokenizer state
pub struct Tokenizer {
    input: Vec<u8>,
    pos: usize,
    len: usize,
    start: usize,
}

impl Tokenizer {
    /// Create a new tokenizer
    pub fn new(input: &str) -> Self {
        let input_bytes = input.as_bytes().to_vec();
        Self {
            input: input_bytes,
            pos: 0,
            len: input_bytes.len(),
            start: 0,
        }
    }

    /// Tokenize the input and return all tokens
    pub fn tokenize(&mut self) -> Vec<Token> {
        let mut tokens = Vec::new();

        while !self.is_eof() {
            self.skip_whitespace();
            
            if self.is_eof() {
                break;
            }

            self.start = self.pos;

            match self.current_byte() {
                b'\n' => {
                    self.pos += 1;
                    if !tokens.is_empty() {
                        tokens.push(self.make_token(TokenType::Newline));
                    }
                }
                b'#' => {
                    self.skip_comment();
                }
                b'\\' => {
                    self.pos += 1;
                    if self.is_eof() {
                        continue;
                    }
                    if self.current_byte() == b'\n' {
                        self.pos += 1;
                        continue;
                    }
                    self.pos += 1;
                }
                b'\'' => {
                    self.pos += 1;
                    self.parse_quoted_string(b'\'');
                }
                b'"' => {
                    self.pos += 1;
                    self.parse_quoted_string(b'"');
                }
                b'`' => {
                    self.parse_command_substitution();
                }
                b'|' => {
                    self.pos += 1;
                    if self.current_byte() == b'|' {
                        tokens.push(self.make_token(TokenType::Or));
                        self.pos += 1;
                    } else if self.current_byte() == b'&' {
                        tokens.push(self.make_token(TokenType::RedirectErrOut));
                        self.pos += 1;
                    } else {
                        tokens.push(self.make_token(TokenType::Pipe));
                    }
                }
                b'&' => {
                    self.pos += 1;
                    if self.current_byte() == b'&' {
                        tokens.push(self.make_token(TokenType::And));
                        self.pos += 1;
                    } else if self.current_byte() == b'>' {
                        tokens.push(self.make_token(TokenType::RedirectErrOut));
                        self.pos += 1;
                    } else if self.current_byte() == b'<' {
                        tokens.push(self.make_token(TokenType::DupIn));
                        self.pos += 1;
                    } else {
                        tokens.push(self.make_token(TokenType::Ampersand));
                    }
                }
                b';' => {
                    self.pos += 1;
                    if self.current_byte() == b';' {
                        self.pos += 1;
                    }
                    tokens.push(self.make_token(TokenType::Semicolon));
                }
                b'<' => {
                    self.pos += 1;
                    if self.current_byte() == b'<' {
                        self.pos += 1;
                        if self.current_byte() == b'<' {
                            self.pos += 1;
                        }
                        if self.current_byte() == b'-' {
                            self.pos += 1;
                        }
                        tokens.push(self.make_token(TokenType::HereDoc));
                    } else if self.current_byte() == b'&' {
                        self.pos += 1;
                        tokens.push(self.make_token(TokenType::DupIn));
                    } else {
                        tokens.push(self.make_token(TokenType::RedirectIn));
                    }
                }
                b'>' => {
                    self.pos += 1;
                    if self.current_byte() == b'>' {
                        tokens.push(self.make_token(TokenType::Append));
                        self.pos += 1;
                    } else if self.current_byte() == b'|' {
                        tokens.push(self.make_token(TokenType::Clobber));
                        self.pos += 1;
                    } else if self.current_byte() == b'&' {
                        tokens.push(self.make_token(TokenType::DupOut));
                        self.pos += 1;
                    } else {
                        tokens.push(self.make_token(TokenType::RedirectOut));
                    }
                }
                b'(' => {
                    self.pos += 1;
                    tokens.push(self.make_token(TokenType::LParen));
                }
                b')' => {
                    self.pos += 1;
                    tokens.push(self.make_token(TokenType::RParen));
                }
                b'{' => {
                    self.pos += 1;
                    tokens.push(self.make_token(TokenType::LBrace));
                }
                b'}' => {
                    self.pos += 1;
                    tokens.push(self.make_token(TokenType::RBrace));
                }
                b'$' => {
                    self.parse_dollar();
                }
                _ => {
                    self.parse_word();
                    let text = self.extract_text();
                    
                    if let Some(ttype) = self.check_reserved_word(&text) {
                        tokens.push(Token {
                            token_type: ttype,
                            text,
                            start: self.start,
                            end: self.pos,
                            is_quoted: false,
                            is_function_name: false,
                        });
                    } else {
                        if text.contains('=') {
                            if let Some(eq_pos) = text.find('=') {
                                let var_name = &text[..eq_pos];
                                if self.is_valid_var_name(var_name) {
                                    tokens.push(Token {
                                        token_type: TokenType::Assignment,
                                        text,
                                        start: self.start,
                                        end: self.pos,
                                        is_quoted: false,
                                        is_function_name: false,
                                    });
                                    continue;
                                }
                            }
                        }
                        tokens.push(self.make_token(TokenType::Word));
                    }
                }
            }
        }

        tokens.push(Token {
            token_type: TokenType::Eof,
            text: String::new(),
            start: self.pos,
            end: self.pos,
            is_quoted: false,
            is_function_name: false,
        });

        tokens
    }

    fn skip_whitespace(&mut self) {
        while !self.is_eof() && matches!(self.current_byte(), b' ' | b'\t') {
            self.pos += 1;
        }
    }

    fn skip_comment(&mut self) {
        self.pos += 1;
        while !self.is_eof() && self.current_byte() != b'\n' {
            self.pos += 1;
        }
    }

    fn parse_quoted_string(&mut self, quote_char: u8) {
        self.start = self.pos;
        while !self.is_eof() && self.current_byte() != quote_char {
            if self.current_byte() == b'\\' && self.pos + 1 < self.len {
                self.pos += 2;
            } else {
                self.pos += 1;
            }
        }
        if !self.is_eof() && self.current_byte() == quote_char {
            self.pos += 1;
        }
    }

    fn parse_command_substitution(&mut self) {
        self.start = self.pos;
        self.pos += 1;
        let mut depth = 1;
        while !self.is_eof() && depth > 0 {
            if self.current_byte() == b'`' {
                depth -= 1;
                self.pos += 1;
            } else if self.current_byte() == b'\\' && self.pos + 1 < self.len {
                self.pos += 2;
            } else {
                self.pos += 1;
            }
        }
    }

    fn parse_dollar(&mut self) {
        self.pos += 1;
        if self.is_eof() {
            return;
        }

        match self.current_byte() {
            b'\'' => {
                self.pos += 1;
                while !self.is_eof() && self.current_byte() != b'\'' {
                    if self.current_byte() == b'\\' && self.pos + 1 < self.len {
                        self.pos += 2;
                    } else {
                        self.pos += 1;
                    }
                }
                if !self.is_eof() {
                    self.pos += 1;
                }
            }
            b'"' => {
                self.pos += 1;
                while !self.is_eof() && self.current_byte() != b'"' {
                    if self.current_byte() == b'\\' && self.pos + 1 < self.len {
                        self.pos += 2;
                    } else {
                        self.pos += 1;
                    }
                }
                if !self.is_eof() {
                    self.pos += 1;
                }
            }
            b'(' => {
                self.pos += 1;
                let mut depth = 1;
                while !self.is_eof() && depth > 0 {
                    if self.current_byte() == b'(' {
                        depth += 1;
                    } else if self.current_byte() == b')' {
                        depth -= 1;
                    } else if self.current_byte() == b'\\' && self.pos + 1 < self.len {
                        self.pos += 2;
                    } else {
                        self.pos += 1;
                    }
                }
                if !self.is_eof() {
                    self.pos += 1;
                }
            }
            b'{' => {
                self.pos += 1;
                let mut depth = 1;
                while !self.is_eof() && depth > 0 {
                    if self.current_byte() == b'{' {
                        depth += 1;
                    } else if self.current_byte() == b'}' {
                        depth -= 1;
                    } else if self.current_byte() == b'\\' && self.pos + 1 < self.len {
                        self.pos += 2;
                    } else {
                        self.pos += 1;
                    }
                }
                if !self.is_eof() {
                    self.pos += 1;
                }
            }
            b'@' | b'*' | b'#' | b'?' | b'-' | b'$' | b'!' | b'0'..=b'9' => {
                self.pos += 1;
                while !self.is_eof() && (self.current_byte().is_ascii_alphanumeric() || 
                       self.current_byte() == b'_') {
                    self.pos += 1;
                }
            }
            _ => {
                if self.current_byte().is_ascii_alphabetic() || self.current_byte() == b'_' {
                    self.pos += 1;
                    while !self.is_eof() && (self.current_byte().is_ascii_alphanumeric() || 
                           self.current_byte() == b'_') {
                        self.pos += 1;
                    }
                }
            }
        }
    }

    fn parse_word(&mut self) {
        while !self.is_eof() {
            match self.current_byte() {
                b' ' | b'\t' | b'\n' | b'\r' | b';' | b'&' | b'|' | b'<' | b'>' 
                | b'(' | b')' | b'{' | b'}' | b'\'' | b'"' | b'`' => break,
                b'\\' if self.pos + 1 < self.len && self.input[self.pos + 1] == b'\n' => {
                    self.pos += 2;
                }
                _ => self.pos += 1,
            }
        }
    }

    fn extract_text(&self) -> String {
        String::from_utf8_lossy(&self.input[self.start..self.pos]).to_string()
    }

    fn make_token(&mut self, token_type: TokenType) -> Token {
        let text = self.extract_text();
        Token {
            token_type,
            text,
            start: self.start,
            end: self.pos,
            is_quoted: false,
            is_function_name: false,
        }
    }

    fn check_reserved_word(&self, text: &str) -> Option<TokenType> {
        for (word, ttype) in RESERVED_WORDS {
            if *word == text {
                return Some(*ttype);
            }
        }
        None
    }

    fn is_valid_var_name(&self, name: &str) -> bool {
        if name.is_empty() {
            return false;
        }
        let mut chars = name.chars();
        let first = chars.next().unwrap();
        if !first.is_ascii_alphabetic() && first != '_' {
            return false;
        }
        for c in chars {
            if !c.is_ascii_alphanumeric() && c != '_' {
                return false;
            }
        }
        true
    }

    fn current_byte(&self) -> u8 {
        if self.pos < self.len {
            self.input[self.pos]
        } else {
            0
        }
    }

    fn is_eof(&self) -> bool {
        self.pos >= self.len
    }
}

impl Default for Tokenizer {
    fn default() -> Self {
        Self::new("")
    }
}
