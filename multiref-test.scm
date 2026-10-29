;; multiref-test.scm -- two references, one output
;;
;; 1. reference A: red apple on a wooden table
;; 2. reference B: blue ceramic cup on a marble floor
;; 3. multiref: ask for both, in one picture
;;
;; Compare multiref-out.png with ref-a.png and ref-b.png: which layout
;; survives, and how are the two subjects placed?

(iris-generate "a red apple on a wooden table" 128 128 4 42)
(iris-save "ref-a.png")
(display "ref-a.png") (newline)

(iris-generate "a blue ceramic cup on a marble floor" 128 128 4 7)
(iris-save "ref-b.png")
(display "ref-b.png") (newline)

(iris-multiref "a red apple and a blue cup on the same table"
               "ref-a.png" "ref-b.png" 128 128 4 42)
(iris-save "multiref-out.png")
(display "multiref-out.png") (newline)

;; Same references, same seed: only the wording of the layout changes
(iris-multiref "a red apple on the left and a blue ceramic cup on the right, on a wooden table"
               "ref-a.png" "ref-b.png" 128 128 4 42)
(iris-save "multiref-layout.png")
(display "multiref-layout.png") (newline)

(display "OK") (newline)
