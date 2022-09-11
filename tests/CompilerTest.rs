#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(nonstandard_style)]


#[cfg(test)]
mod tests {
    use tl::compiler::Lexer::*;


    #[test]
    fn testAdd() {
        assert_eq!(add(1, 2), 3);
    }
}