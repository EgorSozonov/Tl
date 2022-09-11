#[cfg(test)]
mod tests {
    // Note this useful idiom: importing names from outer (for mod tests) scope.
    use super::*;


    #[test]
    fn testAdd() {
        assert_eq!(add(1, 2), 3);
    }
}