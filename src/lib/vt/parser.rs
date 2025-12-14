#![allow(dead_code)]

use alloc::vec::Vec;

#[derive(Debug, Clone, PartialEq)]
pub enum Action {
    Print(char),
    Execute(u8),
    Hook(Vec<i64>, Vec<u8>, bool),
    Put(u8),
    OscStart,
    OscPut(u8),
    OscEnd,
    CsiDispatch(Vec<i64>, Vec<u8>, bool, char, Option<char>),
    EscDispatch(Vec<u8>, bool, u8),
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum State {
    Ground,
    Escape,
    EscapeIntermediate,
    CsiEntry,
    CsiParam,
    CsiIntermediate,
    CsiIgnore,
    DcsEntry,
    DcsParam,
    DcsIntermediate,
    DcsPassthrough,
    DcsIgnore,
    OscString,
    SosPmapiString,
}

pub struct Parser {
    state: State,
    params: Vec<i64>,
    intermediates: Vec<u8>,
    ignore_flagged: bool,
    private_prefix: Option<char>,
}

impl Parser {
    pub fn new() -> Self {
        Parser {
            state: State::Ground,
            params: Vec::new(),
            intermediates: Vec::new(),
            ignore_flagged: false,
            private_prefix: None,
        }
    }

    pub fn advance<F>(&mut self, byte: u8, mut callback: F)
    where
        F: FnMut(Action),
    {
        match self.state {
            State::Ground => match byte {
                0x00..=0x17 | 0x19 | 0x1C..=0x1F => callback(Action::Execute(byte)),
                0x1B => self.state = State::Escape,
                0x20..=0x7F => callback(Action::Print(byte as char)),
                // UTF-8 continuation bytes and other high bytes treated as print for now
                0x80..=0xFF => callback(Action::Print(byte as char)),
            },
            State::Escape => match byte {
                0x00..=0x17 | 0x19 | 0x1C..=0x1F => callback(Action::Execute(byte)),
                0x1B => (), // Ignore
                0x20..=0x2F => {
                    self.intermediates.push(byte);
                    self.state = State::EscapeIntermediate;
                }
                0x30..=0x4F | 0x51..=0x57 | 0x59 | 0x5A | 0x5C | 0x60..=0x7E => {
                    callback(Action::EscDispatch(self.intermediates.clone(), self.ignore_flagged, byte));
                    self.reset();
                }
                0x50 => self.state = State::DcsEntry,
                0x58 | 0x5E | 0x5F => self.state = State::SosPmapiString,
                0x5B => self.state = State::CsiEntry,
                0x5D => self.state = State::OscString,
                _ => self.reset(),
            },
            State::EscapeIntermediate => match byte {
                0x00..=0x17 | 0x19 | 0x1C..=0x1F => callback(Action::Execute(byte)),
                0x20..=0x2F => self.intermediates.push(byte),
                0x30..=0x7E => {
                    callback(Action::EscDispatch(self.intermediates.clone(), self.ignore_flagged, byte));
                    self.reset();
                }
                _ => self.reset(),
            },
            State::CsiEntry => match byte {
                0x00..=0x17 | 0x19 | 0x1C..=0x1F => callback(Action::Execute(byte)),
                0x1B => self.state = State::Escape,
                0x20..=0x2F => {
                    self.intermediates.push(byte);
                    self.state = State::CsiIntermediate;
                }
                0x30..=0x39 | 0x3B => {
                    if byte == 0x3B {
                        self.params.push(0); // Implicit first param 0 if starts with ;
                        self.params.push(0); // Second param starts empty
                    } else {
                        self.params.push((byte - 0x30) as i64);
                    }
                    self.state = State::CsiParam;
                }
                0x3C..=0x3F => {
                    self.private_prefix = Some(byte as char);
                    self.state = State::CsiParam;
                }
                0x40..=0x7E => {
                    callback(Action::CsiDispatch(self.params.clone(), self.intermediates.clone(), self.ignore_flagged, byte as char, self.private_prefix));
                    self.reset();
                }
                _ => self.state = State::CsiIgnore,
            },
            State::CsiParam => match byte {
                0x00..=0x17 | 0x19 | 0x1C..=0x1F => callback(Action::Execute(byte)),
                0x30..=0x39 => {
                    if let Some(last) = self.params.last_mut() {
                        if *last >= 0 {
                            *last = last.saturating_mul(10).saturating_add((byte - 0x30) as i64);
                        } else {
                             // Just append digit to new param
                             self.params.push((byte - 0x30) as i64);
                        }
                    } else {
                        self.params.push((byte - 0x30) as i64);
                    }
                }
                0x3B => self.params.push(0), // New param
                0x40..=0x7E => {
                    callback(Action::CsiDispatch(self.params.clone(), self.intermediates.clone(), self.ignore_flagged, byte as char, self.private_prefix));
                    self.reset();
                }
                0x3A | 0x3C..=0x3F => self.state = State::CsiIgnore,
                _ => self.state = State::CsiIgnore,
            },
            State::CsiIntermediate => match byte {
                0x00..=0x17 | 0x19 | 0x1C..=0x1F => callback(Action::Execute(byte)),
                0x20..=0x2F => self.intermediates.push(byte),
                0x40..=0x7E => {
                    callback(Action::CsiDispatch(self.params.clone(), self.intermediates.clone(), self.ignore_flagged, byte as char, self.private_prefix));
                    self.reset();
                }
                _ => self.state = State::CsiIgnore,
            },
            State::CsiIgnore => match byte {
                0x00..=0x17 | 0x19 | 0x1C..=0x1F => callback(Action::Execute(byte)),
                0x40..=0x7E => self.reset(),
                _ => (),
            },
            State::OscString => match byte {
                0x07 => { // BEL terminates OSC
                    callback(Action::OscEnd);
                    self.reset();
                }
                0x1B => self.state = State::Escape, // ST (ESC \) terminates OSC
                _ => callback(Action::OscPut(byte)),
            },
            // Simplified handling for others
            _ => self.reset(),
        }
    }

    fn reset(&mut self) {
        self.state = State::Ground;
        self.params.clear();
        self.intermediates.clear();
        self.ignore_flagged = false;
        self.private_prefix = None;
    }
}
