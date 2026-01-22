//! VT100/ANSI Terminal Emulation
//!
//! This module implements ANSI escape sequence parsing and terminal state
//! management for VT100-compatible terminal emulation.

/// ANSI escape sequence parser state
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ParserState {
    Idle,
    Escape,
    Csi,
    EscapeSequence,
    String,
    Osc,
    Dcs,
}

/// Terminal attributes
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalAttributes {
    // Cursor
    pub cursor_visible: bool,
    pub cursor_blinking: bool,
    pub saved_cursor_row: usize,
    pub saved_cursor_col: usize,
    
    // Tab stops
    pub tab_stops: [bool; 256],
    
    // Character sets
    pub g0_charset: CharsetType,
    pub g1_charset: CharsetType,
    pub charset_in_use: usize,
    
    // State
    pub origin_mode: bool,
    pub auto_wrap: bool,
    pub insert_mode: bool,
    pub application_keypad: bool,
    pub ansi_mode: bool,
}

impl Default for TerminalAttributes {
    fn default() -> Self {
        let mut tab_stops = [false; 256];
        for i in (8..256).step_by(8) {
            tab_stops[i] = true;
        }
        
        Self {
            cursor_visible: true,
            cursor_blinking: true,
            saved_cursor_row: 0,
            saved_cursor_col: 0,
            tab_stops,
            g0_charset: CharsetType::Ascii,
            g1_charset: CharsetType::Ascii,
            charset_in_use: 0,
            origin_mode: false,
            auto_wrap: true,
            insert_mode: false,
            application_keypad: false,
            ansi_mode: true,
        }
    }
}

/// Character set types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CharsetType {
    Ascii,
    SpecialGraphics,
    British,
    German,
    DecTechnical,
}

/// Terminal dimensions
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalSize {
    pub rows: usize,
    pub cols: usize,
}

impl Default for TerminalSize {
    fn default() -> Self {
        Self { rows: 24, cols: 80 }
    }
}

/// ANSI escape sequence command types
#[derive(Debug, Clone)]
pub enum AnsiCommand {
    // Cursor movement
    CursorUp(usize),
    CursorDown(usize),
    CursorForward(usize),
    CursorBack(usize),
    CursorPosition(usize, usize),  // row, col (1-indexed)
    CursorHome,
    SaveCursorPosition,
    RestoreCursorPosition,
    
    // Scrolling
    ScrollUp(usize),
    ScrollDown(usize),
    SetScrollingRegion(usize, usize),  // top, bottom
    
    // Editing
    EraseInDisplay(EraseType),
    EraseInLine(EraseType),
    InsertLines(usize),
    DeleteLines(usize),
    InsertChars(usize),
    DeleteChars(usize),
    EraseChars(usize),
    
    // Tabs
    SetTabStop,
    ClearTabStop,
    ClearAllTabStops,
    
    // Character attributes
    SetGraphicsMode(Vec<GraphicsAttribute>),
    ResetGraphicsMode,
    
    // Mode changes
    SetMode(ModeType),
    ResetMode(ModeType),
    
    // Keyboard/Input
    SetKeypadMode(bool),
    SetCursorMode(bool),
    
    // Terminal title (OSC 2)
    SetTitle(String),
    SetIconName(String),
    
    // Colors (SGR extended)
    SetForegroundColor(usize),
    SetBackgroundColor(usize),
    Set256ForegroundColor(usize),
    Set256BackgroundColor(usize),
    SetTrueColorForeground(u8, u8, u8),
    SetTrueColorBackground(u8, u8, u8),
    
    // Mouse
    EnableMouseTracking(MouseTrackingMode),
    DisableMouseTracking,
    
    // Unknown
    Unknown(Vec<u8>),
}

/// Erase types for erase commands
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EraseType {
    FromCursorToEnd,
    FromBeginningToCursor,
    EntireScreen,
    FromCursorToEndOfLine,
    FromBeginningToCursorOfLine,
    EntireLine,
}

/// Graphics mode attributes
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GraphicsAttribute {
    Reset,
    Bold,
    Dim,
    Italic,
    Underline,
    Blink,
    Reverse,
    Hidden,
    Strikethrough,
    Foreground(Color),
    Background(Color),
    DoubleUnderline,
    Framed,
    Encircled,
    Overline,
    Foreground256(usize),
    Background256(usize),
    TrueColorForeground(u8, u8, u8),
    TrueColorBackground(u8, u8, u8),
}

