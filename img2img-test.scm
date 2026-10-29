;; img2img-test.scm -- self-contained img2img check
;;
;; 1. txt2img: red apple on a wooden table (seed 42) -> img2img-src.png
;; 2. img2img : same picture, prompt changed to a blue cup -> img2img-out.png
;;
;; Compare the two files: did the cup keep the apple's position,
;; the table, the lighting and the shadow?

(display "model: ") (display (iris-model-info)) (newline)

(iris-generate "a red apple on a wooden table" 128 128 4 42)
(iris-save "img2img-src.png")
(display "src:  img2img-src.png") (newline)

(iris-img2img "img2img-src.png" "a blue ceramic cup on a wooden table" 128 128 4 42)
(iris-save "img2img-out.png")
(display "out:  img2img-out.png") (newline)

(display "OK") (newline)
