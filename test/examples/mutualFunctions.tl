(:f foo Int(s String flag Bool) =
    (:if flag => return #s
            else return getLength s))

(:f getLength Int(s String) =
    return foo s true)

(:f main() =
    result = foo `Hello world` false
    print : + `Result is ` $result)
