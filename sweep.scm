;; sweep.scm -- composition experiment
;; fixed: 4 steps (the distilled default), 128x128

(display "model: ") (display (iris-model-info)) (newline)

;; A: same seed, different prompts -> does the subject move?
(iris-generate "a red apple on a wooden table" 128 128 4 42)
(iris-save "A1-seed42-apple.png")
(iris-generate "a blue ceramic cup on a wooden table" 128 128 4 42)
(iris-save "A2-seed42-cup.png")
(iris-generate "a green pear on a wooden table" 128 128 4 42)
(iris-save "A3-seed42-pear.png")
(display "A: seed fixed at 42") (newline)

;; B: same prompt, different seeds -> does the layout move?
(iris-generate "a red apple on a wooden table" 128 128 4 7)
(iris-save "B1-seed7.png")
(iris-generate "a red apple on a wooden table" 128 128 4 123)
(iris-save "B2-seed123.png")
(iris-generate "a red apple on a wooden table" 128 128 4 999)
(iris-save "B3-seed999.png")
(display "B: prompt fixed, seeds 7 / 123 / 999") (newline)

(display "OK") (newline)