/// Color values for graphics mode
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Color {
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    BrightBlack,
    BrightRed,
    BrightGreen,
    BrightYellow,
    BrightBlue,
    BrightMagenta,
    BrightCyan,
    BrightWhite,
    Default,
}

/// Mode types for set/reset mode
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ModeType {
    CursorKeyMode,         // DECCKM
    ColumnMode,            // DECCOLM
    ScrollMode,            // DECSCLM
    ScreenMode,            // DECNM
    OriginMode,            // DECOM
    AutoWrapMode,          // DECAWM
    InsertMode,            // DECIM
    SixelMode,             // DECKPAM
    KeypadMode,            // DECKM
    BackarrowKeyMode,      // DECBKM
}

/// Mouse tracking modes
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MouseTrackingMode {
    None,
    X10Compatible,
    NormalTracking,
    ButtonTracking,
    AnyEventTracking,
}

/// ANSI escape sequence parser
pub struct AnsiParser {
    state: ParserState,
    params: Vec<i32>,
    intermediate: Vec<u8>,
    current_byte: u8,
    osc_data: String,
}

impl AnsiParser {
    /// Create a new ANSI parser
    pub const fn new() -> Self {
        Self {
            state: ParserState::Idle,
            params: Vec::new(),
            intermediate: Vec::new(),
            current_byte: 0,
            osc_data: String::new(),
        }
    }

    /// Reset parser to initial state
    pub fn reset(&mut self) {
        self.state = ParserState::Idle;
        self.params.clear();
        self.intermediate.clear();
        self.osc_data.clear();
    }

    /// Process a byte and return any command
    pub fn process_byte(&mut self, byte: u8) -> Option<AnsiCommand> {
        self.current_byte = byte;

        match self.state {
            ParserState::Idle => self.process_idle(byte),
            ParserState::Escape => self.process_escape(byte),
            ParserState::Csi => self.process_csi(byte),
            ParserState::EscapeSequence => self.process_escape_sequence(byte),
            ParserState::String => self.process_string(byte),
            ParserState::Osc => self.process_osc(byte),
            ParserState::Dcs => self.process_dcs(byte),
        }
    }

    fn process_idle(&mut self, byte: u8) -> Option<AnsiCommand> {
        match byte {
            0x1B => {
                self.state = ParserState::Escape;
                None
            }
            0x9B => {
                // CSI in 8-bit mode
                self.state = ParserState::Csi;
                self.params.clear();
                self.intermediate.clear();
                None
            }
            0x9D => {
                // OSC in 8-bit mode
                self.state = ParserState::Osc;
                self.params.clear();
                self.osc_data.clear();
                None
            }
            0x90 => {
                // DCS in 8-bit mode
                self.state = ParserState::Dcs;
                self.params.clear();
                None
            }
            0x98 | 0x9E | 0x9F => {
                // String terminators in 8-bit mode
                self.state = ParserState::Idle;
                None
            }
            _ => None,
        }
    }

    fn process_escape(&mut self, byte: u8) -> Option<AnsiCommand> {
        match byte {
            b'[' => {
                self.state = ParserState::Csi;
                self.params.clear();
                self.intermediate.clear();
                None
            }
            b']' => {
                self.state = ParserState::Osc;
                self.params.clear();
                self.osc_data.clear();
                None
            }
            b'P' => {
                self.state = ParserState::Dcs;
                self.params.clear();
                None
            }
            b'N' | b'O' => {
                // Single character sets
                self.state = ParserState::EscapeSequence;
                self.intermediate.clear();
                self.intermediate.push(byte);
                None
            }
            b'7' => Some(AnsiCommand::SaveCursorPosition),
            b'8' => Some(AnsiCommand::RestoreCursorPosition),
            b'c' => {
                // Full reset
                self.state = ParserState::Idle;
                Some(AnsiCommand::ResetMode(ModeType::CursorKeyMode))
            }
            b'=' => {
                // Application keypad
                self.state = ParserState::Idle;
                Some(AnsiCommand::SetKeypadMode(true))
            }
            b'>' => {
                // Normal keypad
                self.state = ParserState::Idle;
                Some(AnsiCommand::SetKeypadMode(false))
            }
            0x1B => {
                // Escape again - ignore
                None
            }
            _ => {
                self.state = ParserState::Idle;
                None
            }
        }
    }

