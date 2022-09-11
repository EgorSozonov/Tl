#![allow(non_snake_case)]
mod compiler {
    pub mod Lexer;
    pub mod LexerTypes;
}


fn main() {
    use compiler::Lexer::*;

    println!("Hello, world!");
    lexicallyFoo();
}
