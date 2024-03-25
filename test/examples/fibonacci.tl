fibonacci = {{ n Int -> Int:
     if { n <= 0: return 0 }
     ei { n == 1: return 1 }

     beforeLast~ = 0;
     accum = 1;
     swap~ = 0;
     for { k = n - 1; k > 0; k -= 1:
          swap = beforeLast;
          beforeLast = accum;
          accum += swap
     }
     return accum
}}

main = {{
    fib = fibonacci(6);
    print fib
}}