    fn process_csi(&mut self, byte: u8) -> Option<AnsiCommand> {
        // Check for intermediate characters
        if byte >= 0x20 && byte <= 0x2F {
            self.intermediate.push(byte);
            return None;
        }

        // Check for parameter characters
        if byte >= 0x30 && byte <= 0x39 {
            let mut value = 0;
            while self.params.last().map_or(false, |&v| v == -1) {
                self.params.pop();
            }
            if let Some(last) = self.params.last_mut() {
                *last = *last * 10 + (byte - b'0') as i32;
            } else {
                self.params.push((byte - b'0') as i32);
            }
            return None;
        }

        // Parameter separator
        if byte == b';' {
            self.params.push(-1);
            return None;
        }

        // Final character
        self.state = ParserState::Idle;
        Some(self.parse_csi_command(byte))
    }

    fn process_escape_sequence(&mut self, byte: u8) -> Option<AnsiCommand> {
        self.state = ParserState::Idle;
        match byte {
            0x1B => None,  // Escape
            _ => None,     // Ignore for now
        }
    }

    fn process_string(&mut self, byte: u8) -> Option<AnsiCommand> {
        match byte {
            0x1B => {
                self.state = ParserState::EscapeSequence;
                None
            }
            0x07 => {
                // String terminator (BEL)
                self.state = ParserState::Idle;
                self.parse_osc_command()
            }
            _ => {
                self.osc_data.push(byte as char);
                None
            }
        }
    }

    fn process_osc(&mut self, byte: u8) -> Option<AnsiCommand> {
        if byte == 0x1B {
            self.state = ParserState::EscapeSequence;
            return None;
        }
        if byte == 0x07 {
            self.state = ParserState::Idle;
            return self.parse_osc_command();
        }
        if byte == b';' || byte == b'=' {
            if self.params.is_empty() {
                self.params.push((byte - b'0') as i32);
            } else if let Some(last) = self.params.last_mut() {
                *last = *last * 10 + (byte - b'0') as i32;
            }
            return None;
        }
        if byte >= b'0' && byte <= b'9' {
            if let Some(last) = self.params.last_mut() {
                *last = *last * 10 + (byte - b'0') as i32;
            } else {
                self.params.push((byte - b'0') as i32);
            }
            return None;
        }
        self.osc_data.push(byte as char);
        None
    }

    fn process_dcs(&mut self, byte: u8) -> Option<AnsiCommand> {
        if byte == 0x1B {
            self.state = ParserState::EscapeSequence;
            return None;
        }
        if byte == 0x07 || byte == 0x9C {
            self.state = ParserState::Idle;
            return None;
        }
        None
    }

