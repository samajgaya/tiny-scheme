(define pi 3.14159)

(define cube (lambda (x)
               (* x x x)))

(define sin (lambda (x)
              (if (< x 0.01)
                x
                (- (* 3 (sin (/ x 3))) (* 4 (cube (sin (/ x 3))))))))

(show (sin (/ pi 6)))
(show (sin (/ pi 3)))
