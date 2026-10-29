;; cmp-test.scm -- is img2img just txt2img with a different prompt?
;;
;; Same prompt, same seed, same size. The only difference is whether a
;; reference image is supplied as in-context conditioning.
;; Compare cmp-txt2img.png against cmp-img2img.png:
;;   differ -> the reference tokens do influence the result
;;   same   -> the reference image had no effect at all

(iris-generate "a blue ceramic cup on a wooden table" 128 128 4 42)
(iris-save "cmp-txt2img.png")

(iris-img2img "img2img-src.png" "a blue ceramic cup on a wooden table" 128 128 4 42)
(iris-save "cmp-img2img.png")

(display "OK") (newline)