    fn parse_csi_command(&mut self, final_byte: u8) -> AnsiCommand {
        // Normalize parameters
        let params: Vec<usize> = self.params.iter()
            .filter(|&&v| v > 0)
            .map(|&v| v as usize)
            .collect();

        let param = |idx: usize, default: usize| -> usize {
            params.get(idx).copied().unwrap_or(default)
        };

        match final_byte {
            b'A' => AnsiCommand::CursorUp(param(0, 1)),
            b'B' | b'e' => AnsiCommand::CursorDown(param(0, 1)),
            b'C' | b'a' => AnsiCommand::CursorForward(param(0, 1)),
            b'D' => AnsiCommand::CursorBack(param(0, 1)),
            b'E' => AnsiCommand::CursorDown(param(0, 1)),  // Cursor to beginning of line N down
            b'F' => AnsiCommand::CursorUp(param(0, 1)),    // Cursor to beginning of line N up
            b'G' | b'`' => {
                if params.is_empty() {
                    AnsiCommand::CursorHome
                } else {
                    AnsiCommand::CursorPosition(1, params[0])
                }
            }
            b'H' | b'f' => {
                if params.len() >= 2 {
                    AnsiCommand::CursorPosition(params[0], params[1])
                } else {
                    AnsiCommand::CursorHome
                }
            }
            b'J' => AnsiCommand::EraseInDisplay(match param(0, 0) {
                0 => EraseType::FromCursorToEnd,
                1 => EraseType::FromBeginningToCursor,
                2 | 3 => EraseType::EntireScreen,
                _ => EraseType::FromCursorToEnd,
            }),
            b'K' => AnsiCommand::EraseInLine(match param(0, 0) {
                0 => EraseType::FromCursorToEndOfLine,
                1 => EraseType::FromBeginningToCursorOfLine,
                2 => EraseType::EntireLine,
                _ => EraseType::FromCursorToEndOfLine,
            }),
            b'L' => AnsiCommand::InsertLines(param(0, 1)),
            b'M' => AnsiCommand::DeleteLines(param(0, 1)),
            b'P' => AnsiCommand::DeleteChars(param(0, 1)),
            b'S' => AnsiCommand::ScrollUp(param(0, 1)),
            b'T' => AnsiCommand::ScrollDown(param(0, 1)),
            b'X' => AnsiCommand::EraseChars(param(0, 1)),
            b'Z' => {
                // Backtab
                AnsiCommand::CursorBack(param(0, 1))
            }
            b'[' if self.intermediate.is_empty() => AnsiCommand::SetCursorMode(true),
            b']' if self.intermediate.is_empty() => AnsiCommand::SetCursorMode(false),
            b'^' => AnsiCommand::EraseInLine(EraseType::EntireLine),  // Privacy message
            b'c' if params.is_empty() => AnsiCommand::ResetMode(ModeType::CursorKeyMode),  // Full reset
            b'd' => {
                // Cursor to line
                if params.is_empty() {
                    AnsiCommand::CursorPosition(1, 1)
                } else {
                    AnsiCommand::CursorPosition(params[0], 1)
                }
            }
            b'g' => {
                // Tabs
                if param(0, 0) == 0 {
                    AnsiCommand::ClearTabStop
                } else {
                    AnsiCommand::ClearAllTabStops
                }
            }
            b'h' => self.parse_set_mode(),
            b'l' => self.parse_reset_mode(),
            b'm' => self.parse_graphics_command(),
            b'n' => {
                // Device status report
                if param(0, 0) == 5 {
                    // Status report
                    AnsiCommand::Unknown(vec![])
                } else if param(0, 0) == 6 {
                    // Cursor position report
                    AnsiCommand::Unknown(vec![])
                } else {
                    AnsiCommand::Unknown(vec![])
                }
            }
            b'q' => {
                // LEDs
                AnsiCommand::Unknown(vec![])
            }
            b'r' => {
                // Scrolling region
                if params.len() >= 2 {
                    AnsiCommand::SetScrollingRegion(param(0, 1), param(1, 24))
                } else {
                    AnsiCommand::Unknown(vec![])
                }
            }
            _ => AnsiCommand::Unknown(vec![]),
        }
    }

    fn parse_set_mode(&mut self) -> AnsiCommand {
        let mode = if let Some(&m) = self.params.first() { m as i32 } else { 0 };
        
        // Check for private modes (DECSET)
        if self.intermediate.contains(&b'?') {
            match mode {
                1 => AnsiCommand::SetMode(ModeType::CursorKeyMode),
                2 => AnsiCommand::SetMode(ModeType::AnsiMode),  // DECANM
                3 => AnsiCommand::SetMode(ModeType::ColumnMode),  // DECCOLM
                4 => AnsiCommand::SetMode(ModeType::ScrollMode),  // DECSCLM
                5 => AnsiCommand::SetMode(ModeType::ScreenMode),  // DECNM
                6 => AnsiCommand::SetMode(ModeType::OriginMode),  // DECOM
                7 => AnsiCommand::SetMode(ModeType::AutoWrapMode),  // DECAWM
                12 => {} // Start blinking cursor
                25 => AnsiCommand::SetCursorMode(true),  // Show cursor
                1000 => AnsiCommand::EnableMouseTracking(MouseTrackingMode::X10Compatible),
                1002 => AnsiCommand::EnableMouseTracking(MouseTrackingMode::ButtonTracking),
                1003 => AnsiCommand::EnableMouseTracking(MouseTrackingMode::AnyEventTracking),
                1005 => {}  // Extended mouse reporting
                1006 => {}  // SGR mouse reporting
                1015 => {}  // URXVT mouse reporting
                1048 => AnsiCommand::SaveCursorPosition,
                1049 => {
                    // Save cursor and clear screen (alternative screen buffer)
                    AnsiCommand::Unknown(vec![])
                }
                _ => AnsiCommand::Unknown(vec![]),
            }
        } else {
            AnsiCommand::Unknown(vec![])
        }
    }

