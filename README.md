# tiny-scheme
Small scheme interpreter for educational purposes.
Implemented:
- double precision real numbers
- basic mark & sweep garbage collection
- built-ins:
    - arithematic : +, -, *, /, <, >, =
    - `show` to pretty-print S-expressions
    - cons, car, cdr, null?
- special-forms:
    - quote
    - and, or
    - cond, if
    - begin
    - lambda
    - define
    - set!

Tail recursion and other data-types are not implemented

## example
n'th fibonacci number, recursively
```scheme
(define fib (lambda (x)
	      (if (or (= x 0) (= x 1))
           x
           (+ (fib (- x 1)) (fib (- x 2))))))

(show (fib 9))
34
(show (fib 20))
6765
```
