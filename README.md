# Tl
### A new progamming language for the browser

Goal: a practical JVM development language that doesn't suck

### Current version: 0.1.0 (2022-12-22)

* basic arithmetic, variables, nested function definitions
* not much else, but the language finally exists


### Language features
* Practical: no Big Idea, just a language that works
* Full static nominal typing, with a single Dynamic type
* Simple, well thought-out syntax (not C-like)
* Null safety
* Sum types, generics, modular implicits
* Language-level support for dependency injection, metadata and unit testing
* No inheritance, just interfaces and composition
* Immutability by default, but no "purity" or side effects control
* No "import" duplication in every source file
* Hot code reloading
* Stability after version 1.0: no breaking changes, few new features if any
* Compiles to JokeScript, but with no dependencies on any NPM packages
* Compiler and all tooling written in a native language, i.e. faster than JS

### Why this name?

"Tl" kind of looks like the letter π and may stand for "Top language" or "The language". 
Short and sweet. 

### Future releases roadmap

#### version 0.2.0
* type checker, type-based overloading
* if-expression, including inline if
* JVM bytecode generation
* start working on documentation

#### version 0.3.0
* looping
* exception handling
* structs, sum types
* more documentation