    fn parse_reset_mode(&mut self) -> AnsiCommand {
        let mode = if let Some(&m) = self.params.first() { m as i32 } else { 0 };
        
        if self.intermediate.contains(&b'?') {
            match mode {
                1 => AnsiCommand::ResetMode(ModeType::CursorKeyMode),
                2 => AnsiCommand::ResetMode(ModeType::AnsiMode),
                3 => AnsiCommand::ResetMode(ModeType::ColumnMode),
                4 => AnsiCommand::ResetMode(ModeType::ScrollMode),
                5 => AnsiCommand::ResetMode(ModeType::ScreenMode),
                6 => AnsiCommand::ResetMode(ModeType::OriginMode),
                7 => AnsiCommand::ResetMode(ModeType::AutoWrapMode),
                25 => AnsiCommand::SetCursorMode(false),  // Hide cursor
                1000 | 1002 | 1003 => AnsiCommand::DisableMouseTracking,
                1048 => AnsiCommand::RestoreCursorPosition,
                _ => AnsiCommand::Unknown(vec![]),
            }
        } else {
            AnsiCommand::Unknown(vec![])
        }
    }

    fn parse_graphics_command(&mut self) -> AnsiCommand {
        if self.params.is_empty() {
            return AnsiCommand::ResetGraphicsMode;
        }

        let mut attrs = Vec::new();
        let mut i = 0;

        while i < self.params.len() {
            let param = self.params[i];
            
            if param < 0 {
                i += 1;
                continue;
            }

            match param {
                0 => attrs.push(GraphicsAttribute::Reset),
                1 => attrs.push(GraphicsAttribute::Bold),
                2 => attrs.push(GraphicsAttribute::Dim),
                3 => attrs.push(GraphicsAttribute::Italic),
                4 => attrs.push(GraphicsAttribute::Underline),
                5 | 6 => attrs.push(GraphicsAttribute::Blink),
                7 => attrs.push(GraphicsAttribute::Reverse),
                8 => attrs.push(GraphicsAttribute::Hidden),
                9 => attrs.push(GraphicsAttribute::Strikethrough),
                10..=19 => {}  // Font selection (not implemented)
                20 => attrs.push(GraphicsAttribute::DoubleUnderline),
                21 => attrs.push(GraphicsAttribute::Reset),  // Normal intensity
                22 => attrs.push(GraphicsAttribute::Reset),  // Bold off
                23 => attrs.push(GraphicsAttribute::Reset),  // Italic off
                24 => attrs.push(GraphicsAttribute::Reset),  // Underline off
                25 => attrs.push(GraphicsAttribute::Reset),  // Blink off
                27 => attrs.push(GraphicsAttribute::Reset),  // Reverse off
                28 => attrs.push(GraphicsAttribute::Reset),  // Hidden off
                29 => attrs.push(GraphicsAttribute::Reset),  // Strikethrough off
                30..=37 => {
                    let color = Self::color_from_param(param as usize - 30);
                    attrs.push(GraphicsAttribute::Foreground(color));
                }
                38 => {
                    if i + 1 < self.params.len() {
                        if self.params[i + 1] == 5 {
                            // 256 color
                            if i + 2 < self.params.len() {
                                let color = self.params[i + 2] as usize;
                                attrs.push(GraphicsAttribute::Foreground256(color));
                                i += 2;
                            }
                        } else if self.params[i + 1] == 2 {
                            // True color
                            if i + 4 < self.params.len() {
                                let r = self.params[i + 2] as u8;
                                let g = self.params[i + 3] as u8;
                                let b = self.params[i + 4] as u8;
                                attrs.push(GraphicsAttribute::TrueColorForeground(r, g, b));
                                i += 4;
                            }
                        }
                        i += 1;
                    }
                }
                39 => attrs.push(GraphicsAttribute::Foreground(Color::Default)),
                40..=47 => {
                    let color = Self::color_from_param(param as usize - 40);
                    attrs.push(GraphicsAttribute::Background(color));
                }
                48 => {
                    if i + 1 < self.params.len() {
                        if self.params[i + 1] == 5 {
                            // 256 color
                            if i + 2 < self.params.len() {
                                let color = self.params[i + 2] as usize;
                                attrs.push(GraphicsAttribute::Background256(color));
                                i += 2;
                            }
                        } else if self.params[i + 1] == 2 {
                            // True color
                            if i + 4 < self.params.len() {
                                let r = self.params[i + 2] as u8;
                                let g = self.params[i + 3] as u8;
                                let b = self.params[i + 4] as u8;
                                attrs.push(GraphicsAttribute::TrueColorBackground(r, g, b));
                                i += 4;
                            }
                        }
                        i += 1;
                    }
                }
                49 => attrs.push(GraphicsAttribute::Background(Color::Default)),
                51 => attrs.push(GraphicsAttribute::Framed),
                52 => attrs.push(GraphicsAttribute::Encircled),
                53 => attrs.push(GraphicsAttribute::Overline),
                54 => attrs.push(GraphicsAttribute::Reset),  // Framed/encircled off
                55 => attrs.push(GraphicsAttribute::Reset),  // Overline off
                _ => {}
            }
            i += 1;
        }

        AnsiCommand::SetGraphicsMode(attrs)
    }

