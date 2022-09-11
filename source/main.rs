#![allow(non_snake_case)]
#[allow(non_upper_case_globals)]



fn main() {
    use tl::compiler::Lexer::*;

    println!("Hello, world!");
    lexicallyFoo();
    //let inp = Box::new([21 as u8; 5]);
    let inp = "asdf bcjk".as_bytes();
    let lexResult = lexicallyAnalyze(inp);
}
