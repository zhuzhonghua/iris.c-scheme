;; test.scm -- minimal smoke test (mkdir -p out first)
;;

(display "model: ") (display (iris-model-info)) (newline)

(display "tokens: ") (display (iris-encode-text "a red apple")) (newline)
(display "tokens: ") (display (iris-encode-text "a red apple on a wooden table")) (newline)

(iris-generate "a red apple" 128 128 1 42)
(display "info:  ") (display (iris-image-info)) (newline)
(iris-save "test-a.png")
(display "saved: test-a.png") (newline)

(iris-generate "a blue cup" 128 128 1 7)
(display "info:  ") (display (iris-image-info)) (newline)
(iris-save "test-b.png")
(display "saved: test-b.png") (newline)

;; 4 steps -> iris.c skips the last one, so step-1..3-of-4.png are written
(iris-step-save #t)
(iris-generate "a red apple" 128 128 4 42)
(display "steps: step-1..3-of-4.png") (newline)
(iris-step-save #f)

(display "OK") (newline)