    fn color_from_param(param: usize) -> Color {
        match param {
            0 => Color::Black,
            1 => Color::Red,
            2 => Color::Green,
            3 => Color::Yellow,
            4 => Color::Blue,
            5 => Color::Magenta,
            6 => Color::Cyan,
            7 => Color::White,
            8 => Color::BrightBlack,
            9 => Color::BrightRed,
            10 => Color::BrightGreen,
            11 => Color::BrightYellow,
            12 => Color::BrightBlue,
            13 => Color::BrightMagenta,
            14 => Color::BrightCyan,
            15 => Color::BrightWhite,
            _ => Color::Default,
        }
    }

    fn parse_osc_command(&mut self) -> Option<AnsiCommand> {
        if self.params.is_empty() {
            return None;
        }

        match self.params[0] {
            0 | 1 | 2 => {
                // Set icon name / window title
                Some(AnsiCommand::SetTitle(self.osc_data.clone()))
            }
            10 | 11 => {
                // Set dynamic colors
                Some(AnsiCommand::Unknown(vec![]))
            }
            12 => {
                // Set cursor color
                Some(AnsiCommand::Unknown(vec![]))
            }
            17 => {
                // Highlight background color
                Some(AnsiCommand::Unknown(vec![]))
            }
            19 => {
                // Highlight foreground color
                Some(AnsiCommand::Unknown(vec![]))
            }
            22 => {
                // Store window title
                Some(AnsiCommand::Unknown(vec![]))
            }
            23 => {
                // Restore window title
                Some(AnsiCommand::Unknown(vec![]))
            }
            _ => Some(AnsiCommand::Unknown(vec![])),
        }
    }
}

impl Default for AnsiParser {
    fn default() -> Self {
        Self::new()
    }
}

/// ANSI escape sequence generator for terminal output
pub struct AnsiGenerator;

