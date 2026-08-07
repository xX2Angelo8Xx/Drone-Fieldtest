# DigitMatrix

Offline Android prototype for recognizing handwritten digit matrices from photos. The app performs thresholding, grid-line suppression, connected-component segmentation, CNN digit classification, spatial row/column reconstruction, and text/CSV/JSON export. The bundled model is trained reproducibly from official MNIST during CI and quantized to int8 weights for mobile deployment.
