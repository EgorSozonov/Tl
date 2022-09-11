#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(dead_code)]
#![allow(unused_variables)]

use std::borrow::BorrowMut;
use std::rc::Rc;

fn main() {
    use tl::compiler::Lexer::*;

    let mut foo = Rc::new([5; 1000]);
    let mut b = foo.borrow_mut();
    b[0] = 15;

    println!("Hello, world!");
    lexicallyFoo();
    //let inp = Box::new([21 as u8; 5]);
    let inp = "asdf bcjk".as_bytes();
    let lexResult = lexicallyAnalyze(inp);
}