impl AnsiGenerator {
    /// Generate cursor up command
    pub fn cursor_up(n: usize) -> &'static str {
        if n == 1 {
            "\x1B[A"
        } else {
            // Would need to allocate dynamically
            "\x1B[A"
        }
    }

    /// Generate cursor down command
    pub fn cursor_down(n: usize) -> &'static str {
        if n == 1 {
            "\x1B[B"
        } else {
            "\x1B[B"
        }
    }

    /// Generate cursor forward command
    pub fn cursor_forward(n: usize) -> &'static str {
        if n == 1 {
            "\x1B[C"
        } else {
            "\x1B[C"
        }
    }

    /// Generate cursor back command
    pub fn cursor_back(n: usize) -> &'static str {
        if n == 1 {
            "\x1B[D"
        } else {
            "\x1B[D"
        }
    }

    /// Generate cursor position command
    pub fn cursor_position(row: usize, col: usize) -> String {
        format!("\x1B[{};{}H", row, col)
    }

    /// Generate cursor home command
    pub fn cursor_home() -> &'static str {
        "\x1B[H"
    }

    /// Generate save cursor position command
    pub fn save_cursor() -> &'static str {
        "\x1B7"
    }

    /// Generate restore cursor position command
    pub fn restore_cursor() -> &'static str {
        "\x1B8"
    }

    /// Generate clear screen command
    pub fn clear_screen(erase_type: EraseType) -> &'static str {
        match erase_type {
            EraseType::EntireScreen => "\x1B[2J",
            EraseType::FromCursorToEnd => "\x1B[0J",
            EraseType::FromBeginningToCursor => "\x1B[1J",
        }
    }

    /// Generate clear line command
    pub fn clear_line(erase_type: EraseType) -> &'static str {
        match erase_type {
            EraseType::EntireLine => "\x1B[2K",
            EraseType::FromCursorToEndOfLine => "\x1B[0K",
            EraseType::FromBeginningToCursorOfLine => "\x1B[1K",
            _ => "\x1B[0K",
        }
    }

    /// Generate scroll up command
    pub fn scroll_up(n: usize) -> String {
        format!("\x1B[{}S", n)
    }

    /// Generate scroll down command
    pub fn scroll_down(n: usize) -> String {
        format!("\x1B[{}T", n)
    }

    /// Generate set scrolling region command
    pub fn set_scrolling_region(top: usize, bottom: usize) -> String {
        format!("\x1B[{};{}r", top, bottom)
    }

    /// Generate show cursor command
    pub fn show_cursor() -> &'static str {
        "\x1B[?25h"
    }

    /// Generate hide cursor command
    pub fn hide_cursor() -> &'static str {
        "\x1B[?25l"
    }

    /// Generate set title command
    pub fn set_title(title: &str) -> String {
        format!("\x1B]2;{}\x07", title)
    }

    /// Generate reset graphics mode command
    pub fn reset_graphics() -> &'static str {
        "\x1B[0m"
    }

    /// Generate bold command
    pub fn bold() -> &'static str {
        "\x1B[1m"
    }

    /// Generate underline command
    pub fn underline() -> &'static str {
        "\x1B[4m"
    }

    /// Generate blink command
    pub fn blink() -> &'static str {
        "\x1B[5m"
    }

    /// Generate reverse command
    pub fn reverse() -> &'static str {
        "\x1B[7m"
    }

    /// Generate color command (foreground)
    pub fn foreground(color: Color) -> &'static str {
        match color {
            Color::Black => "\x1B[30m",
            Color::Red => "\x1B[31m",
            Color::Green => "\x1B[32m",
            Color::Yellow => "\x1B[33m",
            Color::Blue => "\x1B[34m",
            Color::Magenta => "\x1B[35m",
            Color::Cyan => "\x1B[36m",
            Color::White => "\x1B[37m",
            Color::BrightBlack => "\x1B[90m",
            Color::BrightRed => "\x1B[91m",
            Color::BrightGreen => "\x1B[92m",
            Color::BrightYellow => "\x1B[93m",
            Color::BrightBlue => "\x1B[94m",
            Color::BrightMagenta => "\x1B[95m",
            Color::BrightCyan => "\x1B[96m",
            Color::BrightWhite => "\x1B[97m",
            Color::Default => "\x1B[39m",
        }
    }

    /// Generate color command (background)
    pub fn background(color: Color) -> &'static str {
        match color {
            Color::Black => "\x1B[40m",
            Color::Red => "\x1B[41m",
            Color::Green => "\x1B[42m",
            Color::Yellow => "\x1B[43m",
            Color::Blue => "\x1B[44m",
            Color::Magenta => "\x1B[45m",
            Color::Cyan => "\x1B[46m",
            Color::White => "\x1B[47m",
            Color::BrightBlack => "\x1B[100m",
            Color::BrightRed => "\x1B[101m",
            Color::BrightGreen => "\x1B[102m",
            Color::BrightYellow => "\x1B[103m",
            Color::BrightBlue => "\x1B[104m",
            Color::BrightMagenta => "\x1B[105m",
            Color::BrightCyan => "\x1B[106m",
            Color::BrightWhite => "\x1B[107m",
            Color::Default => "\x1B[49m",
        }
    }

    /// Generate 256 color command (foreground)
    pub fn foreground_256(color: usize) -> String {
        format!("\x1B[38;5;{}m", color)
    }

    /// Generate 256 color command (background)
    pub fn background_256(color: usize) -> String {
        format!("\x1B[48;5;{}m", color)
    }

    /// Generate true color command (foreground)
    pub fn foreground_truecolor(r: u8, g: u8, b: u8) -> String {
        format!("\x1B[38;2;{};{};{}m", r, g, b)
    }

    /// Generate true color command (background)
    pub fn background_truecolor(r: u8, g: u8, b: u8) -> String {
        format!("\x1B[48;2;{};{};{}m", r, g, b)
    }

    /// Generate alternate screen buffer command
    pub fn alternate_screen(enable: bool) -> &'static str {
        if enable {
            "\x1B[?1049h"
        } else {
            "\x1B[?1049l"
        }
    }

    /// Generate bracketed paste command
    pub fn bracketed_paste(enable: bool) -> &'static str {
        if enable {
            "\x1B[?2004h"
        } else {
            "\x1B[?2004l"
        }
    }
}
