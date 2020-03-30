(TeX-add-style-hook
 "output_arq"
 (lambda ()
   (TeX-add-to-alist 'LaTeX-provided-class-options
                     '(("scrartcl" "paper=a4" "fontsize=11pt")))
   (TeX-add-to-alist 'LaTeX-provided-package-options
                     '(("fontenc" "T1") ("babel" "english") ("microtype" "protrusion=true" "expansion=true") ("graphicx" "pdftex") ("geometry" "margin=0.5in")))
   (TeX-run-style-hooks
    "latex2e"
    "scrartcl"
    "scrartcl10"
    "fontenc"
    "fourier"
    "babel"
    "microtype"
    "amsmath"
    "amsfonts"
    "amsthm"
    "graphicx"
    "url"
    "titlesec"
    "mathtools"
    "listings"
    "xcolor"
    "fancyhdr"
    "geometry")
   (TeX-add-symbols
    '("horrule" 1)))
 :latex)

