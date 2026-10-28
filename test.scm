;; test.scm -- minimal smoke test (mkdir -p out first)
;;

(display "model: ") (display (iris-model-info)) (newline)

(display "tokens: ") (display (iris-encode-text "a red apple")) (newline)
(display "tokens: ") (display (iris-encode-text "a red apple on a wooden table")) (newline)

(iris-generate "a red apple" 128 128 1 42)
(display "info:  ") (display (iris-image-info)) (newline)
(iris-save "out/test-a.png")
(display "saved: out/test-a.png") (newline)

(iris-generate "a blue cup" 128 128 1 7)
(display "info:  ") (display (iris-image-info)) (newline)
(iris-save "out/test-b.png")
(display "saved: out/test-b.png") (newline)

(display "OK") (newline)
