# Ors
### A new embedded scripting language

Goal: a language for the browser that doesn't suck


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
* Compiler and all tooling written in a native language, i.e. fast

### Why this name?

"Billt" was built by a crazy scientist named Bill Tinkerer, also known as Bill Tin, so you can have
additional functionality "built in" your favorite software

### Using it

Is a little rough for now...

- Build it with `. scripts/build.txt`

- Put the file test/examples/ui.html into the ./_bin folder

- put the source code into ./_bin/code.tl file

- run ./_bin/tlc

- open the UI and run

### Source code

Source code is just 3 files (tl.c, tl.h, tl.internal.h). Navigate them using code folds. For example, in Neovim,
use "za" to toggle a fold, and "zm" to close them en masse.


### Future releases roadmap

#### version 0.3.0
* more developed static type system (structs, arrays)
* generic types and overloading for them
* new and final syntax 
* interpreter

#### version 0.4.0
* exceptions
* lambdas
* "each" loops
* better error reporting
    
#### version 0.5.0
* nullability tracking (a.k.a. flow typing)
* sum types
* pattern matching
* "ifPr" syntax form
    
#### version 0.6.0
* async/await
* generators
* aliases
* defer, destructors
    
#### version 0.7.0
* source maps for the browser 
* loose ends
* maybe something else
    
