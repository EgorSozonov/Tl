# Tl
### A new programming language for the browser

Goal: a practical web development language that doesn't suck


### Language features
* Practical: no Big Idea, just a language that works
* Full static nominal typing, with a single Dynamic type
* Simple, well thought-out syntax (that is, Lisp-like, not C-like)
* Null safety
* Sum types, generics, function overloading
* Language-level support for dependency injection, metadata and unit testing
* No inheritance, just interfaces and composition
* Immutability by default, but no "purity" or side effects control
* No "import" duplication in every source file
* Hot code reloading
* Stability after version 1.0: no breaking changes, few new features if any
* Compiles to JokeScript, but with no dependencies on any JS packages
* Compiler and all tooling written in a native language, i.e. fast

### Why this name?

"Tl" kind of looks like the letter π. Alternatively, it may stand for "Top language" or "The language". 
Short and sweet.

### Using it

Is a little rough for now...

- Build it with `. scripts/build.txt`

- Put the file test/examples/ui.html into the ./_bin folder

- put the source code into ./_bin/code.tl file

- run ./_bin/tlc

- open the UI and run


### Future releases roadmap

#### version 0.3.0
* developed static type system (structs, sum types, nullable types)
* generic types and overloading for them
* pattern matching
* lambdas

